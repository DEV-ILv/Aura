import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import 'logger.dart';

/// Key names for values stored in the platform secure storage.
abstract final class SecureStorageKeys {
  static const String authToken = 'aura.auth.token';
  static const String refreshToken = 'aura.auth.refresh_token';
  static const String username = 'aura.auth.username';
}

/// Typed wrapper over [FlutterSecureStorage].
///
/// Only authentication material (session token, optional refresh token and
/// the username) is stored here. Passwords are never persisted. Everything
/// else (device address, theme, preferences) lives in [StorageService].
class SecureStorageService {
  SecureStorageService([FlutterSecureStorage? storage])
    : _storage = storage ?? const FlutterSecureStorage();

  final FlutterSecureStorage _storage;

  Future<String?> readToken() => _read(SecureStorageKeys.authToken);

  Future<String?> readRefreshToken() => _read(SecureStorageKeys.refreshToken);

  Future<String?> readUsername() => _read(SecureStorageKeys.username);

  Future<void> writeToken(String token) =>
      _write(SecureStorageKeys.authToken, token);

  Future<void> writeRefreshToken(String token) =>
      _write(SecureStorageKeys.refreshToken, token);

  Future<void> writeUsername(String username) =>
      _write(SecureStorageKeys.username, username);

  /// Stores the full authentication session in one call.
  Future<void> saveSession({
    required String username,
    required String token,
    String? refreshToken,
  }) async {
    await writeUsername(username);
    await writeToken(token);
    if (refreshToken != null && refreshToken.isNotEmpty) {
      await writeRefreshToken(refreshToken);
    }
  }

  /// Clears every authentication value.
  Future<void> clear() async {
    for (final key in [
      SecureStorageKeys.authToken,
      SecureStorageKeys.refreshToken,
      SecureStorageKeys.username,
    ]) {
      try {
        await _storage.delete(key: key);
      } catch (error) {
        Logger.warning('Secure storage delete failed for $key: $error');
      }
    }
  }

  Future<String?> _read(String key) async {
    try {
      return await _storage.read(key: key);
    } catch (error) {
      Logger.warning('Secure storage read failed for $key: $error');
      return null;
    }
  }

  Future<void> _write(String key, String value) async {
    try {
      await _storage.write(key: key, value: value);
    } catch (error) {
      Logger.warning('Secure storage write failed for $key: $error');
    }
  }
}
