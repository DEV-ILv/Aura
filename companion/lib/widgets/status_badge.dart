import 'package:flutter/material.dart';

import '../core/theme/app_colors.dart';

/// Colour state for a [StatusBadge].
enum BadgeTone { success, warning, danger, neutral, accent }

/// Small labelled status indicator used across screens.
class StatusBadge extends StatelessWidget {
  const StatusBadge({
    super.key,
    required this.label,
    this.tone = BadgeTone.neutral,
    this.icon,
  });

  final String label;
  final BadgeTone tone;
  final IconData? icon;

  static Color _colorFor(BadgeTone tone) {
    switch (tone) {
      case BadgeTone.success:
        return AppColors.success;
      case BadgeTone.warning:
        return AppColors.warning;
      case BadgeTone.danger:
        return AppColors.danger;
      case BadgeTone.accent:
        return AppColors.primary;
      case BadgeTone.neutral:
        return AppColors.textMuted;
    }
  }

  @override
  Widget build(BuildContext context) {
    final color = _colorFor(tone);
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 6),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withValues(alpha: 0.4)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (icon != null) ...[
            Icon(icon, size: 13, color: color),
            const SizedBox(width: 6),
          ],
          Text(
            label,
            style: TextStyle(
              color: color,
              fontSize: 12,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }
}
