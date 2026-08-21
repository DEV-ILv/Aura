import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:shared_preferences/shared_preferences.dart';

import 'app.dart';
import 'core/services/secure_storage_service.dart';
import 'core/services/storage_service.dart';
import 'core/services/supabase_service.dart';
import 'providers/app_providers.dart';

/// Application entry point.
Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  final preferences = await SharedPreferences.getInstance();
  final storage = StorageService(preferences);
  final secureStorage = SecureStorageService();

  // Cloud (remote) access. Failure is non-fatal: local device mode keeps
  // working, and remote mode reports a clear "cloud unavailable" state.
  await SupabaseService.instance.init();

  runApp(
    ProviderScope(
      overrides: [
        storageProvider.overrideWithValue(storage),
        secureStorageProvider.overrideWithValue(secureStorage),
      ],
      child: const AuraApp(),
    ),
  );
}
