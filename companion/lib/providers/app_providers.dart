import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../api/api_client.dart';
import '../api/api_service.dart';
import '../core/services/notification_service.dart';
import '../core/services/secure_storage_service.dart';
import '../core/services/speech_service.dart';
import '../core/services/storage_service.dart';
import '../core/services/supabase_service.dart';
import '../core/services/tts_service.dart';
import '../repositories/chat_repository.dart';
import '../repositories/cloud_repository.dart';
import '../repositories/device_repository.dart';
import '../repositories/memory_repository.dart';
import '../repositories/ota_repository.dart';
import '../repositories/reminder_repository.dart';
import '../repositories/sd_repository.dart';
import '../repositories/settings_repository.dart';
import '../websocket/websocket_service.dart';

/// Shared [StorageService] backed by the platform preferences.
final storageProvider = Provider<StorageService>((ref) {
  throw UnimplementedError('storageProvider must be overridden in main()');
});

/// Shared [SecureStorageService] for authentication material.
final secureStorageProvider = Provider<SecureStorageService>((ref) {
  throw UnimplementedError(
    'secureStorageProvider must be overridden in main()',
  );
});

/// Shared Supabase wrapper used for cloud (remote) access.
final supabaseServiceProvider = Provider<SupabaseService>((ref) {
  return SupabaseService.instance;
});

/// Remote data access via Supabase (RLS-scoped to the signed-in user).
final cloudRepositoryProvider = Provider<CloudRepository>((ref) {
  return CloudRepository(ref.watch(supabaseServiceProvider));
});

/// Persistence for application settings.
final settingsRepositoryProvider = Provider<SettingsRepository>((ref) {
  return SettingsRepository(
    ref.watch(storageProvider),
    ref.watch(secureStorageProvider),
  );
});

/// Single shared HTTP client used across the application.
final apiClientProvider = Provider<ApiClient>((ref) {
  return ApiClient();
});

/// High-level device operations.
final deviceRepositoryProvider = Provider<DeviceRepository>((ref) {
  return DeviceRepository(ref.watch(apiClientProvider));
});

/// Shared typed API facade used by feature repositories.
final apiServiceProvider = Provider<ApiService>((ref) {
  return ApiService(ref.watch(apiClientProvider));
});

/// Local and scheduled notifications.
final notificationServiceProvider = Provider<NotificationService>((ref) {
  return NotificationService();
});

/// Speech-to-text for voice chat.
final speechServiceProvider = Provider<SpeechService>((ref) {
  final service = SpeechService();
  ref.onDispose(service.dispose);
  return service;
});

/// Text-to-speech for voice replies.
final ttsServiceProvider = Provider<TtsService>((ref) {
  final service = TtsService();
  ref.onDispose(service.dispose);
  return service;
});

/// Reminder persistence + scheduling.
final reminderRepositoryProvider = Provider<ReminderRepository>((ref) {
  return ReminderRepository(
    ref.watch(apiServiceProvider),
    ref.watch(notificationServiceProvider),
  );
});

/// Memory store access.
final memoryRepositoryProvider = Provider<MemoryRepository>((ref) {
  return MemoryRepository(ref.watch(apiServiceProvider));
});

/// Firmware OTA operations.
final otaRepositoryProvider = Provider<OtaRepository>((ref) {
  return OtaRepository(ref.watch(apiServiceProvider));
});

/// SD card / storage operations.
final sdRepositoryProvider = Provider<SdRepository>((ref) {
  return SdRepository(ref.watch(apiServiceProvider));
});

/// Live metrics feed over WebSocket.
final webSocketServiceProvider = Provider<WebSocketService>((ref) {
  final service = WebSocketService(
    tokenProvider: () => ref.read(apiClientProvider).token,
  );
  ref.onDispose(service.dispose);
  return service;
});

/// Chat repository wired to the device send-message API.
final chatRepositoryProvider = Provider<ChatRepository>((ref) {
  final repository = ref.watch(deviceRepositoryProvider);
  return ChatRepository(repository.sendChat);
});

/// Prepares the [storageProvider] before the widget tree builds.
final prefsFutureProvider = FutureProvider<SharedPreferences>((ref) {
  return SharedPreferences.getInstance();
});

/// Async initialisation gate consumed by the splash screen.
///
/// Awaits the plain preference store so the session can be restored before
/// any screen builds. Settings are loaded by the connection initializer.
final bootstrapProvider = FutureProvider<void>((ref) async {
  await ref.watch(prefsFutureProvider.future);
});
