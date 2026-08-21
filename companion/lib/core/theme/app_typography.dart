import 'package:flutter/material.dart';

import 'app_colors.dart';

/// Central typography tokens for the AURA interface.
///
/// Screens should use [Theme.of(context).textTheme] (built from these tokens)
/// rather than ad-hoc `TextStyle` literals.
abstract final class AppTypography {
  static const String display = 'Roboto';
  static const String body = 'Roboto';
  static const String mono = 'RobotoMono';

  static const FontWeight regular = FontWeight.w400;
  static const FontWeight medium = FontWeight.w500;
  static const FontWeight semibold = FontWeight.w600;
  static const FontWeight bold = FontWeight.w700;
  static const FontWeight extrabold = FontWeight.w800;

  static const double sizeXs = 11;
  static const double sizeSm = 12;
  static const double sizeMd = 14;
  static const double sizeLg = 16;
  static const double sizeXl = 18;
  static const double size2xl = 22;
  static const double size3xl = 28;
  static const double size4xl = 36;
  static const double size5xl = 48;

  /// Small caps style used for section titles / labels.
  static const TextStyle sectionLabel = TextStyle(
    color: AppColors.textSecondary,
    fontSize: sizeSm,
    fontWeight: bold,
    letterSpacing: 1.2,
  );
}
