import 'package:flutter/material.dart';

/// Motion timings and curves used across the application.
abstract final class AppAnimations {
  /// Micro interactions: icon toggles, badge fades.
  static const Duration fast = Duration(milliseconds: 180);

  /// Standard transitions: cards, dialogs, screen fades.
  static const Duration normal = Duration(milliseconds: 280);

  /// Emphasized / large transitions.
  static const Duration slow = Duration(milliseconds: 480);

  /// Ambient loops: glow breathing, orbs, loading shimmer.
  static const Duration breathe = Duration(seconds: 3);

  /// Default ease used for most movement.
  static const Curve standard = Curves.easeOutCubic;

  /// Snappy but soft for press / hover states.
  static const Curve press = Curves.easeOut;

  /// Springy entrance for hero elements.
  static const Curve entrance = Curves.easeOutBack;

  /// Standard page/route transition (fade + slight slide).
  static PageTransitionsBuilder pageTransition =
      const FadeForwardsPageTransitionsBuilder();
}
