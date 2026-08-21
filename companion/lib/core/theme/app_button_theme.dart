import 'package:flutter/material.dart';

import 'app_colors.dart';
import 'app_radius.dart';
import 'app_typography.dart';

/// Material 3 button themes, standardised for the whole app.
abstract final class AppButtonTheme {
  static ButtonStyle filled(ColorScheme scheme, {bool isDark = true}) {
    return FilledButton.styleFrom(
      backgroundColor: AppColors.primary,
      foregroundColor: Colors.white,
      disabledBackgroundColor: AppColors.surfaceBorder,
      disabledForegroundColor: AppColors.textMuted,
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
      ),
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
      textStyle: const TextStyle(
        fontWeight: AppTypography.semibold,
        fontSize: AppTypography.sizeLg,
      ),
      shadowColor: AppColors.primary,
    );
  }

  static ButtonStyle outlined(ColorScheme scheme, {bool isDark = true}) {
    return OutlinedButton.styleFrom(
      foregroundColor: AppColors.primary,
      side: BorderSide(color: AppColors.primary.withValues(alpha: 0.7)),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
      ),
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
      textStyle: const TextStyle(
        fontWeight: AppTypography.semibold,
        fontSize: AppTypography.sizeLg,
      ),
    );
  }

  static ButtonStyle text(ColorScheme scheme, {bool isDark = true}) {
    return TextButton.styleFrom(
      foregroundColor: AppColors.primary,
      textStyle: const TextStyle(
        fontWeight: AppTypography.semibold,
        fontSize: AppTypography.sizeMd,
      ),
    );
  }

  static ButtonStyle icon(ColorScheme scheme) {
    return IconButton.styleFrom(foregroundColor: AppColors.primary);
  }
}
