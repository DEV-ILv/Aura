import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../core/theme/app_colors.dart';

/// Animated AURA logo.
///
/// Draws the brand glyph with an orbiting energy ring. The animation is
/// cosmetic and never blocks UI work.
class AuraLogo extends StatefulWidget {
  const AuraLogo({super.key, this.size = 120, this.animate = true});

  final double size;
  final bool animate;

  @override
  State<AuraLogo> createState() => _AuraLogoState();
}

class _AuraLogoState extends State<AuraLogo>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 4),
    );
    if (widget.animate) {
      _controller.repeat();
    }
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AnimatedBuilder(
      animation: _controller,
      builder: (context, child) {
        return CustomPaint(
          size: Size.square(widget.size),
          painter: _AuraPainter(_controller.value),
        );
      },
    );
  }
}

class _AuraPainter extends CustomPainter {
  _AuraPainter(this.progress);

  final double progress;

  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero);
    final radius = size.shortestSide / 2;

    // Outer glow.
    final glowPaint = Paint()
      ..color = AppColors.accentGlow
      ..style = PaintingStyle.fill;
    canvas.drawCircle(center, radius * 0.95, glowPaint);

    // Ring.
    final ringPaint = Paint()
      ..shader = const SweepGradient(
        colors: [AppColors.primary, AppColors.secondary, AppColors.primary],
        transform: GradientRotation(-math.pi / 2),
      ).createShader(Rect.fromCircle(center: center, radius: radius))
      ..style = PaintingStyle.stroke
      ..strokeWidth = radius * 0.08;
    canvas.drawCircle(center, radius * 0.72, ringPaint);

    // Orbiting comet.
    final angle = progress * 2 * math.pi;
    final cometRadius = radius * 0.72;
    final cometCenter = Offset(
      center.dx + cometRadius * math.cos(angle),
      center.dy + cometRadius * math.sin(angle),
    );
    final cometPaint = Paint()
      ..color = AppColors.secondary
      ..maskFilter = const MaskFilter.blur(BlurStyle.normal, 6);
    canvas.drawCircle(cometCenter, radius * 0.1, cometPaint);

    // Core glyph.
    final corePaint = Paint()
      ..shader = const LinearGradient(
        begin: Alignment.topLeft,
        end: Alignment.bottomRight,
        colors: [AppColors.primary, AppColors.secondary],
      ).createShader(Rect.fromCircle(center: center, radius: radius * 0.4));
    canvas.drawCircle(center, radius * 0.32, corePaint);

    // Pulse dot in the middle.
    final pulse = 0.5 + 0.5 * math.sin(progress * 2 * math.pi);
    final dotPaint = Paint()
      ..color = Color.lerp(AppColors.primary, Colors.white, pulse)!;
    canvas.drawCircle(center, radius * 0.09 * (0.7 + 0.3 * pulse), dotPaint);
  }

  @override
  bool shouldRepaint(_AuraPainter oldDelegate) =>
      oldDelegate.progress != progress;
}
