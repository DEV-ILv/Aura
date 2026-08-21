import 'dart:ui' show ImageFilter;

import 'package:flutter/gestures.dart';
import 'package:flutter/material.dart';

import '../core/theme/app_animations.dart';
import '../core/theme/app_colors.dart';
import '../core/theme/app_gradients.dart';
import '../core/theme/app_radius.dart';
import '../core/theme/app_shadows.dart';

/// Frosted, bordered glass surface used for all cards in the app.
///
/// Supports an optional hover glow and press scale, so the same widget can
/// power both static panels and interactive tiles.
class GlassCard extends StatefulWidget {
  const GlassCard({
    super.key,
    required this.child,
    this.padding = const EdgeInsets.all(16),
    this.onTap,
    this.hover = true,
    this.glow = false,
    this.blur = true,
    this.radius = AppRadius.lg,
    this.accent,
  });

  final Widget child;
  final EdgeInsetsGeometry padding;
  final VoidCallback? onTap;
  final bool hover;
  final bool glow;
  final bool blur;
  final double radius;
  final Color? accent;

  @override
  State<GlassCard> createState() => _GlassCardState();
}

class _GlassCardState extends State<GlassCard> {
  bool _hovered = false;
  bool _pressed = false;

  void _onEnter(PointerEnterEvent _) => setState(() => _hovered = true);

  void _onExit(PointerExitEvent _) => setState(() => _hovered = false);

  void _onDown(PointerDownEvent _) => setState(() => _pressed = true);

  void _onUp(PointerUpEvent _) => setState(() => _pressed = false);

  void _onCancel(PointerCancelEvent _) => setState(() => _pressed = false);

  @override
  Widget build(BuildContext context) {
    final accent = widget.accent ?? AppColors.primary;
    final interactive = widget.onTap != null;
    final elevated = _hovered || widget.glow;

    Widget content = _GlassSurface(
      radius: widget.radius,
      gradient: _hovered ? AppGradients.glassActive : AppGradients.glass,
      borderColor: elevated
          ? AppColors.glassStrokeBright
          : AppColors.glassStroke,
      glow: elevated ? accent : null,
      blur: widget.blur,
      child: Padding(padding: widget.padding, child: widget.child),
    );

    if (interactive) {
      content = MouseRegion(
        onEnter: _onEnter,
        onExit: _onExit,
        cursor: SystemMouseCursors.click,
        child: content,
      );
      // The Listener drives only the press visual; GestureDetector owns the
      // tap gesture so onTap actually fires (and respects the gesture arena).
      return GestureDetector(
        onTap: widget.onTap,
        child: Listener(
          onPointerDown: _onDown,
          onPointerUp: _onUp,
          onPointerCancel: _onCancel,
          child: AnimatedScale(
            scale: _pressed ? 0.985 : 1,
            duration: AppAnimations.fast,
            curve: AppAnimations.press,
            child: content,
          ),
        ),
      );
    }

    if (widget.hover) {
      content = MouseRegion(onEnter: _onEnter, onExit: _onExit, child: content);
    }
    return content;
  }
}

class _GlassSurface extends StatelessWidget {
  const _GlassSurface({
    required this.radius,
    required this.gradient,
    required this.borderColor,
    required this.glow,
    required this.blur,
    required this.child,
  });

  final double radius;
  final Gradient gradient;
  final Color borderColor;
  final Color? glow;
  final bool blur;
  final Widget child;

  @override
  Widget build(BuildContext context) {
    Widget surface = DecoratedBox(
      decoration: BoxDecoration(
        gradient: gradient,
        borderRadius: BorderRadius.circular(radius),
        border: Border.all(color: borderColor),
      ),
      child: child,
    );

    if (blur) {
      surface = ClipRRect(
        borderRadius: BorderRadius.circular(radius),
        child: BackdropFilter(
          filter: ImageFilter.blur(sigmaX: 18, sigmaY: 18),
          child: surface,
        ),
      );
    }

    return DecoratedBox(
      decoration: BoxDecoration(
        boxShadow: [
          ...AppShadows.soft,
          if (glow != null) ...AppShadows.glow(glow!),
        ],
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(radius),
        child: surface,
      ),
    );
  }
}
