import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../providers/chat_provider.dart';
import '../../providers/voice_provider.dart';
import '../../widgets/chat_bubble.dart';

/// AI chat interface with markdown, code blocks, timestamps and typing
/// indicator.
class ChatScreen extends ConsumerStatefulWidget {
  const ChatScreen({super.key});

  @override
  ConsumerState<ChatScreen> createState() => _ChatScreenState();
}

class _ChatScreenState extends ConsumerState<ChatScreen>
    with AutomaticKeepAliveClientMixin {
  final _scrollController = ScrollController();
  final _composerController = TextEditingController();

  @override
  bool get wantKeepAlive => true;

  @override
  void dispose() {
    _scrollController.dispose();
    _composerController.dispose();
    super.dispose();
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!_scrollController.hasClients) {
        return;
      }
      _scrollController.animateTo(
        _scrollController.position.maxScrollExtent,
        duration: const Duration(milliseconds: 250),
        curve: Curves.easeOut,
      );
    });
  }

  void _send() {
    final text = _composerController.text;
    if (text.trim().isEmpty) {
      return;
    }
    _composerController.clear();
    ref.read(chatProvider.notifier).send(text);
    _scrollToBottom();
  }

  void _toggleMic() {
    final notifier = ref.read(voiceProvider.notifier);
    final listening = ref.read(voiceProvider).isListening;
    if (listening) {
      notifier.stopListening();
    } else {
      notifier.startListening();
    }
  }

  @override
  Widget build(BuildContext context) {
    super.build(context);
    final chat = ref.watch(chatProvider);
    final voice = ref.watch(voiceProvider);
    final messages = chat.messages;

    // Scroll to bottom when the message list grows.
    ref.listen(chatProvider.select((state) => state.messages.length), (
      previous,
      next,
    ) {
      if (next != previous) {
        _scrollToBottom();
      }
    });

    return Column(
      children: [
        Expanded(
          child: messages.isEmpty
              ? const _EmptyChat()
              : ListView.builder(
                  controller: _scrollController,
                  padding: const EdgeInsets.fromLTRB(8, 16, 8, 16),
                  itemCount: messages.length,
                  itemBuilder: (context, index) {
                    return KeyedSubtree(
                      key: ValueKey(messages[index].key),
                      child: ChatBubble(message: messages[index]),
                    );
                  },
                ),
        ),
        _ErrorBar(error: chat.error),
        _VoiceBar(voice: voice, onStop: _toggleMic),
        _Composer(
          controller: _composerController,
          isSending: chat.isSending,
          isListening: voice.isListening,
          onMic: _toggleMic,
          onSend: _send,
          onClear: () => ref.read(chatProvider.notifier).clearConversation(),
        ),
      ],
    );
  }
}

class _EmptyChat extends StatelessWidget {
  const _EmptyChat();

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Container(
            width: 72,
            height: 72,
            decoration: const BoxDecoration(
              color: AppColors.accentGlow,
              shape: BoxShape.circle,
            ),
            child: const Icon(
              Icons.auto_awesome,
              color: AppColors.primary,
              size: 34,
            ),
          ),
          const SizedBox(height: 20),
          Text('Talk to AURA', style: Theme.of(context).textTheme.titleLarge),
          const SizedBox(height: 8),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: 32),
            child: Text(
              'Ask a question, set a reminder, or manage your workspace. '
              'Messages render with full Markdown support.',
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ),
        ],
      ),
    );
  }
}

class _ErrorBar extends StatelessWidget {
  const _ErrorBar({this.error});

  final String? error;

  @override
  Widget build(BuildContext context) {
    if (error == null) {
      return const SizedBox.shrink();
    }
    return Container(
      width: double.infinity,
      color: AppColors.danger.withValues(alpha: 0.12),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Row(
        children: [
          const Icon(Icons.warning_amber, color: AppColors.danger, size: 18),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              error!,
              style: const TextStyle(color: AppColors.danger, fontSize: 12),
            ),
          ),
        ],
      ),
    );
  }
}

class _VoiceBar extends StatelessWidget {
  const _VoiceBar({required this.voice, required this.onStop});

  final VoiceState voice;
  final VoidCallback onStop;

  @override
  Widget build(BuildContext context) {
    if (!voice.isListening &&
        voice.transcript.isEmpty &&
        voice.phase != VoicePhase.processing) {
      return const SizedBox.shrink();
    }
    final colour = voice.isListening ? AppColors.danger : AppColors.primary;
    return Container(
      width: double.infinity,
      color: colour.withValues(alpha: 0.12),
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      child: Row(
        children: [
          Icon(
            voice.isListening ? Icons.mic : Icons.spatial_audio_off,
            color: colour,
            size: 18,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              voice.phase == VoicePhase.processing
                  ? 'Sending to AURA…'
                  : voice.transcript.isEmpty
                  ? 'Listening… speak now'
                  : voice.transcript,
              maxLines: 2,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(
                fontSize: 12,
                color: AppColors.textPrimary,
              ),
            ),
          ),
          IconButton(
            icon: const Icon(Icons.stop),
            color: colour,
            onPressed: onStop,
          ),
        ],
      ),
    );
  }
}

class _Composer extends StatelessWidget {
  const _Composer({
    required this.controller,
    required this.isSending,
    required this.isListening,
    required this.onMic,
    required this.onSend,
    required this.onClear,
  });

  final TextEditingController controller;
  final bool isSending;
  final bool isListening;
  final VoidCallback onMic;
  final VoidCallback onSend;
  final VoidCallback onClear;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.fromLTRB(12, 8, 12, 12),
      decoration: const BoxDecoration(
        color: AppColors.surface,
        border: Border(top: BorderSide(color: AppColors.surfaceBorder)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.end,
        children: [
          IconButton(
            tooltip: 'Clear conversation',
            onPressed: onClear,
            icon: const Icon(Icons.delete_outline),
          ),
          const SizedBox(width: 4),
          Expanded(
            child: TextField(
              controller: controller,
              minLines: 1,
              maxLines: 5,
              textInputAction: TextInputAction.send,
              onSubmitted: (_) => onSend(),
              decoration: const InputDecoration(
                hintText: 'Message AURA…',
                prefixIcon: Icon(Icons.chat_bubble_outline, size: 20),
              ),
            ),
          ),
          const SizedBox(width: 4),
          IconButton(
            tooltip: isListening ? 'Stop voice input' : 'Hold to talk',
            onPressed: onMic,
            icon: Icon(
              isListening ? Icons.mic : Icons.mic_none,
              color: isListening ? AppColors.danger : AppColors.primary,
            ),
          ),
          const SizedBox(width: 4),
          IconButton.filled(
            tooltip: 'Send',
            onPressed: isSending ? null : onSend,
            icon: Icon(
              isSending ? Icons.hourglass_top : Icons.send,
              color: isSending ? AppColors.textMuted : Colors.white,
            ),
          ),
        ],
      ),
    );
  }
}
