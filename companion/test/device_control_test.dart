import 'package:aura_companion/core/services/secure_storage_service.dart';
import 'package:aura_companion/core/services/storage_service.dart';
import 'package:aura_companion/providers/app_providers.dart';
import 'package:aura_companion/screens/device_control/device_control_screen.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';

void main() {
  testWidgets('Device Control page loads without exceptions', (tester) async {
    SharedPreferences.setMockInitialValues({});
    final prefs = await SharedPreferences.getInstance();

    await tester.pumpWidget(
      ProviderScope(
        overrides: [
          storageProvider.overrideWithValue(StorageService(prefs)),
          secureStorageProvider.overrideWithValue(SecureStorageService()),
        ],
        child: const MaterialApp(home: DeviceControlScreen()),
      ),
    );
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 100));

    // The page builds without throwing and shows its local-only banner when
    // there is no live local connection (test environment).
    expect(tester.takeException(), isNull);
    expect(find.text('Device Control'), findsOneWidget);
    expect(
      find.textContaining('Connect to the device on your local network'),
      findsOneWidget,
    );
  });
}
