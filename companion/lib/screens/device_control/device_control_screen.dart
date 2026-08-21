import 'dart:async';

import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../core/theme/app_spacing.dart';
import '../../core/theme/app_typography.dart';
import '../../core/utils/formatters.dart';
import '../../models/device_control.dart';
import '../../providers/connection_provider.dart';
import '../../providers/dashboard_provider.dart';
import '../../providers/device_control_provider.dart';
import '../../widgets/glass_card.dart';
import '../../widgets/metric_bar.dart';
import '../../widgets/status_badge.dart';

/// Device Control centre: system actions, network, OLED, LED ring, speaker and
/// microphone — backed by the V2 firmware control endpoints.
class DeviceControlScreen extends ConsumerStatefulWidget {
  const DeviceControlScreen({super.key});

  @override
  ConsumerState<DeviceControlScreen> createState() =>
      _DeviceControlScreenState();
}

class _DeviceControlScreenState extends ConsumerState<DeviceControlScreen> {
  final _textController = TextEditingController();
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
        _control!.load();
        _control!.startMicPolling();
      }
    });
  }

  @override
  void dispose() {
    _textController.dispose();
    _control?.stopMicPolling();
    super.dispose();
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

  void _toast(String message) {
    if (!mounted) return;
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  Future<void> _confirmDanger({
    required String title,
    required String message,
    required String confirmLabel,
    required Future<void> Function() action,
  }) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(title),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: Text(confirmLabel),
          ),
        ],
      ),
    );
    if (confirmed != true) return;
    await _apply(action);
    if (mounted) {
      _toast(confirmLabel);
    }
  }

  Future<void> _showWifiConnectDialog(WifiNetwork network) async {
    final passwordController = TextEditingController();
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text(network.ssid),
        content: network.isOpen
            ? const Text('This is an open network. Connect without a password?')
            : TextField(
                controller: passwordController,
                obscureText: true,
                decoration: const InputDecoration(
                  labelText: 'Password',
                  prefixIcon: Icon(Icons.lock_outline),
                ),
              ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Connect'),
          ),
        ],
      ),
    );
    passwordController.dispose();
    if (confirmed != true) return;
    await _apply(
      () => ref
          .read(deviceControlProvider.notifier)
          .connectWifi(
            ssid: network.ssid,
            password: network.isOpen ? '' : passwordController.text,
          ),
    );
    if (mounted) {
      _toast('Connecting to ${network.ssid}…');
      unawaited(ref.read(dashboardProvider.notifier).refresh());
    }
  }

  @override
  Widget build(BuildContext context) {
    final control = ref.watch(deviceControlProvider);
    final dashboard = ref.watch(dashboardProvider);
    final connection = ref.watch(connectionProvider);

    final status = dashboard.data.status;
    final device = dashboard.data.device;
    final wifi = dashboard.data.wifi;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Device Control'),
        actions: [
          const Center(
            child: StatusBadge(label: 'CONTROL', tone: BadgeTone.accent),
          ),
          const SizedBox(width: 12),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: () => ref.read(deviceControlProvider.notifier).load(),
        child: ListView(
          padding: const EdgeInsets.all(AppSpacing.page),
          children: [
            if (!_canControl) const _LocalOnlyBanner(),
            if (_canControl && control.lastError.isNotEmpty) ...[
              _ErrorBanner(
                message: control.lastError,
                onRetry: () => _fire(
                  () => ref.read(deviceControlProvider.notifier).load(),
                ),
              ),
            ],
            const SizedBox(height: AppSpacing.sm),

            _sectionTitle('System'),
            GlassCard(
              blur: false,
              child: Column(
                children: [
                  ListTile(
                    leading: const Icon(Icons.memory, color: AppColors.primary),
                    title: Text(device.name.isEmpty ? 'AURA' : device.name),
                    subtitle: Text(
                      device.version.isEmpty
                          ? 'Firmware build ${device.buildDate}'
                          : 'Firmware v${device.version}',
                    ),
                    trailing: StatusBadge(
                      label: connection.isConnected ? 'ONLINE' : 'OFFLINE',
                      tone: connection.isConnected
                          ? BadgeTone.success
                          : BadgeTone.danger,
                    ),
                  ),
                  const Divider(height: 1),
                  ListTile(
                    leading: const Icon(Icons.timer_outlined),
                    title: const Text('Uptime'),
                    subtitle: Text(Formatters.uptime(status.uptimeSeconds)),
                  ),
                  const Divider(height: 1),
                  Padding(
                    padding: const EdgeInsets.all(AppSpacing.lg),
                    child: Row(
                      children: [
                        Expanded(
                          child: FilledButton.icon(
                            onPressed: _canControl
                                ? () => _confirmDanger(
                                    title: 'Restart device',
                                    message:
                                        'AURA will reboot in about a '
                                        'second. The connection will drop '
                                        'briefly.',
                                    confirmLabel: 'Restart',
                                    action: () => ref
                                        .read(deviceControlProvider.notifier)
                                        .restartDevice(),
                                  )
                                : null,
                            icon: const Icon(Icons.restart_alt),
                            label: const Text('Restart'),
                          ),
                        ),
                        const SizedBox(width: AppSpacing.sm),
                        Expanded(
                          child: OutlinedButton.icon(
                            style: OutlinedButton.styleFrom(
                              foregroundColor: AppColors.danger,
                            ),
                            onPressed: _canControl
                                ? () => _confirmDanger(
                                    title: 'Factory reset',
                                    message:
                                        'Erases saved Wi-Fi credentials '
                                        'and the admin password. This cannot '
                                        'be undone.',
                                    confirmLabel: 'Reset',
                                    action: () => ref
                                        .read(deviceControlProvider.notifier)
                                        .factoryReset(),
                                  )
                                : null,
                            icon: const Icon(Icons.factory_outlined),
                            label: const Text('Factory reset'),
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),

            const SizedBox(height: AppSpacing.section),
            _sectionTitle('Network'),
            GlassCard(
              blur: false,
              child: Column(
                children: [
                  ListTile(
                    leading: const Icon(Icons.wifi, color: AppColors.success),
                    title: Text(
                      wifi.ssid.isEmpty ? 'Not connected' : wifi.ssid,
                    ),
                    subtitle: Text(
                      wifi.ip.isEmpty
                          ? 'No network address'
                          : '${wifi.ip} • ${wifi.signal} dBm',
                    ),
                  ),
                  const Divider(height: 1),
                  Padding(
                    padding: const EdgeInsets.all(AppSpacing.lg),
                    child: Column(
                      children: [
                        if (control.isBusy) const LinearProgressIndicator(),
                        if (control.networks.isNotEmpty) ...[
                          const SizedBox(height: AppSpacing.sm),
                          for (final network in control.networks)
                            ListTile(
                              dense: true,
                              contentPadding: EdgeInsets.zero,
                              leading: Icon(
                                network.isOpen
                                    ? Icons.wifi
                                    : Icons.lock_outline,
                                size: 20,
                              ),
                              title: Text(network.ssid),
                              subtitle: Text(
                                '${network.rssi} dBm • ch ${network.channel}',
                              ),
                              trailing: const Icon(Icons.chevron_right),
                              onTap: () => _showWifiConnectDialog(network),
                            ),
                        ],
                        Row(
                          children: [
                            Expanded(
                              child: FilledButton.tonalIcon(
                                onPressed: _canControl
                                    ? () => _fire(() async {
                                        await ref
                                            .read(
                                              deviceControlProvider.notifier,
                                            )
                                            .scanWifi();
                                        // A scan exercises the radio; on the
                                        // setup network the phone may briefly
                                        // drop/rejoin AURA_Setup, so re-probe
                                        // and recreate the live feed.
                                        await ref
                                            .read(connectionProvider.notifier)
                                            .refreshAfterScan();
                                      })
                                    : null,
                                icon: const Icon(Icons.radar),
                                label: const Text('Scan networks'),
                              ),
                            ),
                            const SizedBox(width: AppSpacing.sm),
                            Expanded(
                              child: OutlinedButton.icon(
                                onPressed: _canControl
                                    ? () => _confirmDanger(
                                        title: 'Forget Wi-Fi',
                                        message:
                                            'Removes the saved Wi-Fi '
                                            'credentials from AURA.',
                                        confirmLabel: 'Forget',
                                        action: () => ref
                                            .read(
                                              deviceControlProvider.notifier,
                                            )
                                            .forgetWifi(),
                                      )
                                    : null,
                                icon: const Icon(Icons.link_off),
                                label: const Text('Forget Wi-Fi'),
                              ),
                            ),
                          ],
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),

            const SizedBox(height: AppSpacing.section),
            _sectionTitle('OLED Display'),
            _displaySection(control),

            const SizedBox(height: AppSpacing.section),
            _sectionTitle('LED Ring'),
            _ledSection(control),

            const SizedBox(height: AppSpacing.section),
            _sectionTitle('Speaker'),
            _audioSection(),

            const SizedBox(height: AppSpacing.section),
            _sectionTitle('Microphone'),
            _micSection(control),

            const SizedBox(height: AppSpacing.xl),
          ],
        ),
      ),
    );
  }

  Widget _displaySection(DeviceControlState control) {
    final display = control.display;
    return GlassCard(
      blur: false,
      child: Column(
        children: [
          SwitchListTile(
            secondary: const Icon(Icons.visibility_outlined),
            title: const Text('Display power'),
            value: display.on,
            onChanged: _canControl
                ? (value) => _fire(
                    () => ref.read(deviceControlProvider.notifier).setDisplay({
                      'power': value,
                    }),
                  )
                : null,
          ),
          const Divider(height: 1),
          _sliderRow(
            title: 'Brightness',
            value: display.brightness.toDouble(),
            min: 0,
            max: 255,
            display: '${display.brightness}',
            onChangeEnd: (value) => _fire(
              () => ref.read(deviceControlProvider.notifier).setDisplay({
                'brightness': value.round(),
              }),
            ),
          ),
          const Divider(height: 1),
          SwitchListTile(
            secondary: const Icon(Icons.contrast),
            title: const Text('Inverted'),
            value: _inverted,
            onChanged: _canControl
                ? (value) {
                    setState(() => _inverted = value);
                    _fire(
                      () => ref.read(deviceControlProvider.notifier).setDisplay(
                        {'invert': value},
                      ),
                    );
                  }
                : null,
          ),
          const Divider(height: 1),
          ListTile(
            leading: const Icon(Icons.screen_rotation_outlined),
            title: const Text('Rotation'),
            trailing: DropdownButton<int>(
              value: _rotation,
              items: const [
                DropdownMenuItem(value: 0, child: Text('0°')),
                DropdownMenuItem(value: 1, child: Text('90°')),
                DropdownMenuItem(value: 2, child: Text('180°')),
                DropdownMenuItem(value: 3, child: Text('270°')),
              ],
              onChanged: _canControl
                  ? (value) {
                      setState(() => _rotation = value ?? _rotation);
                      _fire(
                        () => ref
                            .read(deviceControlProvider.notifier)
                            .setDisplay({'rotation': value ?? _rotation}),
                      );
                    }
                  : null,
            ),
          ),
          const Divider(height: 1),
          Padding(
            padding: const EdgeInsets.all(AppSpacing.lg),
            child: Column(
              children: [
                TextField(
                  controller: _textController,
                  enabled: _canControl,
                  onChanged: (_) => setState(() {}),
                  decoration: const InputDecoration(
                    labelText: 'Show text on OLED',
                    prefixIcon: Icon(Icons.text_fields),
                  ),
                ),
                const SizedBox(height: AppSpacing.sm),
                SizedBox(
                  width: double.infinity,
                  child: FilledButton.tonalIcon(
                    onPressed:
                        _canControl && _textController.text.trim().isNotEmpty
                        ? () => _fire(
                            () => ref
                                .read(deviceControlProvider.notifier)
                                .setDisplay({
                                  'text': _textController.text.trim(),
                                }),
                          )
                        : null,
                    icon: const Icon(Icons.visibility_outlined),
                    label: const Text('Show on screen'),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _ledSection(DeviceControlState control) {
    final led = control.led;
    final currentColor = Color.fromARGB(255, led.r, led.g, led.b);
    return GlassCard(
      blur: false,
      child: Column(
        children: [
          SwitchListTile(
            secondary: const Icon(Icons.circle, color: AppColors.primary),
            title: const Text('Ring enabled'),
            value: led.enabled,
            onChanged: _canControl
                ? (value) => _fire(
                    () => ref.read(deviceControlProvider.notifier).setLed({
                      'enabled': value,
                    }),
                  )
                : null,
          ),
          const Divider(height: 1),
          _sliderRow(
            title: 'Brightness',
            value: led.brightness.toDouble(),
            min: 0,
            max: 255,
            display: '${led.brightness}',
            onChangeEnd: (value) => _fire(
              () => ref.read(deviceControlProvider.notifier).setLed({
                'brightness': value.round(),
              }),
            ),
          ),
          const Divider(height: 1),
          ListTile(
            leading: const Icon(Icons.auto_awesome),
            title: const Text('Mood'),
            subtitle: Text(_moodLabel(led.mood)),
            trailing: DropdownButton<String>(
              value: auraMoods.contains(led.mood) ? led.mood : auraMoods.first,
              items: [
                for (final mood in auraMoods)
                  DropdownMenuItem(value: mood, child: Text(_moodLabel(mood))),
              ],
              onChanged: _canControl
                  ? (value) {
                      if (value != null) {
                        _fire(
                          () => ref.read(deviceControlProvider.notifier).setLed(
                            {'mood': value},
                          ),
                        );
                      }
                    }
                  : null,
            ),
          ),
          const Divider(height: 1),
          Padding(
            padding: const EdgeInsets.all(AppSpacing.lg),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    Icon(
                      Icons.palette_outlined,
                      color: _canControl ? currentColor : AppColors.textMuted,
                    ),
                    const SizedBox(width: 8),
                    const Text(
                      'Theme colour',
                      style: TextStyle(color: AppColors.textSecondary),
                    ),
                    const Spacer(),
                    if (led.enabled)
                      StatusBadge(label: led.mood, tone: BadgeTone.accent),
                  ],
                ),
                const SizedBox(height: AppSpacing.sm),
                Wrap(
                  spacing: 10,
                  runSpacing: 10,
                  children: [
                    for (final preset in _colorPresets)
                      _ColorDot(
                        color: preset.$2,
                        selected:
                            preset.$2.toARGB32() == currentColor.toARGB32(),
                        onTap: _canControl
                            ? () => _fire(
                                () => ref
                                    .read(deviceControlProvider.notifier)
                                    .setLed({
                                      'r': (preset.$2.r * 255).round(),
                                      'g': (preset.$2.g * 255).round(),
                                      'b': (preset.$2.b * 255).round(),
                                    }),
                              )
                            : null,
                      ),
                  ],
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _audioSection() {
    return GlassCard(
      blur: false,
      child: Column(
        children: [
          _sliderRow(
            title: 'Volume',
            value: _volume.toDouble(),
            min: 0,
            max: 100,
            display: '$_volume',
            onChanged: (value) => setState(() => _volume = value.round()),
            onChangeEnd: (value) {
              setState(() => _volume = value.round());
              _fire(
                () => ref.read(deviceControlProvider.notifier).setAudio({
                  'volume': value.round(),
                }),
              );
            },
          ),
          const Divider(height: 1),
          SwitchListTile(
            secondary: const Icon(Icons.volume_off_outlined),
            title: const Text('Mute'),
            value: _muted,
            onChanged: _canControl
                ? (value) {
                    setState(() => _muted = value);
                    _fire(
                      () => ref.read(deviceControlProvider.notifier).setAudio({
                        'mute': value,
                      }),
                    );
                  }
                : null,
          ),
          const Divider(height: 1),
          Padding(
            padding: const EdgeInsets.all(AppSpacing.lg),
            child: SizedBox(
              width: double.infinity,
              child: FilledButton.tonalIcon(
                onPressed: _canControl
                    ? () => _fire(
                        () => ref.read(deviceControlProvider.notifier).setAudio(
                          {'test': true},
                        ),
                      )
                    : null,
                icon: const Icon(Icons.graphic_eq),
                label: const Text('Play test tone'),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _micSection(DeviceControlState control) {
    final mic = control.mic;
    return GlassCard(
      blur: false,
      child: Column(
        children: [
          ListTile(
            leading: const Icon(Icons.mic, color: AppColors.primary),
            title: const Text('Live input level'),
            subtitle: Text(mic.recording ? 'Listening…' : 'Idle'),
            trailing: StatusBadge(
              label: '${(mic.energy * 100).clamp(0, 100).round()}%',
              tone: mic.energy > 0.5
                  ? BadgeTone.danger
                  : mic.energy > 0.2
                  ? BadgeTone.warning
                  : BadgeTone.success,
            ),
          ),
          Padding(
            padding: const EdgeInsets.symmetric(horizontal: AppSpacing.lg),
            child: MetricBar(fraction: (mic.energy * 1.5).clamp(0.0, 1.0)),
          ),
          const Divider(height: 1),
          _sliderRow(
            title: 'Gain',
            value: _gain.toDouble(),
            min: 0,
            max: 100,
            display: '$_gain',
            onChanged: (value) => setState(() => _gain = value.round()),
            onChangeEnd: (value) {
              setState(() => _gain = value.round());
              _fire(
                () => ref.read(deviceControlProvider.notifier).setMic({
                  'gain': value.round(),
                }),
              );
            },
          ),
          const Divider(height: 1),
          Padding(
            padding: const EdgeInsets.all(AppSpacing.lg),
            child: SizedBox(
              width: double.infinity,
              child: FilledButton.tonalIcon(
                onPressed: _canControl
                    ? () => _fire(
                        () => ref.read(deviceControlProvider.notifier).setMic({
                          'calibrate': true,
                        }),
                      )
                    : null,
                icon: const Icon(Icons.tune),
                label: const Text('Recalibrate noise floor'),
              ),
            ),
          ),
        ],
      ),
    );
  }

  int _volume = 70;
  int _gain = 60;
  bool _muted = false;
  bool _inverted = false;
  int _rotation = 0;

  String _moodLabel(String mood) {
    final parts = mood.split('_');
    return parts
        .map(
          (part) =>
              part.isEmpty ? part : part[0].toUpperCase() + part.substring(1),
        )
        .join(' ');
  }

  Widget _sliderRow({
    required String title,
    required double value,
    required double min,
    required double max,
    required String display,
    required ValueChanged<double>? onChangeEnd,
    ValueChanged<double>? onChanged,
  }) {
    return Padding(
      padding: const EdgeInsets.all(AppSpacing.lg),
      child: Row(
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
              onChanged: _canControl ? (onChanged ?? (value) {}) : null,
              onChangeEnd: _canControl ? onChangeEnd : null,
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

const _colorPresets = <(String, Color)>[
  ('White', Color(0xFFFFFFFF)),
  ('Cyan', Color(0xFF00E5FF)),
  ('Green', Color(0xFF00E676)),
  ('Amber', Color(0xFFFFAB00)),
  ('Magenta', Color(0xFFFF4081)),
  ('Blue', Color(0xFF448AFF)),
  ('Red', Color(0xFFFF5252)),
];

class _ColorDot extends StatelessWidget {
  const _ColorDot({required this.color, required this.selected, this.onTap});

  final Color color;
  final bool selected;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(24),
      child: Container(
        width: 36,
        height: 36,
        decoration: BoxDecoration(
          color: color,
          shape: BoxShape.circle,
          border: Border.all(
            color: selected ? Colors.white : Colors.black26,
            width: selected ? 3 : 1,
          ),
        ),
        child: selected ? const Icon(Icons.check, size: 18) : null,
      ),
    );
  }
}

class _ErrorBanner extends StatelessWidget {
  const _ErrorBanner({required this.message, required this.onRetry});

  final String message;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(AppSpacing.lg),
      decoration: BoxDecoration(
        color: AppColors.danger.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AppColors.danger.withValues(alpha: 0.5)),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: AppColors.danger),
          const SizedBox(width: 12),
          Expanded(
            child: Text(
              message.length > 160 ? '${message.substring(0, 160)}…' : message,
              style: const TextStyle(color: AppColors.textSecondary),
            ),
          ),
          TextButton(onPressed: onRetry, child: const Text('Retry')),
        ],
      ),
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
