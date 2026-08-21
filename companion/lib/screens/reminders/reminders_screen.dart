import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../core/utils/formatters.dart';
import '../../models/reminder.dart';
import '../../providers/reminder_provider.dart';
import '../../widgets/status_badge.dart';

/// Reminder management screen.
class RemindersScreen extends ConsumerStatefulWidget {
  const RemindersScreen({super.key});

  @override
  ConsumerState<RemindersScreen> createState() => _RemindersScreenState();
}

class _RemindersScreenState extends ConsumerState<RemindersScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      ref.read(reminderProvider.notifier).load();
    });
  }

  Future<void> _openEditor([Reminder? existing]) async {
    final notifier = ref.read(reminderProvider.notifier);
    final edited = await showModalBottomSheet<Reminder>(
      context: context,
      isScrollControlled: true,
      backgroundColor: AppColors.surface,
      builder: (_) => ReminderEditorSheet(reminder: existing),
    );
    if (edited == null) {
      return;
    }
    if (existing == null) {
      await notifier.add(edited);
    } else {
      await notifier.edit(edited);
    }
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(reminderProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Reminders')),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: () => _openEditor(),
        icon: const Icon(Icons.add),
        label: const Text('Add Reminder'),
      ),
      body: _body(state),
    );
  }

  Widget _body(ReminderState state) {
    if (state.isLoading) {
      return const Center(child: CircularProgressIndicator());
    }
    if (state.reminders.isEmpty) {
      return const _EmptyReminders();
    }
    return ListView.separated(
      padding: const EdgeInsets.all(16),
      itemCount: state.reminders.length,
      separatorBuilder: (_, _) => const SizedBox(height: 10),
      itemBuilder: (context, index) {
        final reminder = state.reminders[index];
        return _ReminderTile(
          reminder: reminder,
          onTap: () => _openEditor(reminder),
          onDelete: () => ref.read(reminderProvider.notifier).remove(reminder),
        );
      },
    );
  }
}

class _EmptyReminders extends StatelessWidget {
  const _EmptyReminders();

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(
            Icons.notifications_none,
            color: AppColors.textMuted,
            size: 48,
          ),
          const SizedBox(height: 16),
          Text(
            'No reminders yet',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 8),
          Text(
            'Tap the + button to create a reminder. Scheduled reminders '
            'will also fire as local notifications.',
            textAlign: TextAlign.center,
            style: Theme.of(context).textTheme.bodyMedium,
          ),
        ],
      ),
    );
  }
}

class _ReminderTile extends StatelessWidget {
  const _ReminderTile({
    required this.reminder,
    required this.onTap,
    required this.onDelete,
  });

  final Reminder reminder;
  final VoidCallback onTap;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: ListTile(
        onTap: onTap,
        leading: Icon(
          reminder.enabled
              ? Icons.notifications_active
              : Icons.notifications_off,
          color: reminder.enabled ? AppColors.primary : AppColors.textMuted,
        ),
        title: Text(
          reminder.text,
          maxLines: 2,
          overflow: TextOverflow.ellipsis,
        ),
        subtitle: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const SizedBox(height: 4),
            Row(
              children: [
                if (reminder.triggerAt != null)
                  Text(
                    Formatters.dateTime(reminder.triggerAt!),
                    style: const TextStyle(fontSize: 12),
                  )
                else
                  const Text('No schedule', style: TextStyle(fontSize: 12)),
                const SizedBox(width: 8),
                StatusBadge(
                  label: reminder.repeatLabel,
                  tone: BadgeTone.accent,
                ),
              ],
            ),
            if (reminder.category.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(
                  reminder.category,
                  style: const TextStyle(
                    fontSize: 11,
                    color: AppColors.textMuted,
                  ),
                ),
              ),
          ],
        ),
        trailing: IconButton(
          icon: const Icon(Icons.delete_outline),
          onPressed: onDelete,
        ),
      ),
    );
  }
}

/// Bottom sheet that creates or edits a reminder.
class ReminderEditorSheet extends StatefulWidget {
  const ReminderEditorSheet({super.key, this.reminder});

  final Reminder? reminder;

  @override
  State<ReminderEditorSheet> createState() => _ReminderEditorSheetState();
}

class _ReminderEditorSheetState extends State<ReminderEditorSheet> {
  late final TextEditingController _textController;
  late final TextEditingController _categoryController;
  RepeatRule _repeat = RepeatRule.none;
  bool _enabled = true;
  DateTime? _when;

  @override
  void initState() {
    super.initState();
    final r = widget.reminder;
    _textController = TextEditingController(text: r?.text ?? '');
    _categoryController = TextEditingController(text: r?.category ?? '');
    _repeat = r?.repeat ?? RepeatRule.none;
    _enabled = r?.enabled ?? true;
    _when = r?.triggerAt;
  }

  @override
  void dispose() {
    _textController.dispose();
    _categoryController.dispose();
    super.dispose();
  }

  Future<void> _pickDateTime() async {
    final now = DateTime.now();
    final date = await showDatePicker(
      context: context,
      initialDate: _when ?? now,
      firstDate: now.subtract(const Duration(days: 1)),
      lastDate: now.add(const Duration(days: 365 * 2)),
    );
    if (date == null) {
      return;
    }
    if (!mounted) {
      return;
    }
    final time = await showTimePicker(
      context: context,
      initialTime: TimeOfDay.fromDateTime(_when ?? now),
    );
    if (time == null || !mounted) {
      return;
    }
    setState(() {
      _when = DateTime(date.year, date.month, date.day, time.hour, time.minute);
    });
  }

  void _save() {
    final text = _textController.text.trim();
    if (text.isEmpty) {
      return;
    }
    final id =
        widget.reminder?.id ?? 'r${DateTime.now().millisecondsSinceEpoch}';
    Navigator.of(context).pop(
      Reminder(
        id: id,
        text: text,
        createdAt: widget.reminder?.createdAt ?? DateTime.now(),
        triggerAt: _when,
        repeat: _repeat,
        category: _categoryController.text.trim(),
        enabled: _enabled,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        left: 16,
        right: 16,
        top: 16,
        bottom: MediaQuery.viewInsetsOf(context).bottom + 16,
      ),
      child: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              widget.reminder == null ? 'New Reminder' : 'Edit Reminder',
              style: Theme.of(context).textTheme.titleLarge,
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _textController,
              decoration: const InputDecoration(
                labelText: 'Reminder text',
                prefixIcon: Icon(Icons.edit_outlined),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _categoryController,
              decoration: const InputDecoration(
                labelText: 'Category (optional)',
                prefixIcon: Icon(Icons.label_outline),
              ),
            ),
            const SizedBox(height: 12),
            DropdownButtonFormField<RepeatRule>(
              initialValue: _repeat,
              decoration: const InputDecoration(
                labelText: 'Repeat',
                prefixIcon: Icon(Icons.repeat),
              ),
              items: RepeatRule.values
                  .map((r) => DropdownMenuItem(value: r, child: Text(r.label)))
                  .toList(),
              onChanged: (value) =>
                  setState(() => _repeat = value ?? RepeatRule.none),
            ),
            const SizedBox(height: 12),
            SwitchListTile(
              contentPadding: EdgeInsets.zero,
              title: const Text('Enabled'),
              value: _enabled,
              onChanged: (value) => setState(() => _enabled = value),
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              onPressed: _pickDateTime,
              icon: const Icon(Icons.calendar_today_outlined),
              label: Text(
                _when == null
                    ? 'No schedule'
                    : 'Scheduled ${Formatters.dateTime(_when!)}',
              ),
            ),
            const SizedBox(height: 20),
            FilledButton.icon(
              onPressed: _save,
              icon: const Icon(Icons.check),
              label: const Text('Save Reminder'),
            ),
          ],
        ),
      ),
    );
  }
}

extension on RepeatRule {
  String get label {
    switch (this) {
      case RepeatRule.daily:
        return 'Daily';
      case RepeatRule.weekly:
        return 'Weekly';
      case RepeatRule.monthly:
        return 'Monthly';
      case RepeatRule.none:
        return 'Once';
    }
  }
}
