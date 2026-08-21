import '../api/api_client.dart';
import '../api/api_exception.dart';
import '../api/api_service.dart';
import '../core/services/logger.dart';
import '../models/dashboard_data.dart';
import '../models/device_info.dart';
import '../models/performance_metrics.dart';
import '../models/system_status.dart';
import '../models/wifi_status.dart';

/// Result of probing a candidate device address.
class ConnectionTestResult {
  const ConnectionTestResult({
    required this.success,
    required this.message,
    this.latencyMs = 0,
    this.deviceName = 'AURA',
    this.firmwareVersion = '',
  });

  final bool success;
  final String message;
  final int latencyMs;
  final String deviceName;
  final String firmwareVersion;
}

/// High-level operations over the AURA device.
///
/// Hides transport details from consumers and provides graceful failure
/// handling for every operation.
class DeviceRepository {
  DeviceRepository(ApiClient client)
    : _client = client,
      _api = ApiService(client);

  final ApiClient _client;
  final ApiService _api;

  /// Probes a candidate address without changing the active configuration.
  Future<ConnectionTestResult> testConnection(String host) async {
    final stopwatch = Stopwatch()..start();
    try {
      final reachable = await _probe(host);
      stopwatch.stop();
      String deviceName = 'AURA';
      String version = '';
      try {
        final device = await _fetchDeviceInfoAt(host);
        deviceName = device.name;
        version = device.version;
      } catch (_) {
        // Identity is optional; a reachable device is enough to connect.
      }
      return ConnectionTestResult(
        success: reachable,
        message: reachable
            ? 'Device is reachable and running.'
            : 'Device responded but is not reachable.',
        latencyMs: stopwatch.elapsedMilliseconds,
        deviceName: deviceName,
        firmwareVersion: version,
      );
    } on ApiException catch (error) {
      stopwatch.stop();
      Logger.warning('Connection test to $host failed: ${error.message}');
      return ConnectionTestResult(
        success: false,
        message: error.message,
        latencyMs: stopwatch.elapsedMilliseconds,
      );
    }
  }

  Future<bool> _probe(String host) async {
    final saved = _client.baseUrl;
    _client.updateBaseUrl('http://$host');
    try {
      return await _api.fetchReachability();
    } finally {
      _client.updateBaseUrl(saved);
    }
  }

  Future<DeviceInfo> _fetchDeviceInfoAt(String host) async {
    final saved = _client.baseUrl;
    _client.updateBaseUrl('http://$host');
    try {
      return await _api.fetchDeviceInfo();
    } finally {
      _client.updateBaseUrl(saved);
    }
  }

  /// Authenticates against the device and stores the session token.
  Future<void> authenticate({
    required String username,
    required String password,
  }) async {
    final token = await _api.login(username: username, password: password);
    _client.setToken(token);
  }

  /// Clears the current session token.
  void clearSession() {
    _client.clearToken();
  }

  /// Loads a full dashboard snapshot.
  Future<DashboardData> loadDashboard() async {
    final results = await Future.wait<dynamic>([
      _api.fetchStatus(),
      _api.fetchWifi(),
      _api.fetchPerformance(),
      _api.fetchDeviceInfo(),
      _api.fetchVersion(),
    ]);
    final deviceInfo = results[3] as DeviceInfo;
    final versionInfo = results[4] as DeviceInfo;
    final device = deviceInfo.copyWith(
      version: versionInfo.version,
      mark: versionInfo.mark,
      codename: versionInfo.codename,
      channel: versionInfo.channel,
      buildDate: versionInfo.buildDate,
    );
    return DashboardData(
      status: results[0] as SystemStatus,
      wifi: results[1] as WifiStatus,
      performance: results[2] as PerformanceMetrics,
      device: device,
    );
  }

  /// Fetches fresh performance metrics only.
  Future<PerformanceMetrics> fetchPerformance() => _api.fetchPerformance();

  /// Fetches the current WiFi state, including the address the device reports.
  Future<WifiStatus> fetchWifi() => _api.fetchWifi();

  /// Fetches device identity information at a specific host without changing
  /// the active configuration.
  Future<DeviceInfo> fetchDeviceInfoAt(String host) => _api.fetchDeviceInfoAt(host);

  /// Sends a chat message and returns the assistant reply.
  Future<String> sendChat(String message) => _api.sendMessage(message);

  /// Checks whether the stored session is still accepted by the device.
  Future<bool> validateSession() => _api.isSessionValid();

  /// Ends the session server-side and clears the local token.
  Future<void> logout() async {
    try {
      await _api.logout();
    } catch (_) {
      // The session may already be expired; local clearing still applies.
    }
    clearSession();
  }

  /// Exports the device diagnostics report as text.
  Future<String> fetchDeveloperExport() => _api.fetchDeveloperExport();

  /// Applies a performance payload arriving over the live feed.
  PerformanceMetrics mergeLiveMetrics(
    PerformanceMetrics current,
    Map<String, dynamic> json,
  ) {
    return current.copyWith(
      freeHeap: (json['free_heap'] as num?)?.toInt(),
      wifiRssi: (json['wifi_rssi'] as num?)?.toInt(),
      cpuUsage: (json['cpu_usage'] as num?)?.toDouble(),
      apiLatencyMs: (json['api_latency_ms'] as num?)?.toInt(),
      temperature: (json['temperature'] as num?)?.toDouble(),
      battery: (json['battery'] as num?)?.toDouble(),
      storageUsed: (json['storage_used'] as num?)?.toInt(),
      storageTotal: (json['storage_total'] as num?)?.toInt(),
      lastSyncEpoch:
          (json['last_sync'] as num?)?.toInt() ??
          DateTime.now().millisecondsSinceEpoch ~/ 1000,
    );
  }

  /// Best-effort ping used to refresh connectivity state.
  ///
  /// Uses the unauthenticated auth-status endpoint so a session is not
  /// required to determine whether the device is reachable.
  Future<int> ping() async {
    await _api.fetchReachability();
    return DateTime.now().millisecondsSinceEpoch;
  }
}
