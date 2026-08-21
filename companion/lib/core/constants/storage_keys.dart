/// Shared key names used by the plain preference layer.
///
/// Authentication values are NOT stored here; they live in secure storage
/// (see [SecureStorageKeys]).
abstract final class StorageKeys {
  static const String deviceHost = 'aura.device.host';
  static const String devicePort = 'aura.device.port';
  static const String webSocketPort = 'aura.device.ws_port';

  /// Legacy keys from earlier builds that persisted credentials in plain
  /// preferences. Removed on load to complete the secure-storage migration.
  static const String legacyAuthUsername = 'aura.auth.username';
  static const String legacyAuthToken = 'aura.auth.token';

  static const String timeoutMs = 'aura.settings.timeout_ms';
  static const String autoReconnect = 'aura.settings.auto_reconnect';
  static const String themeMode = 'aura.settings.theme_mode';
  static const String ttsRate = 'aura.settings.tts_rate';
  static const String ttsPitch = 'aura.settings.tts_pitch';
  static const String speechProvider = 'aura.settings.speech_provider';
  static const String ttsProvider = 'aura.settings.tts_provider';
  static const String remindersEnabled = 'aura.settings.reminders_enabled';
  static const String alertsEnabled = 'aura.settings.alerts_enabled';
  static const String reconnectDelayMs = 'aura.settings.reconnect_delay_ms';
  static const String savedDevice = 'aura.device.saved';
}
