import 'dart:math' as math;

import 'package:flutter/material.dart';

import '../core/theme/app_animations.dart';
import '../core/theme/app_colors.dart';
import '../core/theme/app_gradients.dart';

/// Ambient animated backdrop: a dark gradient with slowly drifting
/// blue/cyan glow orbs. Used on the login, splash and about surfaces.
class AnimatedBackground extends StatefulWidget {
  const AnimatedBackground({super.key, this.child});

  final Widget? child;

  @override
  State<AnimatedBackground> createState() => _AnimatedBackgroundState();
}

class _AnimatedBackgroundState extends State<AnimatedBackground>
    with SingleTickerProviderStateMixin {
  late final AnimationController _controller;

  @override
  void initState() {
    super.initState();
    _controller = AnimationController(
      vsync: this,
      duration: AppAnimations.breathe * 4,
    )..repeat();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: const BoxDecoration(gradient: AppGradients.background),
      child: Stack(
        fit: StackFit.expand,
        children: [
          AnimatedBuilder(
            animation: _controller,
            builder: (context, _) {
              final t = _controller.value;
              return Stack(
                fit: StackFit.expand,
                children: [
                  _Orb(
                    color: AppColors.accentGlow,
                    position: Offset(
                      (math.sin(t * 2 * math.pi) * 0.5 + 0.5) * 0.7,
                      (math.cos(t * 2 * math.pi) * 0.5 + 0.5) * 0.7,
                    ),
                    radius: 340,
                  ),
                  _Orb(
                    color: AppColors.secondary.withValues(alpha: 0.10),
                    position: Offset(
                      (math.cos((t + 0.4) * 2 * math.pi) * 0.5 + 0.5) * 0.7,
                      (math.sin((t + 0.4) * 2 * math.pi) * 0.5 + 0.5) * 0.7,
                    ),
                    radius: 260,
                  ),
                ],
              );
            },
          ),
          if (widget.child != null) widget.child!,
        ],
      ),
    );
  }
}

class _Orb extends StatelessWidget {
  const _Orb({
    required this.color,
    required this.position,
    required this.radius,
  });

  final Color color;
  final Offset position;
  final double radius;

  @override
  Widget build(BuildContext context) {
    return Align(
      alignment: Alignment(position.dx * 2 - 1, position.dy * 2 - 1),
      child: Container(
        width: radius,
        height: radius,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: RadialGradient(colors: [color, Colors.transparent]),
        ),
      ),
    );
  }
}
