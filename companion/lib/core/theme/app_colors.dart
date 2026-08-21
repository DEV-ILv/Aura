import 'package:flutter/material.dart';

/// Central palette for the AURA dark, futuristic visual identity.
abstract final class AppColors {
  // Brand accents.
  static const Color primary = Color(0xFF00B4FF);
  static const Color secondary = Color(0xFF00E5FF);
  static const Color tertiary = Color(0xFF4D8DFF);
  static const Color accentGlow = Color(0x3300B4FF);

  // Surfaces.
  static const Color background = Color(0xFF05080F);
  static const Color surface = Color(0xFF0B1220);
  static const Color surfaceElevated = Color(0xFF111A2E);
  static const Color surfaceBorder = Color(0xFF1B2740);

  // Glassmorphism.
  static const Color glassFill = Color(0x14FFFFFF);
  static const Color glassFillStrong = Color(0x1FFFFFFF);
  static const Color glassStroke = Color(0x29FFFFFF);
  static const Color glassStrokeBright = Color(0x45FFFFFF);

  // Text.
  static const Color textPrimary = Color(0xFFE8F1FF);
  static const Color textSecondary = Color(0xFF93A5C4);
  static const Color textMuted = Color(0xFF5A6B8C);

  // Semantic.
  static const Color success = Color(0xFF2EE6A8);
  static const Color warning = Color(0xFFFFC24B);
  static const Color danger = Color(0xFFFF5C7A);
  static const Color info = Color(0xFF58A6FF);

  // Gradient helpers.
  static const LinearGradient brandGradient = LinearGradient(
    begin: Alignment.topLeft,
    end: Alignment.bottomRight,
    colors: [primary, secondary],
  );
}
