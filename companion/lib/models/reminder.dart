/// Recurrence rule for a reminder.
enum RepeatRule { none, daily, weekly, monthly }

/// A scheduled reminder.
class Reminder {
  const Reminder({
    required this.id,
    required this.text,
    required this.createdAt,
    this.triggerAt,
    this.repeat = RepeatRule.none,
    this.category = '',
    this.enabled = true,
  });

  final String id;
  final String text;
  final DateTime createdAt;

  /// When the reminder should fire. Null indicates a non-scheduled note.
  final DateTime? triggerAt;
  final RepeatRule repeat;
  final String category;
  final bool enabled;

  /// Human readable repeat label.
  String get repeatLabel {
    switch (repeat) {
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

  Reminder copyWith({
    String? text,
    DateTime? triggerAt,
    RepeatRule? repeat,
    String? category,
    bool? enabled,
  }) {
    return Reminder(
      id: id,
      text: text ?? this.text,
      createdAt: createdAt,
      triggerAt: triggerAt ?? this.triggerAt,
      repeat: repeat ?? this.repeat,
      category: category ?? this.category,
      enabled: enabled ?? this.enabled,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'text': text,
      'trigger_at': (triggerAt?.millisecondsSinceEpoch ?? 0) ~/ 1000,
      'repeat': repeat.name,
      'category': category,
      'enabled': enabled,
    };
  }

  factory Reminder.fromJson(Map<String, dynamic> json) {
    final epoch = json['trigger_at'] as num?;
    return Reminder(
      id: json['id'] as String? ?? '',
      text: json['text'] as String? ?? '',
      createdAt: DateTime.now(),
      triggerAt: epoch != null && epoch > 0
          ? DateTime.fromMillisecondsSinceEpoch((epoch * 1000).toInt())
          : null,
      repeat: RepeatRule.values.firstWhere(
        (r) => r.name == json['repeat'],
        orElse: () => RepeatRule.none,
      ),
      category: json['category'] as String? ?? '',
      enabled: json['enabled'] as bool? ?? true,
    );
  }
}
