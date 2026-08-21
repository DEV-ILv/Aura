import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/settings_model.dart';
import '../repositories/settings_repository.dart';
import 'app_providers.dart';

/// Settings state exposed to the UI.
class SettingsState {
  const SettingsState({required this.settings, required this.isLoading});

  const SettingsState.loading()
    : settings = const SettingsModel(),
      isLoading = true;

  final SettingsModel settings;
  final bool isLoading;

  SettingsState copyWith({SettingsModel? settings, bool? isLoading}) {
    return SettingsState(
      settings: settings ?? this.settings,
      isLoading: isLoading ?? this.isLoading,
    );
  }
}

/// Controller that owns loading, updating and persisting [SettingsModel].
class SettingsNotifier extends StateNotifier<SettingsState> {
  SettingsNotifier(this._repository) : super(const SettingsState.loading());

  final SettingsRepository _repository;

  /// Loads settings from disk.
  Future<void> load() async {
    final settings = await _repository.load();
    state = SettingsState(settings: settings, isLoading: false);
  }

  /// Persists a host change (developer-only surface).
  Future<void> updateDevice(String host, int port) async {
    final updated = state.settings.copyWith(deviceHost: host, devicePort: port);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  /// Persists the WebSocket port (developer-only surface).
  Future<void> updateWebSocketPort(int port) async {
    final updated = state.settings.copyWith(webSocketPort: port);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  /// Persists authentication details after a successful login.
  Future<void> updateAuth({
    required String username,
    required String token,
  }) async {
    await _repository.updateAuth(username: username, token: token);
    final updated = state.settings.copyWith(
      authUsername: username,
      authToken: token,
    );
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> clearAuth() async {
    await _repository.clearAuth();
    final updated = state.settings.copyWith(authUsername: '', authToken: '');
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateTimeout(int timeoutMs) async {
    final updated = state.settings.copyWith(requestTimeoutMs: timeoutMs);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateAutoReconnect(bool enabled) async {
    final updated = state.settings.copyWith(autoReconnect: enabled);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateThemeMode(ThemeMode mode) async {
    final updated = state.settings.copyWith(themeMode: mode);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateTtsRate(double rate) async {
    final updated = state.settings.copyWith(ttsRate: rate);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateTtsPitch(double pitch) async {
    final updated = state.settings.copyWith(ttsPitch: pitch);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  /// Persists the preferred speech-to-text provider.
  Future<void> updateSpeechProvider(String provider) async {
    final updated = state.settings.copyWith(speechProvider: provider);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  /// Persists the preferred text-to-speech provider.
  Future<void> updateTtsProvider(String provider) async {
    final updated = state.settings.copyWith(ttsProvider: provider);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateRemindersEnabled(bool enabled) async {
    final updated = state.settings.copyWith(remindersEnabled: enabled);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateAlertsEnabled(bool enabled) async {
    final updated = state.settings.copyWith(alertsEnabled: enabled);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }

  Future<void> updateReconnectDelay(int delayMs) async {
    final updated = state.settings.copyWith(reconnectDelayMs: delayMs);
    await _repository.save(updated);
    state = state.copyWith(settings: updated);
  }
}

final settingsProvider = StateNotifierProvider<SettingsNotifier, SettingsState>(
  (ref) {
    final notifier = SettingsNotifier(ref.watch(settingsRepositoryProvider));
    return notifier;
  },
);
