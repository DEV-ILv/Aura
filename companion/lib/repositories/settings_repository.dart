import 'package:flutter/material.dart';

import '../core/config/device_config.dart';
import '../core/constants/app_constants.dart';
import '../core/constants/storage_keys.dart';
import '../core/services/secure_storage_service.dart';
import '../core/services/storage_service.dart';
import '../models/settings_model.dart';

/// Persistence for [SettingsModel].
///
/// Non-sensitive values go to [StorageService] (plain preferences); session
/// credentials are read and written through [SecureStorageService]. The
/// repository also migrates legacy plaintext credentials on first load.
class SettingsRepository {
  SettingsRepository(this._storage, this._secureStorage);

  final StorageService _storage;
  final SecureStorageService _secureStorage;

  /// Loads the persisted settings, applying sensible defaults.
  Future<SettingsModel> load() async {
    final host =
        await _storage.readString(StorageKeys.deviceHost) ??
        AppConstants.defaultHost;
    final port = await _storage.readInt(
      StorageKeys.devicePort,
      fallback: AppConstants.defaultPort,
    );
    final wsPort = await _storage.readInt(
      StorageKeys.webSocketPort,
      fallback: DeviceConfig.defaultWebSocketPort,
    );
    final username = await _secureStorage.readUsername() ?? '';
    final token = await _secureStorage.readToken() ?? '';
    final timeout = await _storage.readInt(
      StorageKeys.timeoutMs,
      fallback: AppConstants.defaultTimeoutMs,
    );
    final autoReconnect = await _storage.readBool(
      StorageKeys.autoReconnect,
      fallback: true,
    );
    final themeRaw = await _storage.readString(StorageKeys.themeMode);
    final themeMode = ThemeMode.values.firstWhere(
      (mode) => mode.name == themeRaw,
      orElse: () => ThemeMode.dark,
    );
    final ttsRate = await _storage.readDouble(
      StorageKeys.ttsRate,
      fallback: 0.5,
    );
    final ttsPitch = await _storage.readDouble(
      StorageKeys.ttsPitch,
      fallback: 1.0,
    );
    final speechProvider =
        await _storage.readString(StorageKeys.speechProvider) ??
        AppConstants.defaultSpeechProvider;
    final ttsProvider =
        await _storage.readString(StorageKeys.ttsProvider) ??
        AppConstants.defaultTtsProvider;
    final remindersEnabled = await _storage.readBool(
      StorageKeys.remindersEnabled,
      fallback: true,
    );
    final alertsEnabled = await _storage.readBool(
      StorageKeys.alertsEnabled,
      fallback: true,
    );
    final reconnectDelayMs = await _storage.readInt(
      StorageKeys.reconnectDelayMs,
      fallback: AppConstants.backoffBaseMs,
    );

    await _purgeLegacyAuth();

    return SettingsModel(
      deviceHost: host,
      devicePort: port,
      webSocketPort: wsPort,
      authUsername: username,
      authToken: token,
      requestTimeoutMs: timeout,
      autoReconnect: autoReconnect,
      themeMode: themeMode,
      ttsRate: ttsRate,
      ttsPitch: ttsPitch,
      speechProvider: speechProvider,
      ttsProvider: ttsProvider,
      remindersEnabled: remindersEnabled,
      alertsEnabled: alertsEnabled,
      reconnectDelayMs: reconnectDelayMs,
    );
  }

  /// Persists the supplied settings.
  Future<void> save(SettingsModel settings) async {
    await _storage.writeString(StorageKeys.deviceHost, settings.deviceHost);
    await _storage.writeInt(StorageKeys.devicePort, settings.devicePort);
    await _storage.writeInt(StorageKeys.webSocketPort, settings.webSocketPort);
    await _storage.writeInt(StorageKeys.timeoutMs, settings.requestTimeoutMs);
    await _storage.writeBool(StorageKeys.autoReconnect, settings.autoReconnect);
    await _storage.writeString(StorageKeys.themeMode, settings.themeMode.name);
    await _storage.writeDouble(StorageKeys.ttsRate, settings.ttsRate);
    await _storage.writeDouble(StorageKeys.ttsPitch, settings.ttsPitch);
    await _storage.writeString(
      StorageKeys.speechProvider,
      settings.speechProvider,
    );
    await _storage.writeString(StorageKeys.ttsProvider, settings.ttsProvider);
    await _storage.writeBool(
      StorageKeys.remindersEnabled,
      settings.remindersEnabled,
    );
    await _storage.writeBool(StorageKeys.alertsEnabled, settings.alertsEnabled);
    await _storage.writeInt(
      StorageKeys.reconnectDelayMs,
      settings.reconnectDelayMs,
    );
    await _storage.writeBool(StorageKeys.savedDevice, true);
  }

  /// Persists authentication details to secure storage.
  Future<void> updateAuth({
    required String username,
    required String token,
  }) async {
    await _secureStorage.saveSession(username: username, token: token);
  }

  /// Clears persisted authentication from secure storage.
  Future<void> clearAuth() async {
    await _secureStorage.clear();
  }

  /// Whether a device has been configured before.
  Future<bool> hasSavedDevice() async {
    return _storage.readBool(StorageKeys.savedDevice);
  }

  /// Removes any credentials left behind by pre-secure-storage builds.
  Future<void> _purgeLegacyAuth() async {
    await _storage.remove(StorageKeys.legacyAuthUsername);
    await _storage.remove(StorageKeys.legacyAuthToken);
  }
}
