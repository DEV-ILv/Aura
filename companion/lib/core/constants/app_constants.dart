/// Application-wide constant values.
abstract final class AppConstants {
  /// Display name of the companion application.
  static const String appName = 'A.U.R.A';

  /// Companion application semantic version.
  static const String appVersion = '1.1.0';

  /// Companion application build number.
  static const String buildNumber = '2';

  /// Human readable description shown in the About section.
  static const String appDescription =
      'Official companion app for the AURA Personal AI Assistant.';

  /// Default host used when no device has been saved yet.
  static const String defaultHost = '192.168.4.1';

  /// Default REST port exposed by the AURA firmware web portal.
  static const int defaultPort = 80;

  /// Default WebSocket port exposed by the AURA firmware.
  static const int defaultWebSocketPort = 81;

  /// Default HTTP request timeout in milliseconds.
  static const int defaultTimeoutMs = 8000;

  /// Timeout for a single Wi-Fi scan poll request in milliseconds.
  ///
  /// The firmware runs the scan asynchronously and the app polls the scan
  /// endpoint, so each individual request only needs to outlast a single
  /// poll round-trip rather than the whole scan.
  static const int wifiScanPollTimeoutMs = 15000;

  /// Interval between Wi-Fi scan status polls in milliseconds.
  static const int wifiScanPollIntervalMs = 800;

  /// Maximum total time the app waits for a Wi-Fi scan to complete.
  static const int wifiScanMaxDurationMs = 40000;

  /// Default session validity reported by the firmware (seconds).
  static const int defaultSessionTimeoutSeconds = 3600;

  /// Maximum number of chat messages kept in memory before pruning.
  static const int maxChatHistory = 200;

  /// Interval in milliseconds used to poll metrics when the live
  /// WebSocket feed is unavailable.
  static const int metricsPollIntervalMs = 3000;

  /// Lower bound for generated reconnect delays.
  static const int backoffBaseMs = 1000;

  /// Upper bound for generated reconnect delays.
  static const int backoffMaxMs = 30000;

  /// Default speech-to-text provider (matches the firmware default).
  static const String defaultSpeechProvider = 'Google';

  /// Default text-to-speech provider (matches the firmware default).
  static const String defaultTtsProvider = 'Google';

  /// Supported speech-to-text providers.
  ///
  /// Informational only: the actual recogniser is chosen by the device
  /// firmware. 'Google' is the current default; 'Sarvam' is prepared for a
  /// future firmware update and has no effect yet.
  static const List<String> speechProviders = [
    'Google',
    'Sarvam',
    'Deepgram',
    'Local',
  ];

  /// Supported text-to-speech providers.
  ///
  /// Informational only: the actual synthesiser is chosen by the device
  /// firmware. 'Google' is the current default; 'Sarvam' is prepared for a
  /// future firmware update and has no effect yet.
  static const List<String> ttsProviders = [
    'Google',
    'Sarvam',
    'ElevenLabs',
    'Piper',
  ];
}
