import 'package:intl/intl.dart';

/// Direction of a [ChatMessage].
enum MessageRole { user, assistant, system }

/// A single message inside a conversation.
class ChatMessage {
  const ChatMessage({
    required this.id,
    required this.role,
    required this.content,
    required this.sentAt,
    this.streaming = false,
  });

  final String id;
  final MessageRole role;
  final String content;
  final DateTime sentAt;
  final bool streaming;

  /// Stable identifier for list keys.
  String get key => id;

  bool get isUser => role == MessageRole.user;

  bool get isAssistant => role == MessageRole.assistant;

  ChatMessage copyWith({String? content, bool? streaming}) {
    return ChatMessage(
      id: id,
      role: role,
      content: content ?? this.content,
      sentAt: sentAt,
      streaming: streaming ?? this.streaming,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'role': role.name,
      'content': content,
      'timestamp': sentAt.millisecondsSinceEpoch ~/ 1000,
    };
  }

  factory ChatMessage.fromJson(Map<String, dynamic> json) {
    final epoch = json['timestamp'] as num? ?? 0;
    final rawRole = json['role'] as String? ?? 'assistant';
    final role = MessageRole.values.firstWhere(
      (r) => r.name == rawRole,
      orElse: () => MessageRole.assistant,
    );
    final content = json['content'] as String? ?? '';
    return ChatMessage(
      id: '${epoch}_${content.hashCode}',
      role: role,
      content: content,
      sentAt: DateTime.fromMillisecondsSinceEpoch((epoch * 1000).toInt()),
    );
  }

  /// Renders a short timestamp suitable for chat bubbles.
  String get formattedTime => DateFormat('HH:mm').format(sentAt.toLocal());
}
