import 'package:flutter/material.dart';

import 'app_colors.dart';
import 'app_radius.dart';
import 'app_typography.dart';

/// Material 3 input (text field) theme, standardised for the whole app.
abstract final class AppInputTheme {
  static InputDecorationTheme data(ColorScheme scheme, {bool isDark = true}) {
    final fill = isDark ? AppColors.surfaceElevated : Colors.white;
    final border = isDark ? AppColors.surfaceBorder : Colors.black12;
    return InputDecorationTheme(
      filled: true,
      fillColor: fill,
      hintStyle: TextStyle(
        color: isDark ? AppColors.textMuted : AppColors.textSecondary,
        fontSize: AppTypography.sizeMd,
      ),
      labelStyle: TextStyle(
        color: isDark ? AppColors.textSecondary : AppColors.textMuted,
        fontSize: AppTypography.sizeMd,
      ),
      contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 15),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
        borderSide: BorderSide(color: border),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
        borderSide: BorderSide(color: border),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
        borderSide: const BorderSide(color: AppColors.primary, width: 1.6),
      ),
      errorBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
        borderSide: const BorderSide(color: AppColors.danger),
      ),
      focusedErrorBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(AppRadius.md),
        borderSide: const BorderSide(color: AppColors.danger, width: 1.4),
      ),
      prefixIconColor: AppColors.textMuted,
      suffixIconColor: AppColors.textMuted,
    );
  }
}
