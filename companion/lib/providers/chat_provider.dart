import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/chat_message.dart';
import '../repositories/chat_repository.dart';
import 'app_providers.dart';

/// Chat view state.
class ChatState {
  const ChatState({
    required this.messages,
    required this.isSending,
    this.error,
  });

  const ChatState.initial()
    : messages = const <ChatMessage>[],
      isSending = false,
      error = null;

  final List<ChatMessage> messages;
  final bool isSending;
  final String? error;

  ChatState copyWith({
    List<ChatMessage>? messages,
    bool? isSending,
    bool clearError = false,
    String? error,
  }) {
    return ChatState(
      messages: messages ?? this.messages,
      isSending: isSending ?? this.isSending,
      error: clearError ? null : (error ?? this.error),
    );
  }
}

/// Owns the conversation state, sending messages and history.
class ChatNotifier extends StateNotifier<ChatState> {
  ChatNotifier(this._repository) : super(const ChatState.initial());

  final ChatRepository _repository;

  /// Sends a message from the composer.
  Future<void> send(String text) async {
    final trimmed = text.trim();
    if (trimmed.isEmpty || state.isSending) {
      return;
    }
    state = state.copyWith(isSending: true, clearError: true);
    try {
      await _repository.send(trimmed);
      state = state.copyWith(messages: _repository.history, isSending: false);
    } catch (error) {
      state = state.copyWith(
        messages: _repository.history,
        isSending: false,
        error: 'Could not reach the assistant. Check the connection.',
      );
    }
  }

  /// Clears the current conversation.
  void clearConversation() {
    _repository.clear();
    state = ChatState(messages: _repository.history, isSending: false);
  }

  /// Cancels an in-flight assistant placeholder.
  void cancel() {
    _repository.cancelPending();
    state = state.copyWith(messages: _repository.history, isSending: false);
  }
}

final chatProvider = StateNotifierProvider<ChatNotifier, ChatState>((ref) {
  return ChatNotifier(ref.watch(chatRepositoryProvider));
});
