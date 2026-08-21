import 'package:supabase_flutter/supabase_flutter.dart';

import '../core/services/supabase_service.dart';

/// Remote (cloud) data access via Supabase.
///
/// Every query is scoped to the signed-in user by Row Level Security on the
/// server — the client never filters by user id manually. All tables carry a
/// `user_id` column that RLS compares against `auth.uid()`.
class CloudRepository {
  CloudRepository(this._service);

  final SupabaseService _service;

  SupabaseQueryBuilder _from(String table) {
    return _service.client!.from(table);
  }

  // ---------------------------------------------------------------------------
  // devices
  // ---------------------------------------------------------------------------

  /// Lists the devices registered to the signed-in user.
  Future<List<Map<String, dynamic>>> listDevices() async {
    final rows = await _from('devices').select().order('created_at');
    return _asList(rows);
  }

  /// Registers or updates a device owned by the user.
  Future<Map<String, dynamic>> upsertDevice({
    required String deviceId,
    required String name,
    String model = '',
    String firmwareVersion = '',
    String mark = '',
    String codename = '',
    String channel = '',
  }) async {
    final row = await _from('devices')
        .upsert({
          'device_id': deviceId,
          'name': name,
          'model': model,
          'firmware_version': firmwareVersion,
          'mark': mark,
          'codename': codename,
          'channel': channel,
        }, onConflict: 'user_id,device_id')
        .select()
        .single();
    return row;
  }

  /// Updates the device heartbeat so the user sees it as online.
  Future<void> touchDevice(String deviceId) async {
    await _from('devices')
        .update({
          'is_online': true,
          'last_seen_at': DateTime.now().toUtc().toIso8601String(),
        })
        .eq('device_id', deviceId);
  }

  /// Marks a device as offline.
  Future<void> markDeviceOffline(String deviceId) async {
    await _from(
      'devices',
    ).update({'is_online': false}).eq('device_id', deviceId);
  }

  /// Removes a device from the user's account.
  Future<void> deleteDevice(String deviceId) async {
    await _from('devices').delete().eq('device_id', deviceId);
  }

  // ---------------------------------------------------------------------------
  // commands
  // ---------------------------------------------------------------------------

  /// Records a command issued to a device.
  Future<void> logCommand({
    required String deviceId,
    required String command,
    Map<String, dynamic>? args,
    String status = 'sent',
    String? response,
  }) async {
    await _from('commands').insert({
      'device_id': deviceId,
      'command': command,
      'args': args,
      'status': status,
      'response': response,
    });
  }

  // ---------------------------------------------------------------------------
  // reminders
  // ---------------------------------------------------------------------------

  /// Lists the signed-in user's reminders.
  Future<List<Map<String, dynamic>>> fetchReminders() async {
    final rows = await _from('reminders').select().order('remind_at');
    return _asList(rows);
  }

  /// Creates a remote reminder.
  Future<Map<String, dynamic>> createReminder(
    Map<String, dynamic> values,
  ) async {
    final row = await _from('reminders').insert(values).select().single();
    return row;
  }

  Future<void> updateReminder(String id, Map<String, dynamic> values) async {
    await _from('reminders').update(values).eq('id', id);
  }

  Future<void> deleteReminder(String id) async {
    await _from('reminders').delete().eq('id', id);
  }

  // ---------------------------------------------------------------------------
  // memory
  // ---------------------------------------------------------------------------

  /// Lists the signed-in user's memories.
  Future<List<Map<String, dynamic>>> fetchMemories() async {
    final rows = await _from(
      'memory',
    ).select().order('created_at', ascending: false);
    return _asList(rows);
  }

  Future<void> insertMemory(Map<String, dynamic> values) async {
    await _from('memory').insert(values);
  }

  Future<void> deleteMemory(String id) async {
    await _from('memory').delete().eq('id', id);
  }

  // ---------------------------------------------------------------------------
  // notifications
  // ---------------------------------------------------------------------------

  /// Logs a notification delivered to the user.
  Future<void> logNotification({
    String? deviceId,
    required String title,
    required String body,
    String type = 'device',
  }) async {
    await _from('notifications').insert({
      'device_id': deviceId,
      'title': title,
      'body': body,
      'type': type,
    });
  }

  /// Lists the user's notifications, newest first.
  Future<List<Map<String, dynamic>>> fetchNotifications() async {
    final rows = await _from(
      'notifications',
    ).select().order('created_at', ascending: false);
    return _asList(rows);
  }

  // ---------------------------------------------------------------------------
  // settings
  // ---------------------------------------------------------------------------

  /// Stores a single settings key/value pair for the user.
  Future<void> pushSetting(String key, Object value) async {
    await _from(
      'settings',
    ).upsert({'key': key, 'value': value}, onConflict: 'user_id,key');
  }

  /// Reads the user's stored settings.
  Future<Map<String, dynamic>> fetchSettings() async {
    final rows = await _from('settings').select('key,value');
    final result = <String, dynamic>{};
    for (final row in _asList(rows)) {
      result[row['key'] as String] = row['value'];
    }
    return result;
  }

  List<Map<String, dynamic>> _asList(dynamic data) {
    if (data is List) {
      return data.whereType<Map<String, dynamic>>().toList();
    }
    return const [];
  }
}
