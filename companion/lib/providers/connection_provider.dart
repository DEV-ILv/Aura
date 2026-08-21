import 'dart:async';
import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../api/api_client.dart';
import '../api/api_exception.dart';
import '../core/config/device_config.dart';
import '../core/services/logger.dart';
import '../core/services/notification_service.dart';
import '../core/services/supabase_service.dart';
import '../models/settings_model.dart';
import '../repositories/device_repository.dart';
import '../websocket/websocket_service.dart';
import 'app_providers.dart';
import 'dashboard_provider.dart';
import 'settings_provider.dart';
import 'supabase_auth_provider.dart';

/// Phase of the connection lifecycle.
enum ConnectionPhase {
  idle,
  probing,
  unauthenticated,
  testing,
  connecting,
  connected,
  reconnecting,
  disconnected,
  unavailable,
  error,
  /// Waiting for device to transition from AP to station mode after provisioning.
  waitingForDevice,
}

/// How the app is currently talking to AURA.
enum ConnectionMode {
  /// Direct LAN connection to the device (REST on port 80, WebSocket on 81).
  local,

  /// Cloud connection through Supabase when the device is unreachable.
  remote,
}

/// Connection state exposed to the UI.
class ConnectionState {
  const ConnectionState({
    this.phase = ConnectionPhase.idle,
    this.message = '',
    this.host = '',
    this.mode = ConnectionMode.local,
  });

  const ConnectionState.initial()
    : phase = ConnectionPhase.idle,
      message = '',
      host = '',
      mode = ConnectionMode.local;

  final ConnectionPhase phase;
  final String message;
  final String host;
  final ConnectionMode mode;

  bool get isConnected => phase == ConnectionPhase.connected;

  bool get isBusy =>
      phase == ConnectionPhase.probing ||
      phase == ConnectionPhase.testing ||
      phase == ConnectionPhase.connecting ||
      phase == ConnectionPhase.reconnecting;

  ConnectionState copyWith({
    ConnectionPhase? phase,
    String? message,
    String? host,
    ConnectionMode? mode,
  }) {
    return ConnectionState(
      phase: phase ?? this.phase,
      message: message ?? this.message,
      host: host ?? this.host,
      mode: mode ?? this.mode,
    );
  }
}

/// Owns the device connection lifecycle including automatic reconnects and
/// the authentication flow (probe -> login -> token -> dashboard).
///
/// The connection is either [ConnectionMode.local] (direct LAN device access)
/// or [ConnectionMode.remote] (Supabase cloud). Local is always preferred;
/// the app automatically falls back to the cloud when the device is
/// unreachable and the user has a Supabase session.
class ConnectionNotifier extends StateNotifier<ConnectionState> {
  ConnectionNotifier(
    this._ref,
    this._repository,
    this._webSocket,
    this._settingsNotifier,
    this._notifications,
  ) : super(const ConnectionState.initial()) {
    _wsSubscription = _webSocket.state.listen(_onWebSocketState);
    _wsEventsSubscription = _webSocket.events.listen(_onWebSocketEvent);
    // Keep a direct handle to the client so dispose() never touches the
    // provider container after it has been torn down.
    final client = _ref.read(apiClientProvider);
    _apiClient = client;
    client.onUnauthorized = _handleSessionRejected;
  }

  final Ref _ref;
  final DeviceRepository _repository;
  final WebSocketService _webSocket;
  final SettingsNotifier _settingsNotifier;
  final NotificationService _notifications;
  ApiClient? _apiClient;

  Timer? _reconnectTimer;
  StreamSubscription<WebSocketState>? _wsSubscription;
  StreamSubscription<WebSocketEvent>? _wsEventsSubscription;
  int _reconnectAttempt = 0;
  bool _autoReconnect = true;
  String _candidateHost = '';
  int _candidatePort = 0;
  bool _wasConnected = false;
  bool _connecting = false;
  bool _disposed = false;
  String? _deviceMac; // Cache the device MAC for mDNS

  SupabaseService get _supabase => _ref.read(supabaseServiceProvider);

  /// Synchronises connection status with the live WebSocket feed.
  void _onWebSocketState(WebSocketState wsState) {
    if (wsState == WebSocketState.connected) {
      if (state.phase == ConnectionPhase.reconnecting ||
          state.phase == ConnectionPhase.connecting) {
        state = state.copyWith(
          phase: ConnectionPhase.connected,
          message: 'Connected to ${state.host}',
        );
      }
    } else if (wsState == WebSocketState.disconnected) {
      if (state.phase == ConnectionPhase.connected ||
          state.phase == ConnectionPhase.reconnecting) {
        _scheduleReconnect('Connection lost. Reconnecting…');
      }
    }
  }

  /// Handles the device rejecting the session token over the live feed.
  void _onWebSocketEvent(WebSocketEvent event) {
    if (event.type != 'unauthorized') {
      return;
    }
    _handleSessionRejected();
  }

  /// Clears the local session and returns to the login screen.
  ///
  /// Reached either from a rejected WebSocket handshake or from an HTTP 401
  /// on a non-credential endpoint, so an expired/invalidated token always
  /// routes back to sign-in instead of silently failing in the background.
  void _handleSessionRejected() {
    Logger.warning('Session rejected by the device; clearing local session');
    _ref.read(apiClientProvider).clearToken();
    _settingsNotifier.clearAuth();
    _webSocket.setEnabled(false);
    _reconnectTimer?.cancel();
    _reconnectAttempt = 0;
    if (!_disposed) {
      state = const ConnectionState(
        phase: ConnectionPhase.unauthenticated,
        message: 'Session expired. Sign in again.',
      );
    }
  }

  /// Applies the persisted configuration to the API client.
  void _applySettings(SettingsModel settings) {
    _autoReconnect = settings.autoReconnect;
    _ref.read(apiClientProvider).updateBaseUrl(settings.baseUrl);
    _ref.read(apiClientProvider).updateTimeout(settings.requestTimeoutMs);
    if (settings.authToken.isNotEmpty) {
      _ref.read(apiClientProvider).setToken(settings.authToken);
    }
    _webSocket.setEnabled(settings.autoReconnect);
    if (settings.autoReconnect) {
      _webSocket.configure(settings.webSocketUrl);
    }
  }

  /// Startup flow: probe the device, then restore the session or require login.
  Future<void> initialize() async {
    final settingsNotifier = _settingsNotifier;
    if (settingsNotifier.state.isLoading) {
      await settingsNotifier.load();
    }
    final settings = settingsNotifier.state.settings;
    _applySettings(settings);
    _wasConnected = false;
    _ref.read(supabaseAuthProvider.notifier).start();

    state = ConnectionState(
      phase: ConnectionPhase.probing,
      message: 'Contacting AURA device…',
      host: settings.deviceHost,
    );

    final resolvedHost = await _findReachableHost(settings.deviceHost);
    if (resolvedHost.isEmpty) {
      // Automatic local -> remote fallback when a cloud session exists.
      if (_supabase.isSignedIn) {
        _enterRemoteMode();
        return;
      }
      state = ConnectionState(
        phase: ConnectionPhase.unavailable,
        message: 'AURA device unavailable.',
        host: settings.deviceHost,
      );
      return;
    }

    if (resolvedHost != settings.deviceHost) {
      await _settingsNotifier.updateDevice(resolvedHost, settings.devicePort);
    }

    if (settings.authToken.isNotEmpty && await _validateStoredSession()) {
      await _connect(resolvedHost, settings.devicePort);
      return;
    }

    state = ConnectionState(
      phase: ConnectionPhase.unauthenticated,
      message: 'Sign in to continue.',
      host: resolvedHost,
    );
  }

  /// Switches the app to cloud mode, used when the local device is out of
  /// range. When a Supabase session exists the app goes straight online;
  /// otherwise the connection screen shows the cloud sign-in form.
  Future<bool> switchToCloud() async {
    if (!_supabase.isInitialized) {
      state = state.copyWith(
        phase: ConnectionPhase.unavailable,
        message: _supabase.initError.isEmpty
            ? 'Cloud service is unavailable right now.'
            : 'Cloud service unavailable.',
      );
      return false;
    }
    _enterRemoteMode();
    return state.mode == ConnectionMode.remote;
  }

  /// Switches back to direct LAN access (re-runs the local probe).
  Future<void> switchToLocal() => initialize();

  /// Signs into the cloud (Supabase) and enters remote mode.
  Future<bool> cloudLogin(String email, String password) async {
    try {
      await _supabase.signIn(email: email, password: password);
    } on Exception catch (error) {
      Logger.warning('Cloud sign-in failed: $error');
      state = const ConnectionState(
        phase: ConnectionPhase.unauthenticated,
        message: 'Sign-in failed. Check your email and password.',
        mode: ConnectionMode.remote,
      );
      return false;
    }
    _enterRemoteMode();
    return state.isConnected;
  }

  /// Creates a cloud account. When email confirmation is required by the
  /// Supabase project, an informative exception message is surfaced instead.
  Future<bool> cloudSignUp(String email, String password) async {
    try {
      await _supabase.signUp(email: email, password: password);
    } on Exception catch (error) {
      Logger.warning('Cloud sign-up failed: $error');
      state = ConnectionState(
        phase: ConnectionPhase.unauthenticated,
        message: _friendlyAuthError(error),
        mode: ConnectionMode.remote,
      );
      return false;
    }
    if (_supabase.isSignedIn) {
      _enterRemoteMode();
      return state.isConnected;
    }
    // Email confirmation is pending; keep the user on the sign-in form.
    state = const ConnectionState(
      phase: ConnectionPhase.unauthenticated,
      message: 'Account created — confirm your email, then sign in.',
      mode: ConnectionMode.remote,
    );
    return false;
  }

  /// Sends a password-reset email for [email].
  Future<bool> cloudForgotPassword(String email) async {
    try {
      await _supabase.sendPasswordReset(email: email);
      return true;
    } on Exception catch (error) {
      Logger.warning('Password reset failed: $error');
      return false;
    }
  }

  void _enterRemoteMode() {
    _reconnectTimer?.cancel();
    _reconnectAttempt = 0;
    _webSocket.setEnabled(false);
    if (_supabase.isSignedIn) {
      state = const ConnectionState(
        phase: ConnectionPhase.connected,
        message: 'Connected via AURA Cloud.',
        mode: ConnectionMode.remote,
      );
    } else {
      state = const ConnectionState(
        phase: ConnectionPhase.unauthenticated,
        message: 'Sign in to AURA Cloud to continue.',
        mode: ConnectionMode.remote,
      );
    }
  }

  String _friendlyAuthError(Object error) {
    final message = error.toString().toLowerCase();
    if (message.contains('already registered') ||
        message.contains('already been registered')) {
      return 'An account with this email already exists. Try signing in.';
    }
    if (message.contains('valid email')) {
      return 'Please enter a valid email address.';
    }
    if (message.contains('should be at least')) {
      return 'Your password must be at least 6 characters long.';
    }
    return 'Could not create the account. Please try again.';
  }

  /// Candidate addresses probed when the configured host cannot be reached.
  ///
  /// `DeviceConfig.defaultHost` is the AURA Setup access-point gateway, so a
  /// device whose LAN lease has changed (or that is currently in AP mode) can
  /// still be reached as long as the phone is on that network.
  static const List<String> _fallbackHosts = [DeviceConfig.defaultHost];

  static const String _unresolvedHost = '';

  /// Returns the first candidate address that answers a reachability probe,
  /// or an empty string when none do. The configured host is tried first so a
  /// stale stored address can never silently break the connection forever.
  Future<String> _findReachableHost(String preferredHost) async {
    final candidates = <String>[
      preferredHost,
      ..._fallbackHosts.where((host) => host != preferredHost),
    ];
    for (final host in candidates) {
      if (host.isEmpty) {
        continue;
      }
      try {
        final result = await _repository.testConnection(host);
        if (result.success) {
          Logger.info('Using device address $host');
          return host;
        }
      } catch (error) {
        Logger.warning('Reachability probe to $host failed: $error');
      }
    }
    return _unresolvedHost;
  }

  Future<bool> _validateStoredSession() async {
    try {
      if (await _repository.validateSession()) {
        return true;
      }
    } on ApiException catch (error) {
      Logger.warning('Session validation failed: ${error.message}');
    }
    _ref.read(apiClientProvider).clearToken();
    return false;
  }

  /// Authenticates and connects. Returns true when signed in.
  Future<bool> login(String username, String password) async {
    try {
      await _repository.authenticate(username: username, password: password);
    } on ApiException catch (error) {
      final rateLimited = error.statusCode == 429;
      state = ConnectionState(
        phase: ConnectionPhase.error,
        message: rateLimited
            ? 'Too many attempts. Try again later.'
            : 'Invalid username or password.',
        host: _settingsNotifier.state.settings.deviceHost,
      );
      return false;
    }

    final settings = _settingsNotifier.state.settings;
    await _settingsNotifier.updateAuth(
      username: username,
      token: _ref.read(apiClientProvider).token,
    );

    await _connect(settings.deviceHost, settings.devicePort);
    return state.isConnected;
  }

  /// Connects to [host], updating the stored configuration on success.
  Future<ConnectionTestResult> connect(String host, int port) async {
    return _connect(host, port);
  }

  Future<ConnectionTestResult> _connect(String host, int port) async {
    // Mutual exclusion: a probe is already in flight (reconnect timer,
    // manual reconnect or login). Returning a non-success result here is safe
    // because the active attempt will update the state.
    if (_connecting) {
      return const ConnectionTestResult(
        success: false,
        message: 'Connection attempt already in progress.',
      );
    }
    _connecting = true;
    _candidateHost = host;
    _candidatePort = port;
    try {
      state = ConnectionState(
        phase: ConnectionPhase.testing,
        message: 'Testing connection to $host…',
        host: host,
      );

      final result = await _repository.testConnection(host);
      if (!result.success) {
        if (!_disposed) {
          state = ConnectionState(
            phase: ConnectionPhase.error,
            message: result.message,
            host: host,
          );
        }
        return result;
      }

      state = ConnectionState(
        phase: ConnectionPhase.connecting,
        message: 'Connected to ${result.deviceName}. Fetching data…',
        host: host,
      );

      await _settingsNotifier.updateDevice(host, port);
      final settings = _settingsNotifier.state.settings;
      _applySettings(settings);

      await _ref.read(dashboardProvider.notifier).refresh();
      await _adoptReportedAddress(host);

      // Extract MAC from device info for mDNS
      try {
        final deviceInfo = await _repository.fetchDeviceInfoAt(host);
        _tryExtractDeviceMac(deviceInfo.name);
        // Update WebSocket URL to point to the connected host
        _webSocket.configure(DeviceConfig.webSocketUrl(host));
      } catch (_) {
        // If we can't fetch device info, still update WebSocket URL
        _webSocket.configure(DeviceConfig.webSocketUrl(host));
      }

      _reconnectAttempt = 0;
      _wasConnected = true;
      if (!_disposed) {
        state = ConnectionState(
          phase: ConnectionPhase.connected,
          message: 'Connected to ${result.deviceName}',
          host: _candidateHost,
        );
      }
      return result;
    } finally {
      _connecting = false;
    }
  }

  /// Handles the post-provisioning flow after Wi-Fi credentials are sent.
  ///
  /// The ESP32 enters a 5-second WAITING_TO_CONNECT state, then attempts to
  /// connect to the router. This method waits for the device to appear on the
  /// LAN (via mDNS or IP scan) and establishes the connection.
  Future<void> handlePostProvisioning() async {
    if (_disposed) return;

    state = ConnectionState(
      phase: ConnectionPhase.waitingForDevice,
      message: 'Waiting for AURA to join your Wi-Fi network…',
      host: _candidateHost,
      mode: ConnectionMode.local,
    );

    // Wait for the device to transition from AP to station mode.
    // The ESP32 waits 5 seconds, then attempts connection (~10-15s total).
    // We'll poll for the device at its new LAN address.
    for (int i = 0; i < 20; i++) {
      if (_disposed) return;

      await Future.delayed(const Duration(seconds: 3));

      // Try to discover the device at its new LAN address
      final resolved = await _discoverDevice();
      if (resolved.isNotEmpty) {
        // Found the device at its new LAN address!
        Logger.info('Post-provisioning: discovered device at $resolved');
        await _connect(resolved, _candidatePort);
        return;
      }

      Logger.info('Post-provisioning: waiting for device to join LAN (attempt ${i + 1}/20)');
    }

    // If we get here, the device didn't appear on the LAN in time
    Logger.warning('Post-provisioning: device did not appear on LAN in time');
    if (!_disposed) {
      state = ConnectionState(
        phase: ConnectionPhase.error,
        message: 'Device did not join Wi-Fi network. Please check credentials.',
        mode: ConnectionMode.local,
      );
    }
  }

  /// Explicit user-triggered reconnect.
  Future<void> reconnect() async {
    final settings = _settingsNotifier.state.settings;
    if (!settings.hasDevice) {
      return;
    }
    state = state.copyWith(
      phase: ConnectionPhase.reconnecting,
      message: 'Reconnecting…',
    );
    final resolvedHost = await _discoverDevice();
    if (resolvedHost.isEmpty) {
      state = ConnectionState(
        phase: ConnectionPhase.unavailable,
        message: 'AURA device unavailable.',
        host: settings.deviceHost,
      );
      return;
    }
    if (resolvedHost != settings.deviceHost) {
      await _settingsNotifier.updateDevice(resolvedHost, settings.devicePort);
    }
    await _connect(resolvedHost, settings.devicePort);
  }

  /// Re-runs the startup probe (used by the unavailable screen).
  Future<void> retry() => initialize();

  /// Refreshes transport after a Wi-Fi scan.
  ///
  /// A scan exercises the device radio; on the setup network the phone may
  /// briefly lose / regain AURA_Setup, and the device may have just joined a
  /// different network. This re-probes reachability (including the
  /// AURA_Setup gateway `192.168.4.1`) and recreates the WebSocket so the
  /// live feed re-authenticates against the device's current address.
  Future<void> refreshAfterScan() async {
    if (_disposed) {
      return;
    }
    try {
      // Use comprehensive discovery after a scan (mDNS -> cached LAN -> AP gateway)
      final resolved = await _discoverDevice();
      if (resolved.isEmpty) {
        Logger.warning('No reachable device found after scan');
        return;
      }
      final settings = _settingsNotifier.state.settings;
      if (resolved != settings.deviceHost) {
        await _settingsNotifier.updateDevice(resolved, settings.devicePort);
      }
      // updateDevice() only persists the new address; re-apply it so the REST
      // client base URL and the WebSocket target both point at the resolved
      // host before the feed is recreated. Without this the "fresh" channel
      // would reconnect to the stale host.
      _applySettings(_settingsNotifier.state.settings);
      // Recreate the live feed against the current address: dropping and
      // re-enabling forces a fresh connection + auth handshake even when the
      // URL is unchanged, so stale channels cannot linger after a network
      // interface change.
      _webSocket.setEnabled(false);
      if (_settingsNotifier.state.settings.autoReconnect) {
        _webSocket.setEnabled(true);
      }
    } catch (error) {
      Logger.debug('Post-scan refresh failed: $error');
    }
  }

  /// Signs out locally and on the device, returning to the login screen.
  ///
  /// Also signs out of the cloud session so the next launch starts clean.
  Future<void> logout() async {
    final mode = state.mode;
    try {
      await _repository.logout();
    } finally {
      await _settingsNotifier.clearAuth();
      _webSocket.setEnabled(false);
      _reconnectTimer?.cancel();
      _reconnectAttempt = 0;
      _wasConnected = false;
    }
    try {
      await _supabase.signOut();
    } catch (_) {
      Logger.warning('Cloud sign-out failed during logout.');
    }
    if (_disposed) {
      return;
    }
    state = ConnectionState(
      phase: ConnectionPhase.unauthenticated,
      message: mode == ConnectionMode.remote
          ? 'Signed out of AURA Cloud.'
          : 'Signed out.',
      mode: mode,
    );
  }

  /// Extracts the device MAC from a hostname like "aura-a1b2c3.local" or "aura-a1b2c3"
  void _tryExtractDeviceMac(String host) {
    if (_deviceMac != null) return;
    // Match patterns like "aura-a1b2c3.local", "aura-a1b2c3", or "a1b2c3"
    final match = RegExp(r'aura-([a-f0-9]{6,12})', caseSensitive: false).firstMatch(host);
    if (match != null) {
      _deviceMac = match.group(1)!.toLowerCase();
      Logger.info('Extracted device MAC: $_deviceMac');
    }
  }

  /// Discovers the device via mDNS (aura-<mac>.local) when the IP changes.
  Future<String?> _discoverViaMdns() async {
    if (_deviceMac == null) {
      return null;
    }
    final hostname = 'aura-$_deviceMac.local';
    try {
      final result = await InternetAddress.lookup(hostname, type: InternetAddressType.IPv4);
      if (result.isNotEmpty) {
        final ip = result.first.address;
        Logger.info('mDNS discovery found AURA at $ip');
        return ip;
      }
    } catch (e) {
      Logger.debug('mDNS discovery failed: $e');
    }
    return null;
  }

  /// Attempts to discover the device using multiple strategies in order:
  /// 1. mDNS (aura-<mac>.local) - most stable
  /// 2. Cached LAN address
  /// 2. AURA_Setup gateway (192.168.4.1) - for AP mode
  Future<String> _discoverDevice() async {
    // Try mDNS first if we have the MAC
    if (_deviceMac != null) {
      final mdnsIp = await _discoverViaMdns();
      if (mdnsIp != null) {
        return mdnsIp;
      }
    }

    // Try cached host
    final settings = _settingsNotifier.state.settings;
    if (settings.deviceHost.isNotEmpty && settings.deviceHost != DeviceConfig.defaultHost) {
      final result = await _repository.testConnection(settings.deviceHost);
      if (result.success) {
        return settings.deviceHost;
      }
    }

    // Try AURA_Setup gateway (for AP mode)
    try {
      final result = await _repository.testConnection(DeviceConfig.defaultHost);
      if (result.success) {
        return DeviceConfig.defaultHost;
      }
    } catch (_) {}

    return '';
  }

  /// Signs out of the cloud session only, leaving the local device session
  /// intact. Used by the Settings account tile; keeps the connection state
  /// in sync so the UI does not stay stuck in remote mode.
  Future<void> signOutCloud() async {
    try {
      await _supabase.signOut();
    } catch (_) {
      Logger.warning('Cloud sign-out failed.');
    }
    if (_disposed) {
      return;
    }
    if (state.mode == ConnectionMode.remote) {
      state = const ConnectionState(
        phase: ConnectionPhase.unauthenticated,
        message: 'Signed out of AURA Cloud.',
        mode: ConnectionMode.remote,
      );
    }
  }

  /// Authenticates and stores the session (used by the login flow).
  Future<void> authenticate(String username, String password) async {
    await _repository.authenticate(username: username, password: password);
    await _settingsNotifier.updateAuth(
      username: username,
      token: _ref.read(apiClientProvider).token,
    );
    state = state.copyWith(message: 'Authentication successful');
  }

  /// Discovers a device whose DHCP lease or network has changed by reading the
  /// address the firmware reports via `/api/wifi`, and adopts it in place of a
  /// stale stored host. Updates both the REST base URL and the WebSocket feed
  /// so subsequent traffic targets the device's current address.
  Future<void> _adoptReportedAddress(String currentHost) async {
    try {
      final wifi = await _repository.fetchWifi();
      // Only adopt a live LAN address; an AP-only device does not report one.
      if (!wifi.connected || wifi.ip.isEmpty || wifi.ip == currentHost) {
        return;
      }
      Logger.info(
        'Device reports address ${wifi.ip}; adopting it in place of '
        '$currentHost',
      );
      await _settingsNotifier.updateDevice(wifi.ip, _candidatePort);
      _candidateHost = wifi.ip;
      _applySettings(_settingsNotifier.state.settings);
      // Update WebSocket URL to point to the new address
      _webSocket.configure(DeviceConfig.webSocketUrl(wifi.ip));
    } catch (error) {
      Logger.debug('Could not refresh device address: $error');
    }
  }

  void _scheduleReconnect(String message) {
    if (_wasConnected && _settingsNotifier.state.settings.alertsEnabled) {
      _notifications.showDeviceAlert(
        id: NotificationService.deviceAlertId,
        title: 'AURA disconnected',
        body: 'The AURA device went offline. Reconnecting automatically…',
      );
    }
    if (!_autoReconnect) {
      state = state.copyWith(
        phase: ConnectionPhase.disconnected,
        message: message,
      );
      return;
    }
    final delay = _backoffDelay();

    // Automatic local -> remote fallback once local recovery keeps failing.
    if (_reconnectAttempt >= 3 && _supabase.isSignedIn) {
      _enterRemoteMode();
      return;
    }

    Logger.info('Reconnect scheduled in ${delay.inMilliseconds} ms');
    state = state.copyWith(
      phase: ConnectionPhase.reconnecting,
      message: '$message Retrying in ${delay.inMilliseconds ~/ 1000}s…',
    );
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(delay, () async {
      if (_disposed) {
        return;
      }
      if (_connecting) {
        _scheduleReconnect('Connection attempt already in progress. Retrying…');
        return;
      }
      // Re-resolve the reachable address instead of blindly reconnecting to
      // the last candidate: after a network interface change (e.g. the phone
      // rejoined AURA_Setup, or the device moved to a different network) the
      // stored host may be stale, and the fallback gateway 192.168.4.1 is
      // tried automatically.
      final settings = _settingsNotifier.state.settings;
      final resolvedHost = await _findReachableHost(
        settings.deviceHost.isNotEmpty ? settings.deviceHost : _candidateHost,
      );
      if (resolvedHost.isEmpty) {
        _scheduleReconnect('Device not reachable. Retrying…');
        return;
      }
      if (resolvedHost != settings.deviceHost) {
        await _settingsNotifier.updateDevice(resolvedHost, settings.devicePort);
      }
      try {
        await _connect(resolvedHost, settings.devicePort);
      } catch (error) {
        Logger.warning('Reconnect attempt failed: $error');
        _scheduleReconnect('Connection failed. Retrying…');
      }
    });
  }

  Duration _backoffDelay() {
    final base = _settingsNotifier.state.settings.reconnectDelayMs;
    final exp = base * (1 << _reconnectAttempt.clamp(0, 5));
    _reconnectAttempt++;
    return Duration(milliseconds: exp > 30000 ? 30000 : exp);
  }

  /// Updates the connection phase after a failed refresh.
  void markError(String message) {
    if (_wasConnected && _supabase.isSignedIn && _reconnectAttempt >= 2) {
      _enterRemoteMode();
      return;
    }
    state = state.copyWith(phase: ConnectionPhase.error, message: message);
  }

  @override
  void dispose() {
    _disposed = true;
    _reconnectTimer?.cancel();
    _wsSubscription?.cancel();
    _wsEventsSubscription?.cancel();
    _webSocket.setEnabled(false);
    _apiClient?.onUnauthorized = null;
    super.dispose();
  }
}

final connectionProvider =
    StateNotifierProvider<ConnectionNotifier, ConnectionState>((ref) {
      return ConnectionNotifier(
        ref,
        ref.watch(deviceRepositoryProvider),
        ref.watch(webSocketServiceProvider),
        ref.watch(settingsProvider.notifier),
        ref.watch(notificationServiceProvider),
      );
    });
