import 'dart:async';

import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/constants/app_constants.dart';
import '../../core/theme/app_colors.dart';
import '../../core/theme/app_spacing.dart';
import '../../core/theme/app_typography.dart';
import '../../providers/connection_provider.dart';
import '../../providers/dashboard_provider.dart';
import '../../providers/settings_provider.dart';
import '../../providers/supabase_auth_provider.dart';
import '../../routes/app_routes.dart';
import '../../widgets/glass_card.dart';

/// Application settings screen.
///
/// Organised into General, Voice, Notifications, Theme, About and (hidden)
/// Developer sections. Networking details only appear inside the developer
/// surface, which unlocks after tapping the version repeatedly.
class SettingsScreen extends ConsumerStatefulWidget {
  const SettingsScreen({super.key});

  @override
  ConsumerState<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends ConsumerState<SettingsScreen> {
  static const _unlockTaps = 5;

  late final TextEditingController _hostController;
  late final TextEditingController _timeoutController;
  int _aboutTaps = 0;
  bool _devUnlocked = false;

  @override
  void initState() {
    super.initState();
    final settings = ref.read(settingsProvider).settings;
    _hostController = TextEditingController(text: settings.deviceHost);
    _timeoutController = TextEditingController(
      text: '${settings.requestTimeoutMs}',
    );
  }

  @override
  void dispose() {
    _hostController.dispose();
    _timeoutController.dispose();
    super.dispose();
  }

  Future<void> _applySettings() async {
    final notifier = ref.read(settingsProvider.notifier);
    final timeout = int.tryParse(_timeoutController.text.trim());
    if (timeout != null && timeout >= 500 && timeout <= 60000) {
      await notifier.updateTimeout(timeout);
    }

    final settings = ref.read(settingsProvider).settings;
    final host = _hostController.text.trim();
    if (host.isNotEmpty && host != settings.deviceHost) {
      await notifier.updateDevice(host, settings.devicePort);
      await ref
          .read(connectionProvider.notifier)
          .connect(host, settings.devicePort);
    } else {
      await ref.read(dashboardProvider.notifier).refresh();
    }

    if (mounted) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('Settings saved')));
    }
  }

  void _onAboutTapped() {
    setState(() => _aboutTaps++);
    if (_aboutTaps >= _unlockTaps && !_devUnlocked) {
      setState(() => _devUnlocked = true);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Developer settings unlocked')),
      );
    }
  }

  Future<void> _signOut() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Sign out'),
        content: const Text(
          'This clears the stored session and returns to the login screen.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Sign out'),
          ),
        ],
      ),
    );
    if (confirmed != true) {
      return;
    }
    await ref.read(connectionProvider.notifier).logout();
    if (mounted) {
      unawaited(
        Navigator.of(
          context,
        ).pushNamedAndRemoveUntil(AppRoutes.connection, (route) => false),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final settingsNotifier = ref.watch(settingsProvider);
    final settings = settingsNotifier.settings;
    final connection = ref.watch(connectionProvider);

    if (settingsNotifier.isLoading) {
      return const Center(child: CircularProgressIndicator());
    }

    return ListView(
      padding: const EdgeInsets.all(AppSpacing.page),
      children: [
        _sectionTitle('General'),
        GlassCard(
          blur: false,
          child: Padding(
            padding: const EdgeInsets.all(AppSpacing.lg),
            child: Column(
              children: [
                TextField(
                  controller: _timeoutController,
                  keyboardType: TextInputType.number,
                  decoration: const InputDecoration(
                    labelText: 'Request timeout (ms)',
                    prefixIcon: Icon(Icons.timer_outlined),
                  ),
                ),
                const SizedBox(height: AppSpacing.sm),
                SwitchListTile(
                  contentPadding: EdgeInsets.zero,
                  secondary: const Icon(Icons.sync),
                  title: const Text('Auto Reconnect'),
                  subtitle: const Text(
                    'Reconnect automatically when the link drops',
                  ),
                  value: settings.autoReconnect,
                  onChanged: (value) => ref
                      .read(settingsProvider.notifier)
                      .updateAutoReconnect(value),
                ),
                const SizedBox(height: AppSpacing.sm),
                SizedBox(
                  width: double.infinity,
                  child: FilledButton.icon(
                    onPressed: _applySettings,
                    icon: const Icon(Icons.save_outlined),
                    label: const Text('Save'),
                  ),
                ),
              ],
            ),
          ),
        ),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Voice'),
        GlassCard(
          blur: false,
          child: Padding(
            padding: const EdgeInsets.all(AppSpacing.lg),
            child: Column(
              children: [
                _sliderTile(
                  title: 'Speech rate',
                  value: settings.ttsRate,
                  min: 0.4,
                  max: 2.0,
                  display: '${settings.ttsRate.toStringAsFixed(1)}x',
                  onChanged: (value) =>
                      ref.read(settingsProvider.notifier).updateTtsRate(value),
                ),
                const Divider(height: 24),
                _sliderTile(
                  title: 'Speech pitch',
                  value: settings.ttsPitch,
                  min: 0.5,
                  max: 2.0,
                  display: settings.ttsPitch.toStringAsFixed(1),
                  onChanged: (value) =>
                      ref.read(settingsProvider.notifier).updateTtsPitch(value),
                ),
                const Divider(height: 24),
                _providerDropdown(
                  title: 'STT',
                  subtitle:
                      'Speech-to-text provider. Informational — the '
                      'device firmware decides the active engine.',
                  value: settings.speechProvider,
                  options: AppConstants.speechProviders,
                  onChanged: (value) => ref
                      .read(settingsProvider.notifier)
                      .updateSpeechProvider(value),
                ),
                const Divider(height: 24),
                _providerDropdown(
                  title: 'TTS',
                  subtitle:
                      'Text-to-speech provider. Informational — the '
                      'device firmware decides the active engine.',
                  value: settings.ttsProvider,
                  options: AppConstants.ttsProviders,
                  onChanged: (value) => ref
                      .read(settingsProvider.notifier)
                      .updateTtsProvider(value),
                ),
              ],
            ),
          ),
        ),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Notifications'),
        GlassCard(
          blur: false,
          child: Column(
            children: [
              SwitchListTile(
                secondary: const Icon(Icons.notifications_active_outlined),
                title: const Text('Device alerts'),
                subtitle: const Text('Connection loss and battery warnings'),
                value: settings.alertsEnabled,
                onChanged: (value) => ref
                    .read(settingsProvider.notifier)
                    .updateAlertsEnabled(value),
              ),
              const Divider(height: 1),
              SwitchListTile(
                secondary: const Icon(Icons.event_note),
                title: const Text('Reminder notifications'),
                subtitle: const Text(
                  'Local reminders scheduled on this device',
                ),
                value: settings.remindersEnabled,
                onChanged: (value) => ref
                    .read(settingsProvider.notifier)
                    .updateRemindersEnabled(value),
              ),
            ],
          ),
        ),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Appearance'),
        _themeSelector(settings.themeMode),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Status'),
        _statusTile(connection),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Connection'),
        _connectionTile(connection),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Account'),
        _accountTile(),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('About'),
        GlassCard(
          blur: false,
          child: Column(
            children: [
              ListTile(
                leading: const Icon(Icons.info_outline),
                title: const Text('A.U.R.A'),
                subtitle: const Text(
                  'Version ${AppConstants.appVersion} (${AppConstants.buildNumber})',
                ),
                trailing: const Icon(Icons.chevron_right),
                onTap: _onAboutTapped,
              ),
              const Divider(height: 1),
              ListTile(
                leading: const Icon(Icons.auto_awesome),
                title: const Text('About AURA'),
                trailing: const Icon(Icons.chevron_right),
                onTap: _showAbout,
              ),
              if (_devUnlocked) ...[
                const Divider(height: 1),
                ListTile(
                  leading: const Icon(Icons.code, color: AppColors.warning),
                  title: const Text('Developer'),
                  subtitle: const Text('Networking, diagnostics and session'),
                  trailing: const Icon(Icons.chevron_right),
                  onTap: () =>
                      Navigator.of(context).pushNamed(AppRoutes.developer),
                ),
              ],
            ],
          ),
        ),

        const SizedBox(height: AppSpacing.section),
        _sectionTitle('Session'),
        GlassCard(
          blur: false,
          child: ListTile(
            leading: const Icon(Icons.logout, color: AppColors.danger),
            title: const Text('Sign out'),
            subtitle: const Text('End the session and return to login'),
            trailing: const Icon(Icons.chevron_right),
            onTap: _signOut,
          ),
        ),
        const SizedBox(height: AppSpacing.xl),
      ],
    );
  }

  Widget _sliderTile({
    required String title,
    required double value,
    required double min,
    required double max,
    required String display,
    required ValueChanged<double> onChanged,
  }) {
    return Row(
      children: [
        SizedBox(
          width: 96,
          child: Text(
            title,
            style: const TextStyle(
              color: AppColors.textSecondary,
              fontSize: AppTypography.sizeMd,
            ),
          ),
        ),
        Expanded(
          child: Slider(
            value: value.clamp(min, max),
            min: min,
            max: max,
            onChanged: onChanged,
          ),
        ),
        SizedBox(
          width: 40,
          child: Text(
            display,
            textAlign: TextAlign.right,
            style: const TextStyle(
              color: AppColors.textPrimary,
              fontSize: AppTypography.sizeMd,
              fontWeight: AppTypography.semibold,
            ),
          ),
        ),
      ],
    );
  }

  Widget _providerDropdown({
    required String title,
    required String subtitle,
    required String value,
    required List<String> options,
    required ValueChanged<String> onChanged,
  }) {
    final selected = options.contains(value) ? value : options.first;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          title,
          style: const TextStyle(
            color: AppColors.textSecondary,
            fontSize: AppTypography.sizeMd,
          ),
        ),
        const SizedBox(height: AppSpacing.xs),
        DropdownButtonFormField<String>(
          initialValue: selected,
          items: [
            for (final option in options)
              DropdownMenuItem<String>(value: option, child: Text(option)),
          ],
          onChanged: (next) {
            if (next != null && next != selected) {
              onChanged(next);
            }
          },
          decoration: const InputDecoration(isDense: true),
        ),
        const SizedBox(height: AppSpacing.xs),
        Text(
          subtitle,
          style: const TextStyle(
            color: AppColors.textSecondary,
            fontSize: AppTypography.sizeSm,
          ),
        ),
      ],
    );
  }

  Widget _themeSelector(ThemeMode mode) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 8),
      child: SegmentedButton<ThemeMode>(
        segments: const [
          ButtonSegment(
            value: ThemeMode.dark,
            label: Text('Dark'),
            icon: Icon(Icons.dark_mode_outlined),
          ),
          ButtonSegment(
            value: ThemeMode.light,
            label: Text('Light'),
            icon: Icon(Icons.light_mode_outlined),
          ),
          ButtonSegment(
            value: ThemeMode.system,
            label: Text('System'),
            icon: Icon(Icons.brightness_auto),
          ),
        ],
        selected: {mode},
        onSelectionChanged: (selection) => ref
            .read(settingsProvider.notifier)
            .updateThemeMode(selection.first),
      ),
    );
  }

  Widget _statusTile(ConnectionState connection) {
    final connected = connection.isConnected;
    return GlassCard(
      blur: false,
      child: ListTile(
        leading: Icon(
          connected ? Icons.wifi : Icons.wifi_off,
          color: connected ? AppColors.success : AppColors.danger,
        ),
        title: Text(
          connection.message.isEmpty
              ? (connected ? 'Connected' : 'Disconnected')
              : connection.message,
        ),
        subtitle: Text(connection.host.isEmpty ? 'No device' : connection.host),
        trailing: TextButton(
          onPressed: connected
              ? null
              : () => ref.read(connectionProvider.notifier).reconnect(),
          child: const Text('Reconnect'),
        ),
      ),
    );
  }

  Widget _connectionTile(ConnectionState connection) {
    final remote = connection.mode == ConnectionMode.remote;
    final auth = ref.watch(supabaseAuthProvider);

    final String subtitle;
    final Widget? trailing;
    if (remote) {
      subtitle = 'Connected through AURA Cloud (Supabase)';
      trailing = TextButton(
        onPressed: () => ref.read(connectionProvider.notifier).switchToLocal(),
        child: const Text('Use local device'),
      );
    } else if (auth.isSignedIn) {
      subtitle = 'Direct LAN connection (REST + WebSocket)';
      trailing = TextButton(
        onPressed: () => ref.read(connectionProvider.notifier).switchToCloud(),
        child: const Text('Use cloud'),
      );
    } else {
      subtitle = 'Direct LAN connection. Sign in to cloud for remote access.';
      trailing = null;
    }

    return GlassCard(
      blur: false,
      child: ListTile(
        leading: Icon(
          remote ? Icons.cloud : Icons.router,
          color: remote ? AppColors.secondary : AppColors.primary,
        ),
        title: Text(remote ? 'AURA Cloud' : 'Local device'),
        subtitle: Text(subtitle),
        trailing: trailing,
      ),
    );
  }

  Widget _accountTile() {
    final auth = ref.watch(supabaseAuthProvider);

    if (!auth.isSignedIn) {
      return GlassCard(
        blur: false,
        child: ListTile(
          leading: const Icon(Icons.account_circle_outlined),
          title: const Text('Cloud account'),
          subtitle: const Text('Not signed in'),
          trailing: TextButton(
            onPressed: _signInToCloud,
            child: const Text('Sign in'),
          ),
        ),
      );
    }

    return GlassCard(
      blur: false,
      child: ListTile(
        leading: const Icon(Icons.account_circle, color: AppColors.secondary),
        title: const Text('Cloud account'),
        subtitle: Text(auth.email ?? 'Signed in'),
        trailing: TextButton(
          onPressed: _signOutOfCloud,
          child: const Text('Sign out'),
        ),
      ),
    );
  }

  Future<void> _signInToCloud() async {
    final notifier = ref.read(connectionProvider.notifier);
    await notifier.switchToCloud();
    if (!mounted) {
      return;
    }
    if (ref.read(connectionProvider).isConnected) {
      return;
    }
    unawaited(Navigator.of(context).pushNamed(AppRoutes.connection));
  }

  Future<void> _signOutOfCloud() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Sign out of cloud'),
        content: const Text(
          'This signs you out of AURA Cloud. Your local device connection '
          'is not affected.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Sign out'),
          ),
        ],
      ),
    );
    if (confirmed != true) {
      return;
    }
    await ref.read(connectionProvider.notifier).signOutCloud();
    if (mounted) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(const SnackBar(content: Text('Signed out of cloud')));
    }
  }

  void _showAbout() {
    showDialog<void>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: const Text('About'),
        content: const Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Center(
              child: Text(
                'A.U.R.A',
                style: TextStyle(
                  fontSize: 28,
                  fontWeight: FontWeight.w800,
                  letterSpacing: 6,
                  color: AppColors.primary,
                ),
              ),
            ),
            SizedBox(height: 12),
            Text(AppConstants.appDescription),
            SizedBox(height: 16),
            Text(
              'Version ${AppConstants.appVersion} '
              '(${AppConstants.buildNumber})',
              style: TextStyle(color: AppColors.textSecondary),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(),
            child: const Text('Close'),
          ),
        ],
      ),
    );
  }

  Widget _sectionTitle(String title) {
    return Padding(
      padding: const EdgeInsets.only(left: 4, bottom: AppSpacing.sm),
      child: Text(title, style: AppTypography.sectionLabel),
    );
  }
}
