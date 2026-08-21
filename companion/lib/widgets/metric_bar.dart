import 'package:flutter/material.dart';

import '../core/theme/app_colors.dart';

/// Horizontal gauge used for battery, storage, CPU and RAM usage.
class MetricBar extends StatelessWidget {
  const MetricBar({
    super.key,
    required this.fraction,
    this.height = 8,
    this.color = AppColors.primary,
  });

  /// Fraction between 0 and 1.
  final double fraction;
  final double height;
  final Color color;

  @override
  Widget build(BuildContext context) {
    final clamped = fraction.clamp(0.0, 1.0).toDouble();
    return ClipRRect(
      borderRadius: BorderRadius.circular(height / 2),
      child: SizedBox(
        height: height,
        child: Stack(
          children: [
            Container(color: AppColors.surfaceBorder),
            FractionallySizedBox(
              alignment: Alignment.centerLeft,
              widthFactor: clamped,
              child: Container(
                decoration: BoxDecoration(
                  gradient: LinearGradient(
                    colors: [color, AppColors.secondary],
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
