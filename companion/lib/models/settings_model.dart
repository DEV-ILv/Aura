import 'package:flutter/material.dart';

import '../core/config/device_config.dart';
import '../core/constants/app_constants.dart';

/// Persisted application settings for the companion.
///
/// Authentication material ([authUsername], [authToken]) lives in secure
/// storage and is hydrated into this model at load time; it is kept here for
/// in-memory use only and is never written to plain preferences.
class SettingsModel {
  const SettingsModel({
    this.deviceHost = DeviceConfig.defaultHost,
    this.devicePort = DeviceConfig.defaultRestPort,
    this.webSocketPort = DeviceConfig.defaultWebSocketPort,
    this.authUsername = '',
    this.authToken = '',
    this.requestTimeoutMs = AppConstants.defaultTimeoutMs,
    this.autoReconnect = true,
    this.themeMode = ThemeMode.dark,
    this.ttsRate = 0.5,
    this.ttsPitch = 1.0,
    this.speechProvider = AppConstants.defaultSpeechProvider,
    this.ttsProvider = AppConstants.defaultTtsProvider,
    this.remindersEnabled = true,
    this.alertsEnabled = true,
    this.reconnectDelayMs = AppConstants.backoffBaseMs,
  });

  /// Device address (developer-only surface).
  final String deviceHost;
  final int devicePort;

  /// WebSocket port used for the live metrics feed (developer-only surface).
  final int webSocketPort;

  /// In-memory session credentials; persisted in secure storage.
  final String authUsername;
  final String authToken;

  final int requestTimeoutMs;
  final bool autoReconnect;
  final ThemeMode themeMode;

  /// Voice reply speech rate (0.5x - 2.0x).
  final double ttsRate;

  /// Voice reply speech pitch (0.5 - 2.0).
  final double ttsPitch;

  /// Preferred speech-to-text provider (informational; firmware decides).
  final String speechProvider;

  /// Preferred text-to-speech provider (informational; firmware decides).
  final String ttsProvider;

  /// Whether reminder notifications are enabled.
  final bool remindersEnabled;

  /// Whether immediate device alerts are enabled.
  final bool alertsEnabled;

  /// Base delay used by the reconnect backoff (milliseconds).
  final int reconnectDelayMs;

  /// Fully-qualified base URL for REST calls.
  String get baseUrl => DeviceConfig.restBaseUrl(deviceHost, devicePort);

  /// Fully-qualified WebSocket URL for the live metrics feed.
  String get webSocketUrl =>
      DeviceConfig.webSocketUrl(deviceHost, webSocketPort);

  /// Whether a device address has been configured at least once.
  bool get hasDevice => deviceHost.isNotEmpty;

  SettingsModel copyWith({
    String? deviceHost,
    int? devicePort,
    int? webSocketPort,
    String? authUsername,
    String? authToken,
    int? requestTimeoutMs,
    bool? autoReconnect,
    ThemeMode? themeMode,
    double? ttsRate,
    double? ttsPitch,
    String? speechProvider,
    String? ttsProvider,
    bool? remindersEnabled,
    bool? alertsEnabled,
    int? reconnectDelayMs,
  }) {
    return SettingsModel(
      deviceHost: deviceHost ?? this.deviceHost,
      devicePort: devicePort ?? this.devicePort,
      webSocketPort: webSocketPort ?? this.webSocketPort,
      authUsername: authUsername ?? this.authUsername,
      authToken: authToken ?? this.authToken,
      requestTimeoutMs: requestTimeoutMs ?? this.requestTimeoutMs,
      autoReconnect: autoReconnect ?? this.autoReconnect,
      themeMode: themeMode ?? this.themeMode,
      ttsRate: ttsRate ?? this.ttsRate,
      ttsPitch: ttsPitch ?? this.ttsPitch,
      speechProvider: speechProvider ?? this.speechProvider,
      ttsProvider: ttsProvider ?? this.ttsProvider,
      remindersEnabled: remindersEnabled ?? this.remindersEnabled,
      alertsEnabled: alertsEnabled ?? this.alertsEnabled,
      reconnectDelayMs: reconnectDelayMs ?? this.reconnectDelayMs,
    );
  }
}
