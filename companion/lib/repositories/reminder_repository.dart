import '../api/api_exception.dart';
import '../api/api_service.dart';
import '../core/constants/api_paths.dart';
import '../core/services/logger.dart';
import '../core/services/notification_service.dart';
import '../models/reminder.dart';

/// CRUD operations for device reminders with local notification scheduling.
class ReminderRepository {
  ReminderRepository(this._api, this._notifications);

  final ApiService _api;
  final NotificationService _notifications;

  /// Fetches all reminders. Falls back to an empty list on transport error
  /// so the UI can degrade gracefully.
  Future<List<Reminder>> fetchAll() async {
    try {
      final json = await _api.getJson(ApiPaths.reminders);
      final list = json['reminders'] as List? ?? json['entries'] as List? ?? [];
      return list
          .whereType<Map<String, dynamic>>()
          .map(Reminder.fromJson)
          .toList();
    } on ApiException catch (error) {
      Logger.warning('Failed to fetch reminders: ${error.message}');
      return const [];
    }
  }

  /// Creates a reminder and schedules its local notification.
  Future<Reminder> create(Reminder reminder) async {
    final json = await _api.postJson(
      ApiPaths.remindersCreate,
      body: reminder.toJson(),
    );
    final created = Reminder.fromJson(
      json['reminder'] as Map<String, dynamic>? ??
          json['data'] as Map<String, dynamic>? ??
          reminder.toJson(),
    );
    await _scheduleLocal(created);
    return created;
  }

  /// Updates an existing reminder (remote best-effort + local reschedule).
  Future<Reminder> update(Reminder reminder) async {
    final json = await _api.postJson(
      ApiPaths.remindersCreate,
      body: reminder.toJson(),
    );
    final updated = Reminder.fromJson(
      json['reminder'] as Map<String, dynamic>? ??
          json['data'] as Map<String, dynamic>? ??
          reminder.toJson(),
    );
    await _rescheduleLocal(updated);
    return updated;
  }

  /// Deletes a reminder.
  Future<void> delete(Reminder reminder) async {
    try {
      await _api.postJson(ApiPaths.remindersDelete, body: {'id': reminder.id});
    } on ApiException catch (error) {
      Logger.warning('Failed to delete reminder: ${error.message}');
    }
    await _notifications.cancel(NotificationService.stableId(reminder.id));
  }

  Future<void> _scheduleLocal(Reminder reminder) async {
    final when = reminder.triggerAt;
    if (when == null || !reminder.enabled) {
      return;
    }
    await _notifications.scheduleReminder(
      id: NotificationService.stableId(reminder.id),
      title: 'AURA Reminder',
      body: reminder.text,
      when: when,
    );
  }

  Future<void> _rescheduleLocal(Reminder reminder) async {
    await _notifications.cancel(NotificationService.stableId(reminder.id));
    await _scheduleLocal(reminder);
  }
}
