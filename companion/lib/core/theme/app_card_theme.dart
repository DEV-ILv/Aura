import 'package:flutter/material.dart';

import 'app_colors.dart';
import 'app_radius.dart';

/// Material 3 card theme used as the fallback for plain `Card` widgets.
///
/// Rich glass surfaces should use the `GlassCard` widget instead.
abstract final class AppCardTheme {
  static CardThemeData data({bool isDark = true}) {
    return CardThemeData(
      color: isDark ? AppColors.surface : Colors.white,
      elevation: 0,
      shadowColor: Colors.transparent,
      margin: EdgeInsets.zero,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(AppRadius.lg),
        side: BorderSide(
          color: isDark ? AppColors.surfaceBorder : Colors.black12,
        ),
      ),
    );
  }
}
