import 'dart:convert';
import 'dart:typed_data';

import '../core/constants/api_paths.dart';
import '../core/constants/app_constants.dart';
import '../models/device_control.dart';
import '../models/device_info.dart';
import '../models/performance_metrics.dart';
import '../models/system_status.dart';
import '../models/wifi_status.dart';
import 'api_client.dart';
import 'api_exception.dart';

/// Typed methods for the AURA firmware REST API.
class ApiService {
  ApiService(this._client);

  final ApiClient _client;

  /// Returns whether any API call can be made.
  bool get isConfigured => _client.isConfigured;

  /// Generic GET returning the decoded JSON payload.
  Future<Map<String, dynamic>> getJson(
    String path, {
    Map<String, dynamic>? query,
  }) async {
    await _guard();
    final response = await _client.request(path, query: query);
    return _decode(response);
  }

  /// Generic POST returning the decoded JSON payload.
  Future<Map<String, dynamic>> postJson(
    String path, {
    Object? body,
    Map<String, dynamic>? query,
  }) async {
    await _guard();
    final response = await _client.request(
      path,
      method: 'POST',
      body: body ?? const <String, String>{},
      query: query,
    );
    return _decode(response);
  }

  /// Uploads raw bytes with progress reporting.
  Future<Map<String, dynamic>> upload(
    String path,
    List<int> bytes,
    String filename, {
    String field = 'file',
    void Function(double progress)? onProgress,
  }) async {
    await _guard();
    final response = await _client.upload(
      path,
      bytes,
      filename,
      field: field,
      onProgress: onProgress,
    );
    return _decode(response);
  }

  /// Downloads raw bytes from [path].
  Future<Uint8List> download(String path) async {
    await _guard();
    return _client.download(path);
  }

  Future<void> _guard() {
    if (!_client.isConfigured) {
      throw const ApiException(
        message: 'No device configured. Add a device first.',
        type: ApiExceptionType.connection,
      );
    }
    return Future.value();
  }

  Map<String, dynamic> _decode(ResponseData response) {
    final data = response.data;
    if (data is Map<String, dynamic>) {
      return data;
    }
    if (data is String) {
      try {
        final decoded = jsonDecode(data);
        if (decoded is Map<String, dynamic>) {
          return decoded;
        }
      } on FormatException {
        throw const ApiException(
          message: 'The device returned an invalid payload.',
          type: ApiExceptionType.server,
        );
      }
    }
    throw const ApiException(
      message: 'The device returned an unexpected payload.',
      type: ApiExceptionType.server,
    );
  }

  /// Fetches the base system status. Requires an active session.
  Future<SystemStatus> fetchStatus() async {
    await _guard();
    final response = await _client.request(ApiPaths.status);
    return SystemStatus.fromJson(_decode(response));
  }

  /// Lightweight reachability check against the device.
  ///
  /// Uses the unauthenticated `/api/auth/status` endpoint so the device can
  /// be discovered without a session. Returns `true` when the device answers.
  Future<bool> fetchReachability() async {
    await _guard();
    final response = await _client.request(ApiPaths.authStatus);
    _decode(response);
    return true;
  }

  /// Fetches current WiFi information.
  Future<WifiStatus> fetchWifi() async {
    await _guard();
    final response = await _client.request(ApiPaths.wifi);
    return WifiStatus.fromJson(_decode(response));
  }

  /// Fetches device identity information.
  Future<DeviceInfo> fetchDeviceInfo() async {
    await _guard();
    final response = await _client.request(ApiPaths.settings);
    return DeviceInfo.fromJson(_decode(response));
  }

  /// Fetches device identity information at a specific host without changing
  /// the active configuration.
  Future<DeviceInfo> fetchDeviceInfoAt(String host) async {
    await _guard();
    final saved = _client.baseUrl;
    _client.updateBaseUrl('http://$host');
    try {
      final response = await _client.request(ApiPaths.settings);
      return DeviceInfo.fromJson(_decode(response));
    } finally {
      _client.updateBaseUrl(saved);
    }
  }

  /// Fetches performance metrics.
  Future<PerformanceMetrics> fetchPerformance() async {
    await _guard();
    final response = await _client.request(ApiPaths.performance);
    return PerformanceMetrics.fromJson(_decode(response));
  }

  /// Authenticates against the firmware and returns the session token.
  Future<String> login({
    required String username,
    required String password,
  }) async {
    await _guard();
    final response = await _client.request(
      ApiPaths.authLogin,
      method: 'POST',
      body: {'username': username, 'password': password},
    );
    final json = _decode(response);
    final token = json['token'] as String?;
    if (token == null || token.isEmpty) {
      throw const ApiException(
        message: 'Authentication failed. Check your credentials.',
        type: ApiExceptionType.unauthorized,
      );
    }
    return token;
  }

  /// Checks whether the current session is still valid with the device.
  ///
  /// Unauthenticated firmware endpoint; returns `false` when the device
  /// reports an expired or missing session.
  Future<bool> isSessionValid() async {
    await _guard();
    final response = await _client.request(ApiPaths.authStatus);
    final json = _decode(response);
    return json['authenticated'] == true;
  }

  /// Ends the current session server-side. Requires a valid token.
  Future<void> logout() async {
    await _guard();
    await _client.request(ApiPaths.authLogout, method: 'POST');
  }

  /// Fetches the firmware version block (mark, codename, version, channel).
  Future<DeviceInfo> fetchVersion() async {
    await _guard();
    final response = await _client.request(ApiPaths.version);
    return DeviceInfo.fromVersionJson(_decode(response));
  }

  /// Fetches the exported developer diagnostics text.
  Future<String> fetchDeveloperExport() async {
    await _guard();
    final response = await _client.request(ApiPaths.developerExport);
    return (_decode(response)['diagnostics'] as String?) ?? '';
  }

  /// Sends a chat message and returns the assistant reply.
  ///
  /// The request is deliberately structured so a streaming transport can
  /// replace this method later without touching the UI.
  Future<String> sendMessage(String message) async {
    await _guard();
    final response = await _client.request(
      ApiPaths.chat,
      method: 'POST',
      body: {'message': message},
    );
    final json = _decode(response);
    final reply =
        json['reply'] as String? ??
        json['response'] as String? ??
        json['message'] as String?;
    if (reply == null || reply.isEmpty) {
      throw const ApiException(
        message: 'The device did not return a reply.',
        type: ApiExceptionType.server,
      );
    }
    return reply;
  }

  // ---------------------------------------------------------------------------
  // V2 Companion Device Control
  // ---------------------------------------------------------------------------

  /// Scans for nearby Wi-Fi networks using the device radio.
  ///
  /// The firmware runs the scan asynchronously and returns a `state` field
  /// (`scanning` / `done` / `failed`). This method starts the scan and then
  /// polls the endpoint until the results are ready, giving the whole
  /// operation a longer budget than a single HTTP request would allow.
  Future<List<WifiNetwork>> scanWifi() async {
    await _guard();
    final deadline =
        DateTime.now()
            .add(const Duration(
              milliseconds: AppConstants.wifiScanMaxDurationMs,
            ));

    while (true) {
      final response = await _client.request(
        ApiPaths.wifiScan,
        method: 'POST',
        timeout: const Duration(
          milliseconds: AppConstants.wifiScanPollTimeoutMs,
        ),
      );
      final json = _decode(response);
      final state = json['state'] as String? ?? 'done';
      if (state == 'done') {
        final networks = (json['networks'] as List<dynamic>?) ?? const [];
        return networks
            .whereType<Map>()
            .map(
              (entry) => WifiNetwork.fromJson(
                Map<String, dynamic>.from(entry),
              ),
            )
            .toList();
      }
      if (state == 'failed') {
        throw const ApiException(
          message: 'The device could not scan for Wi-Fi networks.',
          type: ApiExceptionType.server,
        );
      }
      if (DateTime.now().isAfter(deadline)) {
        throw const ApiException(
          message: 'Wi-Fi scan timed out. Try again.',
          type: ApiExceptionType.timeout,
        );
      }
      await Future<void>.delayed(
        const Duration(milliseconds: AppConstants.wifiScanPollIntervalMs),
      );
    }
  }

  /// Connects the device to a Wi-Fi network (reuses the existing `/api/wifi`
  /// endpoint so credential handling stays in the firmware).
  Future<void> connectWifi({
    required String ssid,
    required String password,
  }) async {
    await _guard();
    await _client.request(
      ApiPaths.wifi,
      method: 'POST',
      body: {'ssid': ssid, 'password': password},
    );
  }

  /// Clears the saved Wi-Fi credentials on the device.
  Future<void> forgetWifi() async {
    await _guard();
    await _client.request(ApiPaths.wifiForget, method: 'POST');
  }

  /// Fetches the current OLED display state.
  Future<DisplayControlState> fetchDisplayControl() async {
    await _guard();
    final response = await _client.request(ApiPaths.displayControl);
    return DisplayControlState.fromJson(_decode(response));
  }

  /// Applies OLED display settings (`power`, `brightness`, `invert`,
  /// `rotation`, `timeout`, `text`).
  Future<void> setDisplayControl(Map<String, dynamic> body) async {
    await _guard();
    await _client.request(ApiPaths.displayControl, method: 'POST', body: body);
  }

  /// Fetches the current LED ring state.
  Future<LedControlState> fetchLedControl() async {
    await _guard();
    final response = await _client.request(ApiPaths.ledControl);
    return LedControlState.fromJson(_decode(response));
  }

  /// Applies LED ring settings (`enabled`, `brightness`, `mood`, `r/g/b`).
  Future<void> setLedControl(Map<String, dynamic> body) async {
    await _guard();
    await _client.request(ApiPaths.ledControl, method: 'POST', body: body);
  }

  /// Applies speaker settings (`volume`, `mute`, `output_speaker`, `test`).
  Future<void> setAudioControl(Map<String, dynamic> body) async {
    await _guard();
    await _client.request(ApiPaths.audioControl, method: 'POST', body: body);
  }

  /// Applies microphone settings (`gain`, `calibrate`).
  Future<void> setMicControl(Map<String, dynamic> body) async {
    await _guard();
    await _client.request(ApiPaths.micControl, method: 'POST', body: body);
  }

  /// Fetches a live microphone energy sample.
  Future<MicLiveLevel> fetchMicLevel() async {
    await _guard();
    final response = await _client.request(ApiPaths.micLevel);
    return MicLiveLevel.fromJson(_decode(response));
  }

  /// Fetches the raw uptime stats block (`/api/uptime`).
  Future<Map<String, dynamic>> fetchUptime() async {
    await _guard();
    final response = await _client.request(ApiPaths.uptime);
    return _decode(response);
  }

  /// Requests a device restart.
  Future<void> restartDevice() async {
    await _guard();
    await _client.request(ApiPaths.restart, method: 'POST');
  }

  /// Requests a factory reset (clears credentials and reboots).
  Future<void> factoryReset() async {
    await _guard();
    await _client.request(ApiPaths.factoryReset, method: 'POST');
  }
}

/// Shorthand for the JSON payload type returned by Dio.
typedef ResponseData = dynamic;
