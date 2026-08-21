import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/constants/api_paths.dart';
import '../../core/services/logger.dart';
import '../../core/theme/app_colors.dart';
import '../../core/theme/app_spacing.dart';
import '../../core/theme/app_typography.dart';
import '../../providers/app_providers.dart';
import '../../providers/connection_provider.dart';
import '../../providers/settings_provider.dart';
import '../../routes/app_routes.dart';
import '../../widgets/glass_card.dart';

/// Developer-only settings surface.
///
/// Contains every networking detail (host, REST port, WebSocket port),
/// reconnect tuning, diagnostics, log export and session control. It is not
/// reachable from the normal settings flow — only after unlocking the
/// developer section in About.
class DeveloperScreen extends ConsumerStatefulWidget {
  const DeveloperScreen({super.key});

  @override
  ConsumerState<DeveloperScreen> createState() => _DeveloperScreenState();
}

class _DeveloperScreenState extends ConsumerState<DeveloperScreen> {
  late final TextEditingController _hostController;
  late final TextEditingController _restPortController;
  late final TextEditingController _wsPortController;
  late final TextEditingController _reconnectDelayController;

  bool _isSaving = false;
  bool _isBusy = false;

  @override
  void initState() {
    super.initState();
    final settings = ref.read(settingsProvider).settings;
    _hostController = TextEditingController(text: settings.deviceHost);
    _restPortController = TextEditingController(text: '${settings.devicePort}');
    _wsPortController = TextEditingController(
      text: '${settings.webSocketPort}',
    );
    _reconnectDelayController = TextEditingController(
      text: '${settings.reconnectDelayMs}',
    );
  }

  @override
  void dispose() {
    _hostController.dispose();
    _restPortController.dispose();
    _wsPortController.dispose();
    _reconnectDelayController.dispose();
    super.dispose();
  }

  Future<void> _saveDevice() async {
    final host = _hostController.text.trim();
    final restPort = int.tryParse(_restPortController.text.trim());
    final wsPort = int.tryParse(_wsPortController.text.trim());
    if (host.isEmpty || restPort == null || wsPort == null) {
      _showMessage('Enter a valid host and ports');
      return;
    }
    setState(() => _isSaving = true);
    await ref.read(settingsProvider.notifier).updateDevice(host, restPort);
    await ref.read(settingsProvider.notifier).updateWebSocketPort(wsPort);
    await ref.read(connectionProvider.notifier).reconnect();
    if (mounted) {
      setState(() => _isSaving = false);
      _showMessage('Device settings saved and reconnected');
    }
  }

  Future<void> _saveReconnectDelay() async {
    final delay = int.tryParse(_reconnectDelayController.text.trim());
    if (delay == null || delay < 100) {
      _showMessage('Reconnect delay must be at least 100 ms');
      return;
    }
    await ref.read(settingsProvider.notifier).updateReconnectDelay(delay);
    _showMessage('Reconnect delay updated');
  }

  Future<void> _runDiagnostics() async {
    setState(() => _isBusy = true);
    try {
      final json = await ref
          .read(apiServiceProvider)
          .getJson(ApiPaths.developer);
      if (!mounted) {
        return;
      }
      setState(() => _isBusy = false);
      await _showTextDialog('Diagnostics', _pretty(json));
    } catch (error) {
      _fail(error);
    }
  }

  Future<void> _exportLogs() async {
    setState(() => _isBusy = true);
    try {
      final text = await ref
          .read(deviceRepositoryProvider)
          .fetchDeveloperExport();
      if (!mounted) {
        return;
      }
      setState(() => _isBusy = false);
      await _showTextDialog(
        'Exported Logs',
        text.isEmpty ? 'No logs available.' : text,
      );
    } catch (error) {
      _fail(error);
    }
  }

  Future<void> _signOut() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Sign out'),
        content: const Text(
          'This clears the stored session on this device and returns to the '
          'login screen.',
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

  Future<void> _showTextDialog(String title, String body) {
    return showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(title),
        content: SizedBox(
          width: 560,
          child: SingleChildScrollView(
            child: SelectableText(
              body,
              style: const TextStyle(fontSize: 12, height: 1.5),
            ),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Close'),
          ),
          TextButton.icon(
            onPressed: () {
              Clipboard.setData(ClipboardData(text: body));
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(content: Text('Copied to clipboard')),
              );
            },
            icon: const Icon(Icons.copy, size: 18),
            label: const Text('Copy'),
          ),
        ],
      ),
    );
  }

  void _fail(Object error) {
    Logger.warning('Developer action failed: $error');
    if (mounted) {
      setState(() => _isBusy = false);
      _showMessage('Action failed: $error');
    }
  }

  void _showMessage(String message) {
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  String _pretty(Map<String, dynamic> json) {
    final buffer = StringBuffer();
    void walk(Map<String, dynamic> map, String indent) {
      map.forEach((key, value) {
        if (value is Map<String, dynamic>) {
          buffer.writeln('$indent$key:');
          walk(value, '$indent  ');
        } else {
          buffer.writeln('$indent$key: $value');
        }
      });
    }

    walk(json, '');
    return buffer.toString();
  }

  @override
  Widget build(BuildContext context) {
    final loggerEnabled = Logger.enabled;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Developer Settings'),
        actions: const [
          Padding(
            padding: EdgeInsets.only(right: 16),
            child: Center(
              child: Icon(Icons.code, size: 20, color: AppColors.textMuted),
            ),
          ),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(AppSpacing.page),
        children: [
          _sectionTitle('Device'),
          GlassCard(
            blur: false,
            child: Padding(
              padding: const EdgeInsets.all(AppSpacing.lg),
              child: Column(
                children: [
                  TextField(
                    controller: _hostController,
                    decoration: const InputDecoration(
                      labelText: 'Host',
                      prefixIcon: Icon(Icons.router),
                      hintText: '192.168.4.1',
                    ),
                  ),
                  const SizedBox(height: AppSpacing.md),
                  TextField(
                    controller: _restPortController,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'REST Port',
                      prefixIcon: Icon(Icons.http),
                    ),
                  ),
                  const SizedBox(height: AppSpacing.md),
                  TextField(
                    controller: _wsPortController,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'WebSocket Port',
                      prefixIcon: Icon(Icons.hub_outlined),
                    ),
                  ),
                  const SizedBox(height: AppSpacing.lg),
                  SizedBox(
                    width: double.infinity,
                    child: FilledButton.icon(
                      onPressed: _isSaving ? null : _saveDevice,
                      icon: const Icon(Icons.save_outlined),
                      label: Text(_isSaving ? 'Saving…' : 'Save & Reconnect'),
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: AppSpacing.section),
          _sectionTitle('Connection'),
          GlassCard(
            blur: false,
            child: Padding(
              padding: const EdgeInsets.all(AppSpacing.lg),
              child: Column(
                children: [
                  TextField(
                    controller: _reconnectDelayController,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'Reconnect delay (ms)',
                      prefixIcon: Icon(Icons.timer_outlined),
                      suffixIcon: Icon(Icons.check_circle_outline),
                    ),
                  ),
                  const SizedBox(height: AppSpacing.sm),
                  Align(
                    alignment: Alignment.centerRight,
                    child: TextButton.icon(
                      onPressed: _saveReconnectDelay,
                      icon: const Icon(Icons.save_outlined, size: 18),
                      label: const Text('Apply'),
                    ),
                  ),
                  Row(
                    children: [
                      const Expanded(
                        child: Text(
                          'Logging',
                          style: TextStyle(
                            color: AppColors.textSecondary,
                            fontSize: AppTypography.sizeMd,
                          ),
                        ),
                      ),
                      Switch(
                        value: loggerEnabled,
                        onChanged: (value) {
                          Logger.enabled = value;
                          setState(() {});
                        },
                      ),
                    ],
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: AppSpacing.section),
          _sectionTitle('Diagnostics'),
          GlassCard(
            blur: false,
            child: Padding(
              padding: const EdgeInsets.all(AppSpacing.lg),
              child: Row(
                children: [
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _isBusy ? null : _runDiagnostics,
                      icon: const Icon(Icons.bug_report_outlined),
                      label: const Text('Run diagnostics'),
                    ),
                  ),
                  const SizedBox(width: AppSpacing.md),
                  Expanded(
                    child: OutlinedButton.icon(
                      onPressed: _isBusy ? null : _exportLogs,
                      icon: const Icon(Icons.ios_share),
                      label: const Text('Export logs'),
                    ),
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: AppSpacing.section),
          _sectionTitle('Session'),
          GlassCard(
            blur: false,
            child: ListTile(
              leading: const Icon(Icons.logout, color: AppColors.danger),
              title: const Text('Sign out'),
              subtitle: const Text(
                'Clear the stored session and return to login',
              ),
              trailing: const Icon(Icons.chevron_right),
              onTap: _signOut,
            ),
          ),
          const SizedBox(height: AppSpacing.xl),
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
