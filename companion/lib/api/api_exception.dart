import 'package:dio/dio.dart';

/// Typed exception hierarchy for API failures.
///
/// [ApiException] wraps transport, timeout, authentication and contract
/// failures so callers can respond with context instead of raw exceptions.
class ApiException implements Exception {
  const ApiException({
    required this.message,
    this.type = ApiExceptionType.unknown,
    this.statusCode,
    this.cause,
  });

  final String message;
  final ApiExceptionType type;
  final int? statusCode;
  final Object? cause;

  @override
  String toString() => 'ApiException($type): $message';

  /// Converts any thrown error into an [ApiException].
  static ApiException from(Object error) {
    if (error is ApiException) {
      return error;
    }
    if (error is DioException) {
      return fromDio(error);
    }
    return ApiException(message: error.toString(), cause: error);
  }

  static ApiException fromDio(DioException error) {
    switch (error.type) {
      case DioExceptionType.connectionTimeout:
      case DioExceptionType.sendTimeout:
      case DioExceptionType.receiveTimeout:
      case DioExceptionType.transformTimeout:
        return ApiException(
          message:
              'The device took too long to respond. '
              'Check the address and try again.',
          type: ApiExceptionType.timeout,
          cause: error,
        );
      case DioExceptionType.badCertificate:
      case DioExceptionType.connectionError:
        return ApiException(
          message:
              'Could not reach the AURA device. '
              'Make sure it is powered on and on the same network.',
          type: ApiExceptionType.connection,
          cause: error,
        );
      case DioExceptionType.badResponse:
        return ApiException(
          message: _messageForStatus(error.response?.statusCode),
          type: _typeForStatus(error.response?.statusCode),
          statusCode: error.response?.statusCode,
          cause: error,
        );
      case DioExceptionType.cancel:
        return ApiException(
          message: 'The request was cancelled.',
          type: ApiExceptionType.cancelled,
          cause: error,
        );
      case DioExceptionType.unknown:
        return ApiException(
          message: error.message ?? 'An unexpected network error occurred.',
          cause: error,
        );
    }
  }

  static String _messageForStatus(int? status) {
    switch (status) {
      case 401:
        return 'Authentication failed. Check your credentials.';
      case 404:
        return 'The requested resource is not available on this device.';
      case 429:
        return 'Too many requests. Wait a moment and try again.';
      default:
        return 'The device responded with an unexpected status '
            '(${status ?? 'unknown'}).';
    }
  }

  static ApiExceptionType _typeForStatus(int? status) {
    switch (status) {
      case 401:
      case 403:
        return ApiExceptionType.unauthorized;
      case 404:
        return ApiExceptionType.notFound;
      case 429:
        return ApiExceptionType.rateLimited;
      default:
        return ApiExceptionType.server;
    }
  }
}

/// Categories used to decide recovery behaviour.
enum ApiExceptionType {
  connection,
  timeout,
  unauthorized,
  notFound,
  rateLimited,
  server,
  cancelled,
  unknown,
}
