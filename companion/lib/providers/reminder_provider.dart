import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../api/api_exception.dart';
import '../core/services/logger.dart';
import '../models/reminder.dart';
import '../repositories/reminder_repository.dart';
import 'app_providers.dart';

/// Reminder list state.
class ReminderState {
  const ReminderState({
    required this.reminders,
    required this.isLoading,
    this.error,
  });

  const ReminderState.initial()
    : reminders = const [],
      isLoading = true,
      error = null;

  final List<Reminder> reminders;
  final bool isLoading;
  final String? error;

  ReminderState copyWith({
    List<Reminder>? reminders,
    bool? isLoading,
    String? error,
  }) {
    return ReminderState(
      reminders: reminders ?? this.reminders,
      isLoading: isLoading ?? this.isLoading,
      error: error,
    );
  }
}

/// Owns reminder list, create/update/delete and local scheduling.
class ReminderNotifier extends StateNotifier<ReminderState> {
  ReminderNotifier(this._repository) : super(const ReminderState.initial());

  final ReminderRepository _repository;

  Future<void> load() async {
    state = state.copyWith(isLoading: true);
    try {
      final reminders = await _repository.fetchAll();
      state = ReminderState(reminders: reminders, isLoading: false);
    } on Exception catch (error) {
      Logger.warning('Reminder load failed: $error');
      state = const ReminderState(
        reminders: [],
        isLoading: false,
        error: 'Could not load reminders. Check the connection.',
      );
    }
  }

  Future<bool> add(Reminder reminder) async {
    try {
      final created = await _repository.create(reminder);
      final updated = [...state.reminders, created]
        ..sort(
          (a, b) => (a.triggerAt ?? b.createdAt).compareTo(
            b.triggerAt ?? a.createdAt,
          ),
        );
      state = ReminderState(reminders: updated, isLoading: false);
      return true;
    } on ApiException catch (error) {
      state = state.copyWith(error: error.message);
      return false;
    }
  }

  Future<bool> edit(Reminder reminder) async {
    try {
      final updated = await _repository.update(reminder);
      state = ReminderState(
        reminders: state.reminders
            .map((r) => r.id == updated.id ? updated : r)
            .toList(),
        isLoading: false,
      );
      return true;
    } on ApiException catch (error) {
      state = state.copyWith(error: error.message);
      return false;
    }
  }

  Future<void> remove(Reminder reminder) async {
    await _repository.delete(reminder);
    state = ReminderState(
      reminders: state.reminders.where((r) => r.id != reminder.id).toList(),
      isLoading: false,
    );
  }

  Future<void> clear() async {
    state = const ReminderState(reminders: [], isLoading: false);
  }
}

final reminderProvider = StateNotifierProvider<ReminderNotifier, ReminderState>(
  (ref) {
    return ReminderNotifier(ref.watch(reminderRepositoryProvider));
  },
);
