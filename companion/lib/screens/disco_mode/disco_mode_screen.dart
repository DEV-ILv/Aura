import 'dart:async';

import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../core/theme/app_spacing.dart';
import '../../core/theme/app_typography.dart';
import '../../providers/connection_provider.dart';
import '../../providers/device_control_provider.dart';
import '../../widgets/glass_card.dart';

/// App-controlled Disco Mode: turns the LED ring into a party light override
/// that plays a rotating set of animations at a configurable brightness.
class DiscoModeScreen extends ConsumerStatefulWidget {
  const DiscoModeScreen({super.key});

  @override
  ConsumerState<DiscoModeScreen> createState() => _DiscoModeScreenState();
}

class _DiscoModeScreenState extends ConsumerState<DiscoModeScreen> {
  DeviceControlNotifier? _control;

  bool get _canControl {
    final connection = ref.read(connectionProvider);
    return connection.isConnected && connection.mode == ConnectionMode.local;
  }

  @override
  void initState() {
    super.initState();
    _control = ref.read(deviceControlProvider.notifier);
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_canControl) {
        unawaited(_control!.load());
      }
    });
  }

  Future<void> _apply(Future<void> Function() action) async {
    try {
      await action();
    } catch (error) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Failed: $error')));
      }
    }
  }

  void _fire(Future<void> Function() action) {
    unawaited(_apply(action));
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(deviceControlProvider);
    final led = state.led;

    return Scaffold(
      appBar: AppBar(title: const Text('Disco Mode')),
      body: ListView(
        padding: const EdgeInsets.all(AppSpacing.page),
        children: [
          if (!_canControl) const _LocalOnlyBanner(),
          const SizedBox(height: AppSpacing.section),
          GlassCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            'Party lights',
                            style: TextStyle(
                              color: AppColors.textPrimary,
                              fontSize: AppTypography.sizeLg,
                              fontWeight: AppTypography.semibold,
                            ),
                          ),
                          SizedBox(height: 4),
                          Text(
                            'Overrides the live mood with rotating animations.',
                            style: TextStyle(
                              color: AppColors.textMuted,
                              fontSize: AppTypography.sizeSm,
                            ),
                          ),
                        ],
                      ),
                    ),
                    Switch(
                      value: led.disco,
                      onChanged: _canControl
                          ? (value) => _fire(
                              () => _control!.setLed({'disco': value}),
                            )
                          : null,
                    ),
                  ],
                ),
                const SizedBox(height: AppSpacing.lg),
                _StatusLine(
                  active: led.discoActive,
                  text: led.discoActive
                      ? 'Animations running'
                      : led.disco
                      ? 'Waiting for a calm mood\u2026'
                      : 'Off',
                ),
              ],
            ),
          ),
          const SizedBox(height: AppSpacing.section),
          GlassCard(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
const Expanded(
                  child: Text('Brightness', style: AppTypography.sectionLabel),
                ),
                    Text(
                      '${led.discoBrightness}%',
                      style: const TextStyle(
                        color: AppColors.textPrimary,
                        fontWeight: AppTypography.semibold,
                      ),
                    ),
                  ],
                ),
                Slider(
                  value: led.discoBrightness
                      .clamp(10, 100)
                      .toDouble(),
                  min: 10,
                  max: 100,
                  divisions: 90,
                  onChanged: _canControl ? (value) {} : null,
                  onChangeEnd: _canControl
                      ? (value) => _fire(
                          () => _control!.setLed({
                            'discoBrightness': value.round(),
                          }),
                        )
                      : null,
                ),
                const Text(
                  'Sets the target brightness while Disco Mode is active. '
                  'Saved on the device and restored on reconnect.',
                  style: TextStyle(
                    color: AppColors.textMuted,
                    fontSize: AppTypography.sizeSm,
                  ),
                ),
              ],
            ),
          ),
          const SizedBox(height: AppSpacing.section),
          const GlassCard(
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Icon(Icons.lightbulb_outline,
                    color: AppColors.textMuted, size: 20),
                SizedBox(width: 12),
                Expanded(
                  child: Text(
                    'Disco Mode is app-only and not persisted: the ring '
                    'returns to the live mood after a reboot or once Disco is '
                    'switched off. Ten animations cycle automatically.',
                    style: TextStyle(
                      color: AppColors.textSecondary,
                      fontSize: AppTypography.sizeSm,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _StatusLine extends StatelessWidget {
  const _StatusLine({required this.active, required this.text});

  final bool active;
  final String text;

  @override
  Widget build(BuildContext context) {
    final color = active ? AppColors.success : AppColors.textMuted;
    return Row(
      children: [
        Icon(Icons.circle, color: color, size: 10),
        const SizedBox(width: 8),
        Text(
          text,
          style: TextStyle(
            color: active ? AppColors.success : AppColors.textSecondary,
            fontSize: AppTypography.sizeSm,
          ),
        ),
      ],
    );
  }
}

class _LocalOnlyBanner extends StatelessWidget {
  const _LocalOnlyBanner();

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(AppSpacing.lg),
      decoration: BoxDecoration(
        color: AppColors.warning.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AppColors.warning.withValues(alpha: 0.5)),
      ),
      child: const Row(
        children: [
          Icon(Icons.info_outline, color: AppColors.warning),
          SizedBox(width: 12),
          Expanded(
            child: Text(
              'Connect to the device on your local network to control '
              'hardware. Cloud mode shows status only.',
              style: TextStyle(color: AppColors.textSecondary),
            ),
          ),
        ],
      ),
    );
  }
}
