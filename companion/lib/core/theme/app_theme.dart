import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

import 'app_button_theme.dart';
import 'app_card_theme.dart';
import 'app_colors.dart';
import 'app_input_theme.dart';
import 'app_radius.dart';
import 'app_shadows.dart';
import 'app_spacing.dart';
import 'app_typography.dart';

/// Builds the Material 3 dark-first theme from the centralized AURA tokens.
///
/// Every surface, button, input and card derives from [AppColors],
/// [AppTypography], [AppRadius], [AppSpacing] and the theme builders in
/// `app_*_theme.dart`. No hardcoded colors or sizes live here.
abstract final class AppTheme {
  static ThemeData dark() => _base(ThemeMode.dark);

  static ThemeData light() => _base(ThemeMode.light);

  static ThemeData _base(ThemeMode mode) {
    final brightness = mode == ThemeMode.dark
        ? Brightness.dark
        : Brightness.light;
    final isDark = brightness == Brightness.dark;

    final scheme = ColorScheme.fromSeed(
      seedColor: AppColors.primary,
      brightness: brightness,
      primary: AppColors.primary,
      secondary: AppColors.secondary,
      tertiary: AppColors.tertiary,
      surface: isDark ? AppColors.surface : Colors.white,
    );

    return ThemeData(
      useMaterial3: true,
      colorScheme: scheme,
      scaffoldBackgroundColor: isDark ? AppColors.background : Colors.white,
      fontFamily: AppTypography.body,
      textTheme: _textTheme(isDark),
      appBarTheme: AppBarTheme(
        backgroundColor: Colors.transparent,
        elevation: 0,
        centerTitle: false,
        foregroundColor: isDark ? AppColors.textPrimary : AppColors.primary,
        titleTextStyle: TextStyle(
          color: isDark ? AppColors.textPrimary : AppColors.primary,
          fontSize: AppTypography.sizeXl,
          fontWeight: AppTypography.semibold,
          letterSpacing: AppTypography.sectionLabel.letterSpacing,
        ),
      ),
      cardTheme: AppCardTheme.data(isDark: isDark),
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: isDark ? AppColors.surface : Colors.white,
        indicatorColor: AppColors.accentGlow,
        height: 68,
        labelTextStyle: WidgetStatePropertyAll(
          TextStyle(
            fontSize: AppTypography.sizeSm,
            fontWeight: AppTypography.medium,
            color: isDark ? AppColors.textSecondary : AppColors.textMuted,
          ),
        ),
      ),
      navigationRailTheme: NavigationRailThemeData(
        backgroundColor: isDark ? AppColors.surface : Colors.white,
        indicatorColor: AppColors.accentGlow,
        selectedIconTheme: const IconThemeData(
          color: AppColors.primary,
          size: 24,
        ),
        unselectedIconTheme: const IconThemeData(
          color: AppColors.textMuted,
          size: 24,
        ),
        selectedLabelTextStyle: const TextStyle(
          color: AppColors.primary,
          fontWeight: AppTypography.semibold,
        ),
        unselectedLabelTextStyle: const TextStyle(color: AppColors.textMuted),
      ),
      inputDecorationTheme: AppInputTheme.data(scheme, isDark: isDark),
      filledButtonTheme: FilledButtonThemeData(
        style: AppButtonTheme.filled(scheme, isDark: isDark),
      ),
      outlinedButtonTheme: OutlinedButtonThemeData(
        style: AppButtonTheme.outlined(scheme, isDark: isDark),
      ),
      textButtonTheme: TextButtonThemeData(
        style: AppButtonTheme.text(scheme, isDark: isDark),
      ),
      iconButtonTheme: IconButtonThemeData(style: AppButtonTheme.icon(scheme)),
      chipTheme: ChipThemeData(
        backgroundColor: AppColors.surfaceElevated,
        selectedColor: AppColors.accentGlow,
        side: const BorderSide(color: AppColors.surfaceBorder),
        labelStyle: TextStyle(
          color: isDark ? AppColors.textSecondary : AppColors.textMuted,
          fontSize: AppTypography.sizeSm,
        ),
      ),
      dividerTheme: DividerThemeData(
        color: isDark ? AppColors.surfaceBorder : Colors.black12,
        thickness: 1,
      ),
      snackBarTheme: SnackBarThemeData(
        behavior: SnackBarBehavior.floating,
        backgroundColor: isDark ? AppColors.surfaceElevated : Colors.black87,
        contentTextStyle: const TextStyle(color: AppColors.textPrimary),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppRadius.md),
        ),
      ),
      dialogTheme: DialogThemeData(
        backgroundColor: isDark ? AppColors.surface : Colors.white,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppRadius.xl),
        ),
        elevation: 8,
        shadowColor: Colors.black,
      ),
      bottomSheetTheme: BottomSheetThemeData(
        backgroundColor: isDark ? AppColors.surface : Colors.white,
        shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(
            top: Radius.circular(AppRadius.xl),
          ),
        ),
        showDragHandle: true,
      ),
      tooltipTheme: TooltipThemeData(
        decoration: BoxDecoration(
          color: AppColors.surfaceElevated,
          borderRadius: BorderRadius.circular(AppRadius.sm),
          boxShadow: AppShadows.soft,
        ),
        textStyle: const TextStyle(
          color: AppColors.textPrimary,
          fontSize: AppTypography.sizeSm,
        ),
      ),
      progressIndicatorTheme: const ProgressIndicatorThemeData(
        color: AppColors.primary,
        linearTrackColor: AppColors.surfaceBorder,
      ),
      sliderTheme: const SliderThemeData(
        activeTrackColor: AppColors.primary,
        inactiveTrackColor: AppColors.surfaceBorder,
        thumbColor: AppColors.secondary,
        overlayColor: AppColors.accentGlow,
      ),
      switchTheme: SwitchThemeData(
        thumbColor: WidgetStateProperty.resolveWith(
          (states) => states.contains(WidgetState.selected)
              ? AppColors.secondary
              : AppColors.textMuted,
        ),
        trackColor: WidgetStateProperty.resolveWith(
          (states) => states.contains(WidgetState.selected)
              ? AppColors.accentGlow
              : AppColors.surfaceBorder,
        ),
      ),
      splashFactory: InkSparkle.splashFactory,
      pageTransitionsTheme: const PageTransitionsTheme(
        builders: {
          TargetPlatform.android: FadeForwardsPageTransitionsBuilder(),
          TargetPlatform.windows: FadeForwardsPageTransitionsBuilder(),
          TargetPlatform.iOS: CupertinoPageTransitionsBuilder(),
          TargetPlatform.macOS: CupertinoPageTransitionsBuilder(),
          TargetPlatform.linux: FadeForwardsPageTransitionsBuilder(),
        },
      ),
    );
  }

  static TextTheme _textTheme(bool isDark) {
    final baseColor = isDark ? AppColors.textPrimary : Colors.black87;
    return TextTheme(
      displayLarge: TextStyle(
        fontSize: AppTypography.size5xl,
        fontWeight: AppTypography.bold,
        color: baseColor,
        letterSpacing: -1,
      ),
      displayMedium: TextStyle(
        fontSize: AppTypography.size4xl,
        fontWeight: AppTypography.bold,
        color: baseColor,
      ),
      headlineLarge: TextStyle(
        fontSize: AppTypography.size3xl,
        fontWeight: AppTypography.bold,
        color: baseColor,
        letterSpacing: 0.2,
      ),
      headlineMedium: TextStyle(
        fontSize: AppTypography.size2xl,
        fontWeight: AppTypography.semibold,
        color: baseColor,
      ),
      titleLarge: TextStyle(
        fontSize: AppTypography.sizeXl,
        fontWeight: AppTypography.semibold,
        color: baseColor,
      ),
      titleMedium: TextStyle(
        fontSize: AppTypography.sizeLg,
        fontWeight: AppTypography.semibold,
        color: baseColor,
      ),
      bodyLarge: TextStyle(
        fontSize: AppTypography.sizeLg,
        height: 1.5,
        color: baseColor,
      ),
      bodyMedium: TextStyle(
        fontSize: AppTypography.sizeMd,
        height: 1.5,
        color: isDark ? AppColors.textSecondary : Colors.black87,
      ),
      bodySmall: TextStyle(
        fontSize: AppTypography.sizeSm,
        height: 1.4,
        color: isDark ? AppColors.textMuted : Colors.black54,
      ),
      labelLarge: TextStyle(
        fontSize: AppTypography.sizeMd,
        fontWeight: AppTypography.semibold,
        color: isDark ? AppColors.textPrimary : AppColors.primary,
      ),
      labelMedium: const TextStyle(
        fontSize: AppTypography.sizeSm,
        fontWeight: AppTypography.medium,
        color: AppColors.textSecondary,
      ),
    );
  }
}
