import 'package:flutter/material.dart';

import 'app_colors.dart';

/// Central gradient presets for backgrounds, cards and brand elements.
abstract final class AppGradients {
  /// Brand primary-to-cyan diagonal.
  static const LinearGradient brand = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [AppColors.primary, AppColors.secondary],
  );

  /// Near-black radial backdrop with a faint blue centre.
  static const RadialGradient background = RadialGradient(
    center: Alignment(-0.2, -0.4),
    radius: 1.4,
    colors: [Color(0xFF0A1426), AppColors.background],
  );

  /// Translucent glass fill laid over the blur layer of a card.
  static const LinearGradient glass = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [Color(0x1FFFFFFF), Color(0x08FFFFFF)],
  );

  /// Hover / active tint used on interactive glass.
  static const LinearGradient glassActive = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [Color(0x24FFFFFF), Color(0x0CFFFFFF)],
  );

  /// Soft cyan glow used behind the AURA logo.
  static const RadialGradient logoGlow = RadialGradient(
    colors: [AppColors.accentGlow, Color(0x0000B4FF)],
  );
}
