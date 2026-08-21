import 'dart:async';

import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/config/app_config.dart';
import '../../core/theme/app_colors.dart';
import '../../core/theme/app_icons.dart';
import '../../core/theme/app_radius.dart';
import '../../core/theme/app_shadows.dart';
import '../../core/theme/app_spacing.dart';
import '../../providers/connection_provider.dart';
import '../../providers/settings_provider.dart';
import '../../routes/app_routes.dart';
import '../../widgets/animated_background.dart';
import '../../widgets/aura_logo.dart';
import '../../widgets/glass_card.dart';

/// Premium AURA sign-in.
///
/// The card shown depends on the connection mode:
/// - Local device reached  -> branded device sign-in (username/password).
/// - Device unreachable     -> "AURA device unavailable" with a path into
///                              AURA Cloud sign-in.
/// - Remote (cloud) mode    -> email/password sign-in with Sign Up and
///                              Forgot Password via Supabase.
///
/// Networking details are hidden entirely behind the developer settings screen.
class ConnectionScreen extends ConsumerStatefulWidget {
  const ConnectionScreen({super.key});

  @override
  ConsumerState<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends ConsumerState<ConnectionScreen> {
  final _formKey = GlobalKey<FormState>();
  late final TextEditingController _usernameController;
  late final TextEditingController _passwordController;
  late final TextEditingController _emailController;
  late final TextEditingController _cloudPasswordController;
  late final TextEditingController _confirmPasswordController;

  bool _obscurePassword = true;
  bool _rememberMe = true;
  bool _signUpMode = false;
  bool _cloudBusy = false;
  String? _cloudError;

  @override
  void initState() {
    super.initState();
    final settings = ref.read(settingsProvider).settings;
    // Development builds prefill the well-known development credentials
    // (Devil / Devil) for frictionless local testing. Production builds leave
    // the fields empty: the firmware generates a unique admin password on
    // first boot and prints it to the Serial monitor.
    final remembered = settings.authUsername;
    final prefillUser = AppConfig.kDevelopmentMode
        ? AppConfig.kDevUsername
        : remembered;
    _usernameController = TextEditingController(text: prefillUser);
    _passwordController = TextEditingController(
      text: AppConfig.kDevelopmentMode ? AppConfig.kDevPassword : '',
    );
    _emailController = TextEditingController();
    _cloudPasswordController = TextEditingController();
    _confirmPasswordController = TextEditingController();
  }

  @override
  void dispose() {
    _usernameController.dispose();
    _passwordController.dispose();
    _emailController.dispose();
    _cloudPasswordController.dispose();
    _confirmPasswordController.dispose();
    super.dispose();
  }

  void _goHome() {
    if (!mounted) {
      return;
    }
    unawaited(Navigator.of(context).pushReplacementNamed(AppRoutes.home));
  }

  Future<void> _login() async {
    FocusScope.of(context).unfocus();
    final username = _usernameController.text.trim();
    if (username.isEmpty || _passwordController.text.isEmpty) {
      _showMessage('Enter your username and password');
      return;
    }

    final notifier = ref.read(connectionProvider.notifier);
    final ok = await notifier.login(username, _passwordController.text);

    if (!_rememberMe) {
      // Keep the in-memory session but do not persist it across restarts.
      await ref.read(settingsProvider.notifier).clearAuth();
    }

    if (ok && mounted) {
      _goHome();
    }
  }

  Future<void> _cloudSignIn() async {
    final email = _emailController.text.trim();
    if (email.isEmpty || _cloudPasswordController.text.isEmpty) {
      _showMessage('Enter your email and password');
      return;
    }
    setState(() {
      _cloudBusy = true;
      _cloudError = null;
    });
    final notifier = ref.read(connectionProvider.notifier);
    final ok = await notifier.cloudLogin(email, _cloudPasswordController.text);
    if (mounted) {
      setState(() => _cloudBusy = false);
      if (ok) {
        _goHome();
      } else {
        setState(() => _cloudError = ref.read(connectionProvider).message);
      }
    }
  }

  Future<void> _cloudSignUp() async {
    final email = _emailController.text.trim();
    final password = _cloudPasswordController.text;
    if (email.isEmpty || password.isEmpty) {
      _showMessage('Enter your email and password');
      return;
    }
    if (password != _confirmPasswordController.text) {
      _showMessage('Passwords do not match');
      return;
    }
    setState(() {
      _cloudBusy = true;
      _cloudError = null;
    });
    final notifier = ref.read(connectionProvider.notifier);
    final ok = await notifier.cloudSignUp(email, password);
    if (mounted) {
      setState(() => _cloudBusy = false);
      if (ok) {
        _goHome();
      } else {
        setState(() => _cloudError = ref.read(connectionProvider).message);
      }
    }
  }

  Future<void> _forgotPassword() async {
    final controller = TextEditingController();
    final email = await showDialog<String>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Reset your password'),
        content: TextField(
          controller: controller,
          keyboardType: TextInputType.emailAddress,
          decoration: const InputDecoration(
            labelText: 'Email address',
            prefixIcon: Icon(AppIcons.email),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(controller.text.trim()),
            child: const Text('Send reset link'),
          ),
        ],
      ),
    );
    controller.dispose();
    if (email == null || email.isEmpty) {
      return;
    }
    final sent = await ref
        .read(connectionProvider.notifier)
        .cloudForgotPassword(email);
    if (!mounted) {
      return;
    }
    _showMessage(
      sent
          ? 'A password reset link has been sent to $email.'
          : 'Could not send the reset link. Please try again.',
    );
  }

  void _showMessage(String message) {
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  Future<void> _retry() async {
    await ref.read(connectionProvider.notifier).retry();
  }

  Future<void> _switchToCloud() async {
    await ref.read(connectionProvider.notifier).switchToCloud();
  }

  Future<void> _switchToLocal() async {
    setState(() {
      _signUpMode = false;
      _cloudError = null;
    });
    await ref.read(connectionProvider.notifier).switchToLocal();
  }

  @override
  Widget build(BuildContext context) {
    final connection = ref.watch(connectionProvider);
    final phase = connection.phase;

    if (connection.isConnected) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _goHome());
    }

    final Widget card;
    if (phase == ConnectionPhase.unavailable) {
      card = _buildUnavailableCard(connection);
    } else if (connection.mode == ConnectionMode.remote) {
      card = _buildCloudCard(connection);
    } else if (phase == ConnectionPhase.waitingForDevice) {
      card = _buildWaitingForDeviceCard(connection);
    } else {
      card = _buildLoginCard(connection);
    }

    return Scaffold(
      body: AnimatedBackground(
        child: SafeArea(
          child: Center(
            child: SingleChildScrollView(
              padding: const EdgeInsets.all(AppSpacing.xl),
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 460),
                child: card,
              ),
            ),
          ),
        ),
      ),
    );
  }

  // ---------------------------------------------------------------------------
  // Local device sign-in
  // ---------------------------------------------------------------------------

  Widget _buildLoginCard(ConnectionState connection) {
    final textTheme = Theme.of(context).textTheme;
    final busy = connection.isBusy;

    return Form(
      key: _formKey,
      child: GlassCard(
        padding: const EdgeInsets.all(AppSpacing.xl),
        glow: true,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            _buildLogo(),
            const SizedBox(height: AppSpacing.lg),
            Text(
              'A.U.R.A',
              textAlign: TextAlign.center,
              style: textTheme.headlineMedium,
            ),
            const SizedBox(height: AppSpacing.xs),
            Text(
              'Personal AI Assistant',
              textAlign: TextAlign.center,
              style: textTheme.bodyMedium,
            ),
            const SizedBox(height: AppSpacing.xl),
            TextFormField(
              controller: _usernameController,
              decoration: const InputDecoration(
                labelText: 'Username',
                prefixIcon: Icon(AppIcons.person),
              ),
              textInputAction: TextInputAction.next,
            ),
            const SizedBox(height: AppSpacing.md),
            TextFormField(
              controller: _passwordController,
              obscureText: _obscurePassword,
              decoration: InputDecoration(
                labelText: 'Password',
                prefixIcon: const Icon(AppIcons.lock),
                suffixIcon: IconButton(
                  icon: Icon(
                    _obscurePassword
                        ? AppIcons.visibility
                        : AppIcons.visibilityOff,
                  ),
                  onPressed: () =>
                      setState(() => _obscurePassword = !_obscurePassword),
                ),
              ),
              textInputAction: TextInputAction.done,
              onFieldSubmitted: (_) => _login(),
            ),
            const SizedBox(height: AppSpacing.xs),
            Row(
              children: [
                Checkbox(
                  value: _rememberMe,
                  onChanged: (value) =>
                      setState(() => _rememberMe = value ?? false),
                ),
                const SizedBox(width: AppSpacing.xs),
                const Text('Remember me'),
                const Spacer(),
                TextButton(
                  onPressed: _forgotPassword,
                  child: const Text('Forgot password?'),
                ),
              ],
            ),
            const SizedBox(height: AppSpacing.sm),
            _buildLoginButton(busy),
            if (connection.phase == ConnectionPhase.error) ...[
              const SizedBox(height: AppSpacing.lg),
              _buildError(connection.message),
            ],
            const SizedBox(height: AppSpacing.md),
            TextButton.icon(
              onPressed: busy ? null : () => _switchToCloud(),
              icon: const Icon(Icons.cloud_outlined, size: 18),
              label: const Text('Use AURA Cloud sign-in'),
            ),
          ],
        ),
      ),
    );
  }

  // ---------------------------------------------------------------------------
  // Cloud (Supabase) sign-in
  // ---------------------------------------------------------------------------

  Widget _buildCloudCard(ConnectionState connection) {
    final textTheme = Theme.of(context).textTheme;
    final passwordField = TextFormField(
      controller: _cloudPasswordController,
      obscureText: _obscurePassword,
      decoration: InputDecoration(
        labelText: _signUpMode ? 'Password' : 'Password',
        prefixIcon: const Icon(AppIcons.lock),
        suffixIcon: IconButton(
          icon: Icon(
            _obscurePassword ? AppIcons.visibility : AppIcons.visibilityOff,
          ),
          onPressed: () => setState(() => _obscurePassword = !_obscurePassword),
        ),
      ),
      textInputAction: _signUpMode
          ? TextInputAction.next
          : TextInputAction.done,
      onFieldSubmitted: (_) => _signUpMode ? null : _cloudSignIn(),
    );

    return GlassCard(
      padding: const EdgeInsets.all(AppSpacing.xl),
      glow: true,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _buildLogo(),
          const SizedBox(height: AppSpacing.lg),
          Text(
            'AURA Cloud',
            textAlign: TextAlign.center,
            style: textTheme.headlineMedium,
          ),
          const SizedBox(height: AppSpacing.xs),
          Text(
            _signUpMode
                ? 'Create your cloud account to access AURA remotely.'
                : 'Sign in to access AURA when your device is out of range.',
            textAlign: TextAlign.center,
            style: textTheme.bodyMedium,
          ),
          const SizedBox(height: AppSpacing.xl),
          TextFormField(
            controller: _emailController,
            keyboardType: TextInputType.emailAddress,
            decoration: const InputDecoration(
              labelText: 'Email',
              prefixIcon: Icon(AppIcons.email),
            ),
            textInputAction: TextInputAction.next,
          ),
          const SizedBox(height: AppSpacing.md),
          passwordField,
          if (_signUpMode) ...[
            const SizedBox(height: AppSpacing.md),
            TextFormField(
              controller: _confirmPasswordController,
              obscureText: _obscurePassword,
              decoration: const InputDecoration(
                labelText: 'Confirm password',
                prefixIcon: Icon(AppIcons.lock),
              ),
              textInputAction: TextInputAction.done,
              onFieldSubmitted: (_) => _cloudSignUp(),
            ),
          ],
          const SizedBox(height: AppSpacing.sm),
          Row(
            children: [
              const Spacer(),
              TextButton(
                onPressed: _cloudBusy ? null : _forgotPassword,
                child: const Text('Forgot password?'),
              ),
            ],
          ),
          const SizedBox(height: AppSpacing.sm),
          FilledButton(
            onPressed: _cloudBusy
                ? null
                : () => _signUpMode ? _cloudSignUp() : _cloudSignIn(),
            style: FilledButton.styleFrom(
              padding: const EdgeInsets.symmetric(vertical: 16),
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(AppRadius.md),
              ),
            ),
            child: _cloudBusy
                ? const SizedBox(
                    width: 22,
                    height: 22,
                    child: CircularProgressIndicator(
                      strokeWidth: 2.4,
                      color: Colors.white,
                    ),
                  )
                : Text(_signUpMode ? 'Create Account' : 'Sign In'),
          ),
          if (_cloudError != null || _cloudErrorIsFrom(connection)) ...[
            const SizedBox(height: AppSpacing.lg),
            _buildError(_cloudError ?? connection.message),
          ],
          const SizedBox(height: AppSpacing.md),
          TextButton(
            onPressed: _cloudBusy
                ? null
                : () => setState(() => _signUpMode = !_signUpMode),
            child: Text(
              _signUpMode
                  ? 'Already have an account? Sign in'
                  : 'New here? Create an account',
            ),
          ),
          TextButton.icon(
            onPressed: _cloudBusy ? null : () => _switchToLocal(),
            icon: const Icon(Icons.devices_other, size: 18),
            label: const Text('Back to local device'),
          ),
        ],
      ),
    );
  }

  /// The connection screen also surfaces sign-in errors set by the notifier.
  bool _cloudErrorIsFrom(ConnectionState connection) {
    return connection.phase == ConnectionPhase.unauthenticated &&
        connection.mode == ConnectionMode.remote &&
        connection.message.isNotEmpty &&
        connection.message != 'Signed out of AURA Cloud.';
  }

  // ---------------------------------------------------------------------------
  // Device unavailable
  // ---------------------------------------------------------------------------

  Widget _buildUnavailableCard(ConnectionState connection) {
    final textTheme = Theme.of(context).textTheme;
    return GlassCard(
      padding: const EdgeInsets.all(AppSpacing.xl),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _buildLogo(),
          const SizedBox(height: AppSpacing.lg),
          Text(
            'AURA device unavailable.',
            textAlign: TextAlign.center,
            style: textTheme.titleLarge,
          ),
          const SizedBox(height: AppSpacing.sm),
          Text(
            'The device could not be reached. Check that it is powered on '
            'and within range, then try again — or continue through AURA Cloud.',
            textAlign: TextAlign.center,
            style: textTheme.bodyMedium,
          ),
          const SizedBox(height: AppSpacing.xl),
          FilledButton.icon(
            onPressed: connection.isBusy ? null : () => _retry(),
            icon: connection.isBusy
                ? const SizedBox(
                    width: 18,
                    height: 18,
                    child: CircularProgressIndicator(
                      strokeWidth: 2,
                      color: Colors.white,
                    ),
                  )
                : const Icon(AppIcons.refresh),
            label: Text(connection.isBusy ? 'Contacting…' : 'Retry'),
          ),
          const SizedBox(height: AppSpacing.md),
          FilledButton.tonalIcon(
            onPressed: connection.isBusy ? null : () => _switchToCloud(),
            icon: const Icon(Icons.cloud_outlined),
            label: const Text('Continue with AURA Cloud'),
          ),
          const SizedBox(height: AppSpacing.md),
          OutlinedButton.icon(
            onPressed: connection.isBusy
                ? null
                : () => Navigator.of(context).pushNamed(AppRoutes.developer),
            icon: const Icon(Icons.tune),
            label: const Text('Advanced Settings'),
          ),
        ],
      ),
    );
  }

  Widget _buildWaitingForDeviceCard(ConnectionState connection) {
    final textTheme = Theme.of(context).textTheme;

    return GlassCard(
      padding: const EdgeInsets.all(AppSpacing.xl),
      glow: true,
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          _buildLogo(),
          const SizedBox(height: AppSpacing.lg),
          Text(
            'Connecting to Wi-Fi…',
            textAlign: TextAlign.center,
            style: textTheme.headlineSmall,
          ),
          const SizedBox(height: AppSpacing.md),
          Text(
            'AURA is joining your Wi-Fi network. This takes about 5 seconds.',
            textAlign: TextAlign.center,
            style: textTheme.bodyMedium,
          ),
          const SizedBox(height: AppSpacing.xl),
          const SizedBox(
            width: 160,
            child: LinearProgressIndicator(
              minHeight: 4,
              backgroundColor: AppColors.surfaceBorder,
            ),
          ),
          const SizedBox(height: AppSpacing.lg),
          Text(
            'Please wait while AURA joins your network…',
            textAlign: TextAlign.center,
            style: textTheme.bodyMedium?.copyWith(color: AppColors.textMuted),
          ),
        ],
      ),
    );
  }

  Widget _buildLogo() {
    return Center(
      child: Container(
        padding: const EdgeInsets.all(10),
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          boxShadow: AppShadows.glow(),
        ),
        child: const AuraLogo(size: 84),
      ),
    );
  }

  Widget _buildLoginButton(bool busy) {
    return FilledButton(
      onPressed: busy ? null : _login,
      style: FilledButton.styleFrom(
        padding: const EdgeInsets.symmetric(vertical: 16),
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(AppRadius.md),
        ),
      ),
      child: busy
          ? const SizedBox(
              width: 22,
              height: 22,
              child: CircularProgressIndicator(
                strokeWidth: 2.4,
                color: Colors.white,
              ),
            )
          : Row(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                const Text('Login'),
                const SizedBox(width: AppSpacing.sm),
                Icon(
                  AppIcons.chevronRight,
                  size: 20,
                  color: Colors.white.withValues(alpha: 0.9),
                ),
              ],
            ),
    );
  }

  Widget _buildError(String message) {
    return Container(
      padding: const EdgeInsets.all(AppSpacing.md),
      decoration: BoxDecoration(
        color: AppColors.danger.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(AppRadius.md),
        border: Border.all(color: AppColors.danger.withValues(alpha: 0.4)),
      ),
      child: Row(
        children: [
          const Icon(AppIcons.alerts, color: AppColors.danger, size: 20),
          const SizedBox(width: AppSpacing.sm),
          Expanded(
            child: Text(
              message,
              style: const TextStyle(color: AppColors.danger, fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }
}
