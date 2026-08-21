import 'dart:typed_data';

import 'package:dio/dio.dart';

import '../core/constants/api_paths.dart';
import '../core/constants/app_constants.dart';
import '../core/constants/storage_keys.dart';
import '../core/services/logger.dart';
import '../core/services/secure_storage_service.dart';
import '../core/services/storage_service.dart';
import 'api_exception.dart';

/// Thin wrapper around a shared [Dio] client.
///
/// Owns transport configuration, request timeouts and the auth token used
/// for authenticated firmware endpoints. It is designed to be updated in
/// place when the user changes device or settings.
class ApiClient {
  ApiClient() {
    _dio = Dio(
      BaseOptions(
        connectTimeout: const Duration(
          milliseconds: AppConstants.defaultTimeoutMs,
        ),
        receiveTimeout: const Duration(
          milliseconds: AppConstants.defaultTimeoutMs,
        ),
        sendTimeout: const Duration(
          milliseconds: AppConstants.defaultTimeoutMs,
        ),
        headers: {'Accept': 'application/json'},
      ),
    );
    _dio.interceptors.add(
      InterceptorsWrapper(
        onRequest: _onRequest,
        onError: (error, handler) {
          Logger.warning(
            'ApiClient request failed: ${error.type} '
            '${error.response?.statusCode}',
          );
          // The firmware rejects a session with HTTP 401 once the token is
          // gone (expired, revoked by logout, or lost on reboot). Surface it
          // so the app returns to login instead of silently failing polls.
          // Login / change-password also return 401 for bad credentials and
          // must NOT be treated as an expired session.
          if (error.response?.statusCode == 401 &&
              error.requestOptions.path != ApiPaths.authLogin &&
              error.requestOptions.path != ApiPaths.authChangePassword) {
            onUnauthorized?.call();
          }
          handler.next(error);
        },
      ),
    );
  }

  /// Invoked when the firmware rejects the current session token (HTTP 401 on
  /// a non-credential endpoint). Wired by the connection layer to clear the
  /// local session and route back to the login screen.
  void Function()? onUnauthorized;

  late final Dio _dio;
  String _baseUrl = '';
  String _token = '';

  /// The current fully-qualified base URL or empty when not configured.
  String get baseUrl => _baseUrl;

  /// Whether the client has a configured base URL.
  bool get isConfigured => _baseUrl.isNotEmpty;

  /// Updates the base URL, rebuilding transport when the host changes.
  void updateBaseUrl(String baseUrl) {
    if (baseUrl == _baseUrl) {
      return;
    }
    _baseUrl = baseUrl;
    _dio.options.baseUrl = baseUrl;
    Logger.debug('ApiClient base URL set to $baseUrl');
  }

  /// Sets the session token attached to authenticated requests.
  void setToken(String token) {
    _token = token;
  }

  /// Applies a new request timeout to the shared transport.
  void updateTimeout(int timeoutMs) {
    final duration = Duration(milliseconds: timeoutMs);
    _dio.options.connectTimeout = duration;
    _dio.options.receiveTimeout = duration;
    _dio.options.sendTimeout = duration;
  }

  /// The currently stored session token (may be empty).
  String get token => _token;

  /// Clears the session token.
  void clearToken() {
    _token = '';
  }

  void _onRequest(
    RequestOptions options,
    RequestInterceptorHandler handler,
  ) async {
    if (_token.isNotEmpty) {
      options.headers['X-Auth-Token'] = _token;
    }
    handler.next(options);
  }

  /// Runs [request] against the current configuration.
  ///
  /// When [timeout] is provided it overrides the transport timeouts for this
  /// single request (used by operations that legitimately take longer than
  /// the default, e.g. Wi-Fi scanning or OTA handshakes).
  Future<Response<dynamic>> request(
    String path, {
    String method = 'GET',
    Object? body,
    Map<String, dynamic>? query,
    Duration? timeout,
  }) async {
    if (!isConfigured) {
      throw const ApiException(
        message: 'No device configured. Add a device first.',
        type: ApiExceptionType.connection,
      );
    }
    try {
      return await _dio.request<dynamic>(
        path,
        data: body,
        queryParameters: query,
        options: Options(
          method: method,
          connectTimeout: timeout,
          receiveTimeout: timeout,
          sendTimeout: timeout,
        ),
      );
    } on DioException catch (error) {
      throw ApiException.fromDio(error);
    }
  }

  /// Uploads [bytes] as [filename] to [path] with progress reporting.
  Future<Response<dynamic>> upload(
    String path,
    List<int> bytes,
    String filename, {
    String field = 'file',
    void Function(double progress)? onProgress,
  }) async {
    if (!isConfigured) {
      throw const ApiException(
        message: 'No device configured. Add a device first.',
        type: ApiExceptionType.connection,
      );
    }
    final form = FormData.fromMap({
      field: MultipartFile.fromBytes(bytes, filename: filename),
    });
    try {
      return await _dio.post<dynamic>(
        path,
        data: form,
        options: Options(
          contentType: 'multipart/form-data',
          sendTimeout: const Duration(minutes: 2),
          receiveTimeout: const Duration(minutes: 2),
        ),
        onSendProgress: (sent, total) {
          if (total > 0 && onProgress != null) {
            onProgress(sent / total);
          }
        },
      );
    } on DioException catch (error) {
      throw ApiException.fromDio(error);
    }
  }

  /// Downloads raw bytes from [path]. The caller owns decoding.
  Future<Uint8List> download(String path) async {
    if (!isConfigured) {
      throw const ApiException(
        message: 'No device configured. Add a device first.',
        type: ApiExceptionType.connection,
      );
    }
    try {
      final response = await _dio.get<dynamic>(
        path,
        options: Options(responseType: ResponseType.bytes),
      );
      final data = response.data;
      if (data is Uint8List) {
        return data;
      }
      if (data is List<int>) {
        return Uint8List.fromList(data);
      }
      throw const ApiException(
        message: 'Unexpected download payload.',
        type: ApiExceptionType.server,
      );
    } on DioException catch (error) {
      throw ApiException.fromDio(error);
    }
  }
}

/// Internal wiring that allows the app to hydrate the client from storage.
///
/// Lives outside the API layer to avoid importing persistence concerns into
/// transport code that many callers depend on.
abstract final class ApiBootstrap {
  static Future<void> applyStoredSettings(
    ApiClient client,
    StorageService storage,
    SecureStorageService secureStorage,
  ) async {
    final host = await storage.readString(StorageKeys.deviceHost);
    if (host != null && host.isNotEmpty) {
      final port = await storage.readInt(
        StorageKeys.devicePort,
        fallback: AppConstants.defaultPort,
      );
      client.updateBaseUrl('http://$host:$port');
    }
    final token = await secureStorage.readToken();
    if (token != null && token.isNotEmpty) {
      client.setToken(token);
    }
  }

  /// Standard unauthenticated handshake that verifies a device is reachable.
  ///
  /// `/api/auth/status` answers with 200 whether or not a session exists, so
  /// it is safe to probe without presenting a token.
  static Future<void> probeDevice(ApiClient client, String host) async {
    final saved = client.baseUrl;
    client.updateBaseUrl('http://$host:${AppConstants.defaultPort}');
    try {
      await client.request(ApiPaths.authStatus);
    } finally {
      client.updateBaseUrl(saved);
    }
  }
}
