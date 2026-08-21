import 'dart:async';
import 'dart:convert';

import 'package:web_socket_channel/web_socket_channel.dart';

import '../core/constants/app_constants.dart';
import '../core/services/logger.dart';

/// Connection state surfaced to the rest of the app.
enum WebSocketState { disconnected, connecting, connected }

/// Live metrics feed over WebSocket.
///
/// Connects to the firmware WebSocket endpoint, forwards JSON payloads and
/// reconnects automatically using exponential backoff when the link drops.
/// Emits typed [WebSocketEvent]s so consumers can react without knowing the
/// transport details.
class WebSocketService {
  WebSocketService({this.tokenProvider}) {
    _events = StreamController<WebSocketEvent>.broadcast();
    _state = StreamController<WebSocketState>.broadcast();
  }

  /// Supplies the current session token for the firmware auth handshake.
  final String Function()? tokenProvider;

  late final StreamController<WebSocketEvent> _events;
  late final StreamController<WebSocketState> _state;
  WebSocketChannel? _channel;
  StreamSubscription<dynamic>? _subscription;
  Timer? _pingTimer;
  Timer? _reconnectTimer;
  bool _manualClose = false;
  bool _enabled = true;
  int _attempt = 0;
  String _url = '';
  WebSocketState _lastState = WebSocketState.disconnected;

  /// Live JSON payloads (decoded) broadcast from the firmware.
  Stream<WebSocketEvent> get events => _events.stream;

  /// Connection state changes.
  Stream<WebSocketState> get state => _state.stream;

  /// Whether live monitoring is enabled.
  bool get isEnabled => _enabled;

  /// Reconfigures the target URL and restarts the connection when the
  /// address changed.
  void configure(String url) {
    if (url == _url) {
      return;
    }
    _url = url;
    _attempt = 0;
    if (_enabled) {
      _manualClose = false;
      _disposeConnection();
      _connect();
    }
  }

  /// Enables or disables live monitoring.
  void setEnabled(bool enabled) {
    if (_enabled == enabled) {
      return;
    }
    _enabled = enabled;
    if (enabled && _url.isNotEmpty) {
      _manualClose = false;
      _attempt = 0;
      _connect();
    } else {
      _disposeConnection();
    }
  }

  void _connect() {
    if (!_enabled || _url.isEmpty || _manualClose) {
      return;
    }
    if (_channel != null) {
      return;
    }
    _emitState(WebSocketState.connecting);
    Logger.info('WebSocket connecting to $_url');
    try {
      final channel = WebSocketChannel.connect(Uri.parse(_url));
      _channel = channel;
      // The `ready` future completes with an error when the connection is
      // refused/times out. It MUST be observed, otherwise the failure escapes
      // as an unhandled async exception in the zone.
      unawaited(
        channel.ready.then<void>((_) {}).catchError((Object error) {
          Logger.warning('WebSocket connect error: $error');
          _handleConnectFailure(channel);
        }),
      );
      _subscription = channel.stream.listen(
        _onData,
        onError: (Object error) {
          Logger.warning('WebSocket stream error: $error');
          _handleConnectFailure(channel);
        },
        onDone: _onDisconnect,
        cancelOnError: true,
      );
      _pingTimer?.cancel();
      _pingTimer = Timer.periodic(
        const Duration(seconds: 20),
        (_) => _sendPing(),
      );
    } catch (error) {
      Logger.warning('WebSocket connect failed: $error');
      _scheduleReconnect();
    }
  }

  /// Tears down a failed channel exactly once (the `ready` future and the
  /// stream can both surface the same failure) and schedules a reconnect.
  void _handleConnectFailure(WebSocketChannel channel) {
    if (!identical(_channel, channel)) {
      return;
    }
    _disposeConnection();
    _emitState(WebSocketState.disconnected);
    _scheduleReconnect();
  }

  void _onData(dynamic data) {
    if (_attempt > 0) {
      _attempt = 0;
      Logger.info('WebSocket reconnected');
    }
    if (_lastState != WebSocketState.connected) {
      _emitState(WebSocketState.connected);
    }
    if (data is String) {
      if (data == 'pong') {
        return;
      }
      try {
        final decoded = jsonDecode(data);
        if (decoded is Map<String, dynamic>) {
          final type = decoded['type'] as String? ?? 'message';
          if (type == 'auth_required') {
            _sendAuth();
            return;
          }
          if (type == 'error') {
            final message = decoded['message'] as String? ?? '';
            if (message.toLowerCase().contains('unauthorized')) {
              _handleUnauthorized();
              return;
            }
          }
          _events.add(WebSocketEvent.json(decoded));
        }
      } on FormatException {
        Logger.warning('WebSocket ignored non-JSON payload');
      }
    }
  }

  /// Responds to the firmware auth handshake with the current session token.
  void _sendAuth() {
    final token = tokenProvider?.call() ?? '';
    if (token.isEmpty) {
      _handleUnauthorized();
      return;
    }
    try {
      _channel?.sink.add(jsonEncode({'type': 'auth', 'token': token}));
    } catch (_) {
      _onDisconnect();
    }
  }

  /// The device rejected the session. Stops the reconnect loop so the app
  /// does not hammer an endpoint it cannot authenticate against.
  void _handleUnauthorized() {
    Logger.warning('WebSocket session rejected by the device');
    _manualClose = true;
    _disposeConnection();
    _events.add(WebSocketEvent.unauthorized());
    _emitState(WebSocketState.disconnected);
  }

  void _onDisconnect() {
    Logger.warning('WebSocket disconnected');
    _emitState(WebSocketState.disconnected);
    _disposeConnection();
    _scheduleReconnect();
  }

  void _sendPing() {
    try {
      _channel?.sink.add('ping');
    } catch (_) {
      _onDisconnect();
    }
  }

  void _scheduleReconnect() {
    if (!_enabled || _manualClose || _url.isEmpty) {
      return;
    }
    final delay = _nextBackoff();
    Logger.info('WebSocket reconnect scheduled in ${delay.inMilliseconds} ms');
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(delay, () {
      _channel = null;
      _connect();
    });
  }

  Duration _nextBackoff() {
    final base = AppConstants.backoffBaseMs * (1 << _attempt);
    final capped = base > AppConstants.backoffMaxMs
        ? AppConstants.backoffMaxMs
        : base;
    _attempt++;
    return Duration(milliseconds: capped);
  }

  void _disposeConnection() {
    _pingTimer?.cancel();
    _pingTimer = null;
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _subscription?.cancel();
    _subscription = null;
    try {
      _channel?.sink.close();
    } catch (_) {}
    _channel = null;
  }

  void _emitState(WebSocketState value) {
    if (_lastState == value) {
      return;
    }
    _lastState = value;
    if (!_state.isClosed) {
      _state.add(value);
    }
  }

  /// Gracefully closes the connection and stops reconnect scheduling.
  void dispose() {
    _manualClose = true;
    _disposeConnection();
    _events.close();
    _state.close();
  }
}

/// An event delivered to WebSocket consumers.
class WebSocketEvent {
  const WebSocketEvent._(this.type, this.payload);

  /// A JSON object payload.
  factory WebSocketEvent.json(Map<String, dynamic> payload) {
    return WebSocketEvent._(payload['type'] as String? ?? 'message', payload);
  }

  /// Emitted when the device rejects the current session token.
  factory WebSocketEvent.unauthorized() {
    return const WebSocketEvent._('unauthorized', <String, dynamic>{});
  }

  final String type;
  final Map<String, dynamic> payload;
}
