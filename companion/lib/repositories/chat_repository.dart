import 'dart:math';

import '../api/api_exception.dart';
import '../core/constants/app_constants.dart';
import '../core/services/logger.dart';
import '../models/chat_message.dart';

/// Manages the conversation state and persistence-friendly history.
class ChatRepository {
  ChatRepository(this._sendMessage);

  /// Function that performs the actual network call.
  final Future<String> Function(String message) _sendMessage;

  final List<ChatMessage> _history = <ChatMessage>[];
  static const int _idCounterMax = 1 << 30;

  /// Ordered conversation history.
  List<ChatMessage> get history => List.unmodifiable(_history);

  /// Number of messages currently retained.
  int get length => _history.length;

  /// Appends an incoming message (e.g. from a previous session) to history.
  void seed(List<ChatMessage> messages) {
    if (messages.isEmpty) {
      return;
    }
    _history
      ..clear()
      ..addAll(messages.take(AppConstants.maxChatHistory));
  }

  /// Sends [text] as the user and returns the assistant reply.
  ///
  /// The user message is appended immediately; the assistant message is
  /// created as streaming until the network call settles, which keeps the
  /// UI ready for streaming responses later.
  Future<String> send(String text) async {
    final userMessage = ChatMessage(
      id: _nextId(),
      role: MessageRole.user,
      content: text,
      sentAt: DateTime.now(),
    );
    _history.add(userMessage);
    _trim();

    final assistantMessage = ChatMessage(
      id: _nextId(),
      role: MessageRole.assistant,
      content: '',
      sentAt: DateTime.now(),
      streaming: true,
    );
    _history.add(assistantMessage);

    try {
      final reply = await _sendMessage(text);
      _replaceAssistant(
        assistantMessage.id,
        ChatMessage(
          id: assistantMessage.id,
          role: MessageRole.assistant,
          content: reply,
          sentAt: DateTime.now(),
        ),
      );
      return reply;
    } on ApiException catch (error) {
      Logger.warning('Chat send failed: ${error.message}');
      _replaceAssistant(
        assistantMessage.id,
        ChatMessage(
          id: assistantMessage.id,
          role: MessageRole.assistant,
          content: '⚠ ${error.message}',
          sentAt: DateTime.now(),
        ),
      );
      rethrow;
    }
  }

  void _replaceAssistant(String id, ChatMessage replacement) {
    final index = _history.indexWhere((message) => message.id == id);
    if (index == -1) {
      return;
    }
    _history[index] = replacement;
  }

  /// Removes the most recent empty assistant placeholder (used on cancel).
  void cancelPending() {
    if (_history.isNotEmpty && _history.last.streaming) {
      _history.removeLast();
    }
  }

  void clear() {
    _history.clear();
  }

  void _trim() {
    if (_history.length > AppConstants.maxChatHistory) {
      _history.removeRange(0, _history.length - AppConstants.maxChatHistory);
    }
  }

  int _seq = 0;

  String _nextId() {
    _seq = (_seq + 1) % _idCounterMax;
    return 'm${_seq}_${DateTime.now().microsecondsSinceEpoch}_'
        '${Random().nextInt(_idCounterMax)}';
  }
}
