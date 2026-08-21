import 'package:flutter/material.dart';

import 'app_colors.dart';

/// Elevation and glow shadow presets.
abstract final class AppShadows {
  /// Soft, low elevation for resting cards.
  static const List<BoxShadow> soft = [
    BoxShadow(color: Color(0x33000000), blurRadius: 18, offset: Offset(0, 6)),
  ];

  /// Slightly deeper elevation for raised / interacting cards.
  static const List<BoxShadow> raised = [
    BoxShadow(color: Color(0x4D000000), blurRadius: 28, offset: Offset(0, 10)),
  ];

  /// Subtle blue glow used on primary elements.
  static List<BoxShadow> glow([Color color = AppColors.primary]) {
    return [BoxShadow(color: color.withValues(alpha: 0.35), blurRadius: 24)];
  }

  /// Inner soft highlight used to fake the glass edge.
  static const List<BoxShadow> inner = [
    BoxShadow(color: Color(0x0DFFFFFF), blurRadius: 1),
  ];
}
