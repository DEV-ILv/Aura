import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:timezone/data/latest.dart' as tz;
import 'package:timezone/timezone.dart' as tz;

import 'logger.dart';

/// Wraps local notifications, scheduled reminders and device alerts.
///
/// All notification logic is isolated here so the rest of the app depends on
/// a simple interface. Safe to call on platforms without notification support.
class NotificationService {
  NotificationService() {
    _plugin = FlutterLocalNotificationsPlugin();
  }

  late final FlutterLocalNotificationsPlugin _plugin;
  bool _initialized = false;

  /// Whether immediate device alerts are allowed (synced from settings).
  bool alertsEnabled = true;

  /// Whether scheduled reminder notifications are allowed (synced from
  /// settings).
  bool remindersEnabled = true;

  static const _channel = AndroidNotificationDetails(
    'aura_alerts',
    'AURA Alerts',
    channelDescription: 'AURA device alerts',
    importance: Importance.high,
    priority: Priority.high,
  );

  /// Notification id used for generic device alerts.
  static const int deviceAlertId = 9001;

  /// Initialises the plugin. Must be awaited once at startup.
  Future<void> initialize() async {
    if (_initialized) {
      return;
    }
    try {
      tz.initializeTimeZones();
      const settings = InitializationSettings(
        android: AndroidInitializationSettings('@mipmap/ic_launcher'),
        iOS: DarwinInitializationSettings(),
      );
      await _plugin.initialize(settings: settings);
      _initialized = true;
    } catch (error) {
      Logger.warning('Notification initialization failed: $error');
    }
  }

  Future<void> _ensureReady() async {
    if (!_initialized) {
      await initialize();
    }
    try {
      await _plugin
          .resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin
          >()
          ?.requestNotificationsPermission();
    } catch (error) {
      Logger.warning('Notification permission request failed: $error');
    }
  }

  Future<void> _safe(Future<void> Function() action) async {
    try {
      await _ensureReady();
      await action();
    } catch (error) {
      Logger.warning('Notification action failed: $error');
    }
  }

  /// Shows an immediate device alert.
  Future<void> showDeviceAlert({
    required int id,
    required String title,
    required String body,
  }) {
    if (!alertsEnabled) {
      return Future.value();
    }
    return _safe(() {
      return _plugin.show(
        id: id,
        title: title,
        body: body,
        notificationDetails: const NotificationDetails(
          android: _channel,
          iOS: DarwinNotificationDetails(),
        ),
      );
    });
  }

  /// Schedules a local reminder notification at [when].
  Future<void> scheduleReminder({
    required int id,
    required String title,
    required String body,
    required DateTime when,
  }) {
    if (!remindersEnabled || when.isBefore(DateTime.now())) {
      return Future.value();
    }
    return _safe(() {
      return _plugin.zonedSchedule(
        id: id,
        title: title,
        body: body,
        scheduledDate: tz.TZDateTime.from(when, tz.local),
        notificationDetails: const NotificationDetails(android: _channel),
        androidScheduleMode: AndroidScheduleMode.inexactAllowWhileIdle,
      );
    });
  }

  /// Cancels a reminder/alert notification by id.
  Future<void> cancel(int id) async {
    if (!_initialized) {
      return;
    }
    await _plugin.cancel(id: id);
  }

  /// Cancels every scheduled/local notification.
  Future<void> cancelAll() async {
    if (!_initialized) {
      return;
    }
    await _plugin.cancelAll();
  }

  /// Helper to build a stable positive notification id from a string seed.
  static int stableId(String seed) => seed.hashCode & 0x7fffffff;
}
