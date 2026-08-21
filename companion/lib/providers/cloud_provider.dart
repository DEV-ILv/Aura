import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'app_providers.dart';
import 'supabase_auth_provider.dart';

/// Device registry entry for the signed-in user (from Supabase `devices`).
class CloudDevice {
  const CloudDevice({
    required this.id,
    required this.deviceId,
    required this.name,
    this.model = '',
    this.firmwareVersion = '',
    this.mark = '',
    this.codename = '',
    this.isOnline = false,
    this.lastSeenAt,
  });

  final String id;
  final String deviceId;
  final String name;
  final String model;
  final String firmwareVersion;
  final String mark;
  final String codename;
  final bool isOnline;
  final DateTime? lastSeenAt;

  factory CloudDevice.fromJson(Map<String, dynamic> json) {
    return CloudDevice(
      id: (json['id'] as String?) ?? '',
      deviceId: (json['device_id'] as String?) ?? '',
      name: (json['name'] as String?) ?? 'AURA device',
      model: (json['model'] as String?) ?? '',
      firmwareVersion: (json['firmware_version'] as String?) ?? '',
      mark: (json['mark'] as String?) ?? '',
      codename: (json['codename'] as String?) ?? '',
      isOnline: (json['is_online'] as bool?) ?? false,
      lastSeenAt: json['last_seen_at'] != null
          ? DateTime.tryParse(json['last_seen_at'] as String)
          : null,
    );
  }

  String get versionLabel =>
      firmwareVersion.isEmpty ? '—' : 'v$firmwareVersion';
}

/// Loads the signed-in user's cloud-registered devices.
///
/// Re-evaluates when the Supabase auth state changes. Returns an empty list
/// when signed out or when the cloud is unreachable.
final cloudDevicesProvider = FutureProvider<List<CloudDevice>>((ref) async {
  ref.watch(supabaseAuthProvider);
  if (!ref.watch(supabaseServiceProvider).isSignedIn) {
    return const [];
  }
  try {
    final rows = await ref.watch(cloudRepositoryProvider).listDevices();
    return rows.map(CloudDevice.fromJson).toList();
  } catch (_) {
    return const [];
  }
});
