import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/services/logger.dart';
import '../../core/theme/app_colors.dart';
import '../../core/theme/app_spacing.dart';
import '../../providers/app_providers.dart';
import '../../providers/connection_provider.dart';
import '../../routes/app_routes.dart';
import '../../widgets/animated_background.dart';
import '../../widgets/aura_logo.dart';

/// Entry screen shown while the app initialises.
///
/// Displays the animated brand, performs the initial connection and routes
/// to the home or connection screen depending on the outcome.
class SplashScreen extends ConsumerStatefulWidget {
  const SplashScreen({super.key});

  @override
  ConsumerState<SplashScreen> createState() => _SplashScreenState();
}

class _SplashScreenState extends ConsumerState<SplashScreen> {
  bool _bootstrapped = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) => _bootstrap());
  }

  Future<void> _bootstrap() async {
    try {
      await ref.read(bootstrapProvider.future);
      await ref.read(connectionProvider.notifier).initialize();
    } catch (error) {
      // Never let the splash hang: any bootstrap failure routes to the
      // connection screen, which reports the device as unavailable.
      Logger.warning('Bootstrap failed: $error');
    }
    if (mounted) {
      setState(() => _bootstrapped = true);
    }
  }

  void _navigate(String route) {
    if (!mounted) {
      return;
    }
    Navigator.of(context).pushReplacementNamed(route);
  }

  @override
  Widget build(BuildContext context) {
    final connection = ref.watch(connectionProvider);

    if (_bootstrapped) {
      if (connection.phase == ConnectionPhase.connected) {
        WidgetsBinding.instance.addPostFrameCallback(
          (_) => _navigate(AppRoutes.home),
        );
      } else if (connection.phase == ConnectionPhase.idle ||
          connection.phase == ConnectionPhase.unauthenticated ||
          connection.phase == ConnectionPhase.unavailable ||
          connection.phase == ConnectionPhase.error ||
          connection.phase == ConnectionPhase.disconnected) {
        WidgetsBinding.instance.addPostFrameCallback(
          (_) => _navigate(AppRoutes.connection),
        );
      }
    }

    final message = switch (connection.phase) {
      ConnectionPhase.probing => connection.message,
      ConnectionPhase.testing => connection.message,
      ConnectionPhase.connecting => connection.message,
      ConnectionPhase.unauthenticated => 'Checking your session…',
      ConnectionPhase.unavailable => connection.message,
      ConnectionPhase.error =>
        'Device not reachable. Opening connection setup…',
      _ => 'Initializing…',
    };

    return Scaffold(
      body: AnimatedBackground(
        child: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const AuraLogo(size: 140),
              const SizedBox(height: AppSpacing.xxl),
              ShaderMask(
                shaderCallback: (bounds) =>
                    AppColors.brandGradient.createShader(bounds),
                child: const Text(
                  'A.U.R.A',
                  style: TextStyle(
                    fontSize: 42,
                    fontWeight: FontWeight.w800,
                    letterSpacing: 8,
                    color: Colors.white,
                  ),
                ),
              ),
              const SizedBox(height: AppSpacing.sm),
              const Text(
                'Companion',
                style: TextStyle(
                  color: AppColors.textSecondary,
                  fontSize: 14,
                  letterSpacing: 6,
                ),
              ),
              const SizedBox(height: 48),
              const SizedBox(
                width: 160,
                child: LinearProgressIndicator(
                  minHeight: 3,
                  backgroundColor: AppColors.surfaceBorder,
                ),
              ),
              const SizedBox(height: AppSpacing.lg),
              Text(
                message,
                style: const TextStyle(
                  color: AppColors.textMuted,
                  fontSize: 13,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
