import 'package:flutter/material.dart';
import 'package:flutter_markdown/flutter_markdown.dart';

import '../core/theme/app_colors.dart';
import '../models/chat_message.dart';

/// Renders a single chat bubble with markdown support and timestamps.
class ChatBubble extends StatelessWidget {
  const ChatBubble({super.key, required this.message});

  final ChatMessage message;

  @override
  Widget build(BuildContext context) {
    final isUser = message.isUser;
    final bubbleColor = isUser ? AppColors.primary : AppColors.surfaceElevated;

    return Align(
      alignment: isUser ? Alignment.centerRight : Alignment.centerLeft,
      child: Container(
        constraints: const BoxConstraints(maxWidth: 720),
        margin: const EdgeInsets.symmetric(vertical: 4, horizontal: 8),
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
        decoration: BoxDecoration(
          color: bubbleColor,
          borderRadius: BorderRadius.only(
            topLeft: const Radius.circular(16),
            topRight: const Radius.circular(16),
            bottomLeft: Radius.circular(isUser ? 16 : 4),
            bottomRight: Radius.circular(isUser ? 4 : 16),
          ),
          border: isUser ? null : Border.all(color: AppColors.surfaceBorder),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            if (message.streaming)
              const _TypingIndicator()
            else
              MarkdownBody(
                data: message.content,
                styleSheet: _markdownStyle(isUser),
                selectable: true,
              ),
            const SizedBox(height: 4),
            Align(
              alignment: isUser ? Alignment.centerRight : Alignment.centerLeft,
              child: Text(
                message.formattedTime,
                style: TextStyle(
                  fontSize: 10,
                  color: isUser ? Colors.white70 : AppColors.textMuted,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  static MarkdownStyleSheet _markdownStyle(bool isUser) {
    final fg = isUser ? Colors.white : AppColors.textPrimary;
    return MarkdownStyleSheet(
      p: TextStyle(color: fg, fontSize: 14, height: 1.45),
      h1: TextStyle(color: fg, fontSize: 20, fontWeight: FontWeight.w700),
      h2: TextStyle(color: fg, fontSize: 18, fontWeight: FontWeight.w700),
      h3: TextStyle(color: fg, fontSize: 16, fontWeight: FontWeight.w700),
      h4: TextStyle(color: fg, fontSize: 15, fontWeight: FontWeight.w600),
      listBullet: TextStyle(color: fg),
      code: TextStyle(
        color: isUser ? Colors.white : AppColors.secondary,
        backgroundColor: Colors.transparent,
        fontFamily: 'monospace',
        fontSize: 13,
      ),
      codeblockPadding: const EdgeInsets.all(12),
      codeblockDecoration: BoxDecoration(
        color: Colors.black38,
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppColors.surfaceBorder),
      ),
      blockquoteDecoration: BoxDecoration(
        color: AppColors.accentGlow,
        borderRadius: BorderRadius.circular(6),
        border: const Border(
          left: BorderSide(color: AppColors.primary, width: 3),
        ),
      ),
      blockquote: const TextStyle(color: AppColors.textSecondary, fontSize: 13),
      horizontalRuleDecoration: const BoxDecoration(
        color: AppColors.surfaceBorder,
        border: Border(top: BorderSide(color: AppColors.surfaceBorder)),
      ),
    );
  }
}

/// Animated three-dot typing indicator for assistant streaming messages.
class _TypingIndicator extends StatefulWidget {
  const _TypingIndicator();

  @override
  State<_TypingIndicator> createState() => _TypingIndicatorState();
}

class _TypingIndicatorState extends State<_TypingIndicator>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 900),
    )..repeat();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _controller,
      builder: (context, child) {
        return Row(
          mainAxisSize: MainAxisSize.min,
          children: List.generate(3, (index) {
            final phase = (_controller.value + index * 0.2) % 1.0;
            final size = 6.0 + 4.0 * (phase < 0.5 ? phase : 1 - phase);
            return Container(
              width: size,
              height: size,
              margin: const EdgeInsets.only(right: 5),
              decoration: const BoxDecoration(
                color: AppColors.secondary,
                shape: BoxShape.circle,
              ),
            );
          }),
        );
      },
    );
  }
}
