import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../core/utils/formatters.dart';
import '../../models/dashboard_data.dart';
import '../../providers/cloud_provider.dart';
import '../../providers/connection_provider.dart';
import '../../providers/dashboard_provider.dart';
import '../../providers/supabase_auth_provider.dart';
import '../../widgets/glass_card.dart';
import '../../widgets/metric_bar.dart';
import '../../widgets/metric_card.dart';
import '../../widgets/status_badge.dart';

/// Live device overview screen.
class DashboardScreen extends ConsumerStatefulWidget {
  const DashboardScreen({super.key});

  @override
  ConsumerState<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends ConsumerState<DashboardScreen>
    with AutomaticKeepAliveClientMixin {
  @override
  bool get wantKeepAlive => true;

  @override
  Widget build(BuildContext context) {
    super.build(context);
    final dashboard = ref.watch(dashboardProvider);
    final connection = ref.watch(connectionProvider);

    if (connection.mode == ConnectionMode.remote) {
      return const _RemoteDashboard();
    }

    return RefreshIndicator(
      onRefresh: () => ref.read(dashboardProvider.notifier).refresh(),
      child: CustomScrollView(
        physics: const AlwaysScrollableScrollPhysics(),
        slivers: [
          SliverToBoxAdapter(child: _Header(connection: connection)),
          if (dashboard.isLoading)
            const SliverFillRemaining(
              hasScrollBody: false,
              child: Center(child: CircularProgressIndicator()),
            )
          else if (dashboard.error != null)
            SliverFillRemaining(
              hasScrollBody: false,
              child: _ErrorView(
                message: dashboard.error!,
                onRetry: () => ref.read(dashboardProvider.notifier).refresh(),
              ),
            )
          else ...[
            if (dashboard.data.status.headless)
              SliverToBoxAdapter(
                child: _HeadlessBanner(mode: dashboard.data.status.mode),
              ),
            SliverToBoxAdapter(
              child: _DeviceCard(
                data: dashboard.data,
                lastSeen: dashboard.lastUpdated,
              ),
            ),
            SliverToBoxAdapter(child: _StatusRow(data: dashboard.data)),
            if (dashboard.data.status.modules.isNotEmpty)
              SliverPadding(
                padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
                sliver: SliverToBoxAdapter(
                  child: _ModuleStatusCard(data: dashboard.data),
                ),
              ),
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
              sliver: SliverToBoxAdapter(
                child: _ResourceGauges(data: dashboard.data),
              ),
            ),
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(16, 8, 16, 24),
              sliver: SliverToBoxAdapter(
                child: _MetricsGrid(
                  data: dashboard.data,
                  lastUpdated: dashboard.lastUpdated,
                ),
              ),
            ),
          ],
        ],
      ),
    );
  }
}

class _Header extends StatelessWidget {
  const _Header({required this.connection});

  final ConnectionState connection;

  @override
  Widget build(BuildContext context) {
    final (tone, label) = switch (connection.phase) {
      ConnectionPhase.connected => (BadgeTone.success, 'Device Online'),
      ConnectionPhase.reconnecting => (BadgeTone.warning, 'Reconnecting'),
      ConnectionPhase.error => (BadgeTone.danger, 'Device Offline'),
      _ => (BadgeTone.accent, 'Connecting…'),
    };

    return Padding(
      padding: const EdgeInsets.fromLTRB(20, 16, 20, 8),
      child: Row(
        children: [
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'AURA Device',
                  style: Theme.of(context).textTheme.headlineMedium,
                ),
                const SizedBox(height: 4),
                Text(
                  connection.message.isEmpty
                      ? 'Dashboard overview'
                      : connection.message,
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
              ],
            ),
          ),
          StatusBadge(label: label, tone: tone, icon: Icons.circle),
        ],
      ),
    );
  }
}

class _DeviceCard extends StatelessWidget {
  const _DeviceCard({required this.data, this.lastSeen});

  final DashboardData data;
  final DateTime? lastSeen;

  @override
  Widget build(BuildContext context) {
    final device = data.device;
    final wifi = data.wifi;

    final identityParts = <String>[
      if (device.mark.isNotEmpty) 'Mark ${device.mark}',
      if (device.codename.isNotEmpty) device.codename,
    ];
    final identity = identityParts.join(' · ');
    final signalTone = wifi.connected
        ? (wifi.signal >= -70
              ? BadgeTone.success
              : wifi.signal >= -85
              ? BadgeTone.warning
              : BadgeTone.danger)
        : BadgeTone.neutral;

    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 0),
      child: GlassCard(
        padding: const EdgeInsets.all(18),
        child: Row(
          children: [
            Container(
              width: 52,
              height: 52,
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                  colors: [AppColors.primary, AppColors.tertiary],
                ),
                borderRadius: BorderRadius.circular(16),
              ),
              child: const Icon(
                Icons.developer_board,
                color: Colors.white,
                size: 28,
              ),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: Text(
                          device.modelLabel,
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                          style: const TextStyle(
                            color: AppColors.textPrimary,
                            fontSize: 16,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ),
                      const SizedBox(width: 8),
                      StatusBadge(
                        label: 'v${device.version}',
                        tone: BadgeTone.accent,
                        icon: Icons.memory,
                      ),
                    ],
                  ),
                  if (identity.isNotEmpty) ...[
                    const SizedBox(height: 2),
                    Text(
                      identity,
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(
                        color: AppColors.textSecondary,
                        fontSize: 12,
                      ),
                    ),
                  ],
                  const SizedBox(height: 8),
                  Wrap(
                    spacing: 8,
                    runSpacing: 6,
                    crossAxisAlignment: WrapCrossAlignment.center,
                    children: [
                      StatusBadge(
                        label: wifi.connected ? wifi.ssid : 'WiFi Off',
                        tone: signalTone,
                        icon: Icons.wifi,
                      ),
                      StatusBadge(
                        label: '${wifi.signal} dBm',
                        tone: signalTone,
                        icon: Icons.signal_cellular_alt,
                      ),
                      if (device.channel.isNotEmpty)
                        StatusBadge(
                          label: device.channel,
                          icon: Icons.rocket_launch_outlined,
                        ),
                      if (device.chip.isNotEmpty)
                        StatusBadge(label: device.chip, icon: Icons.memory),
                      if (lastSeen != null)
                        StatusBadge(
                          label: 'Seen ${Formatters.timestamp(lastSeen!)}',
                          icon: Icons.schedule,
                        ),
                    ],
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _StatusRow extends StatelessWidget {
  const _StatusRow({required this.data});

  final DashboardData data;

  @override
  Widget build(BuildContext context) {
    final status = data.status;
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 8, 16, 0),
      child: Wrap(
        spacing: 8,
        runSpacing: 8,
        children: [
          StatusBadge(
            label: status.running ? 'System Running' : 'System Standby',
            tone: status.running ? BadgeTone.success : BadgeTone.warning,
            icon: Icons.memory,
          ),
          StatusBadge(
            label: data.wifi.connected ? 'WiFi Connected' : 'WiFi Off',
            tone: data.wifi.connected ? BadgeTone.success : BadgeTone.neutral,
            icon: Icons.wifi,
          ),
          StatusBadge(
            label: 'Uptime ${Formatters.uptime(status.uptimeSeconds)}',
            tone: BadgeTone.accent,
            icon: Icons.timer_outlined,
          ),
          StatusBadge(
            label: 'Requests ${status.requestCount}',
            icon: Icons.swap_vert,
          ),
        ],
      ),
    );
  }
}

class _HeadlessBanner extends StatelessWidget {
  const _HeadlessBanner({required this.mode});

  final String mode;

  @override
  Widget build(BuildContext context) {
    final label = mode == 'forced'
        ? 'Headless Mode (forced by config)'
        : mode == 'auto'
        ? 'Headless Mode (no display detected)'
        : 'Headless Mode';
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 12, 16, 0),
      child: Container(
        width: double.infinity,
        padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
        decoration: BoxDecoration(
          gradient: const LinearGradient(
            colors: [AppColors.warning, AppColors.tertiary],
          ),
          borderRadius: BorderRadius.circular(14),
        ),
        child: Row(
          children: [
            const Icon(Icons.developer_mode, color: Colors.white, size: 22),
            const SizedBox(width: 10),
            Expanded(
              child: Text(
                label,
                style: const TextStyle(
                  color: Colors.white,
                  fontWeight: FontWeight.w700,
                  fontSize: 13,
                ),
              ),
            ),
            const Icon(
              Icons.laptop_chromebook,
              color: Colors.white70,
              size: 20,
            ),
          ],
        ),
      ),
    );
  }
}

class _ModuleStatusCard extends StatelessWidget {
  const _ModuleStatusCard({required this.data});

  final DashboardData data;

  @override
  Widget build(BuildContext context) {
    final status = data.status;
    final entries = status.modules.entries.toList();
    entries.sort((a, b) => a.key.compareTo(b.key));

    return GlassCard(
      blur: false,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text(
                'Module status',
                style: Theme.of(context).textTheme.titleMedium,
              ),
              const Spacer(),
              const Text(
                'ONLINE / OFFLINE / DISABLED / ERROR',
                style: TextStyle(color: AppColors.textMuted, fontSize: 11),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [for (final entry in entries) _moduleBadge(entry)],
          ),
        ],
      ),
    );
  }

  Widget _moduleBadge(MapEntry<String, String> entry) {
    final (label, tone, icon) = switch (entry.value) {
      'ONLINE' => (entry.key, BadgeTone.success, Icons.check_circle),
      'ERROR' => (entry.key, BadgeTone.danger, Icons.error_outline),
      'DISABLED' => (entry.key, BadgeTone.neutral, Icons.block),
      'OFFLINE' => (entry.key, BadgeTone.neutral, Icons.remove_circle_outline),
      _ => (entry.key, BadgeTone.neutral, Icons.help_outline),
    };
    return StatusBadge(label: label, tone: tone, icon: icon);
  }
}

class _MetricsGrid extends StatelessWidget {
  const _MetricsGrid({required this.data, this.lastUpdated});

  final DashboardData data;
  final DateTime? lastUpdated;

  @override
  Widget build(BuildContext context) {
    final perf = data.performance;
    final wifi = data.wifi;
    final storageFraction = perf.storageTotal > 0
        ? perf.storageUsed / perf.storageTotal
        : 0.0;
    final batteryInfo = perf.battery > 0
        ? '${perf.battery.toStringAsFixed(0)}%'
        : 'N/A';
    final temperature = perf.temperature > 0
        ? '${perf.temperature.toStringAsFixed(1)}°C'
        : 'N/A';

    final children = <Widget>[
      MetricCard(
        icon: Icons.devices,
        label: 'Device',
        value: data.device.name,
        subtitle: 'v${data.device.version}',
        accent: AppColors.tertiary,
      ),
      MetricCard(
        icon: Icons.wifi,
        label: 'WiFi',
        value: wifi.connected ? wifi.ssid : 'Not connected',
        subtitle: wifi.ip.isEmpty ? null : 'IP ${wifi.ip}',
        accent: AppColors.secondary,
      ),
      MetricCard(
        icon: Icons.signal_cellular_alt,
        label: 'Signal',
        value: '${wifi.signal} dBm',
        subtitle: Formatters.signalLabel(wifi.signal),
      ),
      MetricCard(
        icon: Icons.memory,
        label: 'RAM',
        value: Formatters.bytes(perf.freeHeap),
        subtitle: 'Free heap',
      ),
      MetricCard(
        icon: Icons.speed,
        label: 'CPU',
        value: '${perf.cpuUsage.toStringAsFixed(0)}%',
        subtitle: '${perf.cpuMhz.toStringAsFixed(0)} MHz',
        accent: AppColors.tertiary,
      ),
      MetricCard(
        icon: Icons.storage,
        label: 'Storage',
        value: '${(storageFraction * 100).toStringAsFixed(0)}%',
        subtitle:
            '${Formatters.bytes(perf.storageUsed)} of '
            '${Formatters.bytes(perf.storageTotal)}',
        accent: AppColors.success,
      ),
      MetricCard(
        icon: Icons.thermostat,
        label: 'Temperature',
        value: temperature,
        accent: AppColors.warning,
      ),
      MetricCard(
        icon: Icons.battery_charging_full,
        label: 'Battery',
        value: batteryInfo,
        accent: AppColors.success,
      ),
      MetricCard(
        icon: Icons.sync,
        label: 'Last Sync',
        value: lastUpdated == null ? '—' : Formatters.timestamp(lastUpdated!),
        accent: AppColors.secondary,
      ),
    ];

    return LayoutBuilder(
      builder: (context, constraints) {
        final columns = constraints.maxWidth >= 1100
            ? 4
            : constraints.maxWidth >= 700
            ? 3
            : 2;
        return GridView.builder(
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: columns,
            crossAxisSpacing: 12,
            mainAxisSpacing: 12,
            mainAxisExtent: 132,
          ),
          itemCount: children.length,
          itemBuilder: (context, index) => children[index],
        );
      },
    );
  }
}

class _ResourceGauges extends StatelessWidget {
  const _ResourceGauges({required this.data});

  final DashboardData data;

  @override
  Widget build(BuildContext context) {
    final perf = data.performance;
    final storageFraction = perf.storageTotal > 0
        ? perf.storageUsed / perf.storageTotal
        : 0.0;
    final batteryFraction = perf.battery > 0 ? perf.battery / 100 : 0.0;

    return LayoutBuilder(
      builder: (context, constraints) {
        final columns = constraints.maxWidth >= 900 ? 4 : 2;
        final gauges = [
          _GaugeTile(
            label: 'CPU',
            value: '${perf.cpuUsage.toStringAsFixed(0)}%',
            fraction: perf.cpuUsage / 100,
            color: AppColors.primary,
          ),
          _GaugeTile(
            label: 'RAM',
            value: Formatters.bytes(perf.freeHeap),
            fraction: _ramFraction(perf.freeHeap),
            color: AppColors.tertiary,
          ),
          _GaugeTile(
            label: 'Storage',
            value: '${(storageFraction * 100).toStringAsFixed(0)}%',
            fraction: storageFraction,
            color: AppColors.success,
          ),
          _GaugeTile(
            label: 'Battery',
            value: perf.battery > 0
                ? '${perf.battery.toStringAsFixed(0)}%'
                : 'N/A',
            fraction: batteryFraction,
            color: AppColors.secondary,
          ),
        ];

        return GridView.builder(
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
            crossAxisCount: columns,
            crossAxisSpacing: 12,
            mainAxisSpacing: 12,
            mainAxisExtent: 96,
          ),
          itemCount: gauges.length,
          itemBuilder: (context, index) => gauges[index],
        );
      },
    );
  }

  /// Heuristic RAM usage based on free heap (ESP32 ~320 KB usable).
  double _ramFraction(int freeHeap) {
    const total = 320 * 1024;
    final used = total - freeHeap;
    return (used / total).clamp(0.0, 1.0).toDouble();
  }
}

class _GaugeTile extends StatelessWidget {
  const _GaugeTile({
    required this.label,
    required this.value,
    required this.fraction,
    required this.color,
  });

  final String label;
  final String value;
  final double fraction;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return GlassCard(
      blur: false,
      padding: const EdgeInsets.all(14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          Row(
            children: [
              Text(
                label,
                style: const TextStyle(
                  color: AppColors.textSecondary,
                  fontSize: 12,
                  fontWeight: FontWeight.w600,
                ),
              ),
              const Spacer(),
              Text(
                value,
                style: const TextStyle(
                  color: AppColors.textPrimary,
                  fontSize: 13,
                  fontWeight: FontWeight.w700,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          MetricBar(fraction: fraction, color: color),
        ],
      ),
    );
  }
}

class _ErrorView extends StatelessWidget {
  const _ErrorView({required this.message, required this.onRetry});

  final String message;
  final VoidCallback onRetry;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Icon(Icons.cloud_off, color: AppColors.danger, size: 48),
            const SizedBox(height: 16),
            Text(
              message,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: onRetry,
              icon: const Icon(Icons.refresh),
              label: const Text('Retry'),
            ),
          ],
        ),
      ),
    );
  }
}

/// Dashboard shown while the app is connected through AURA Cloud.
///
/// The device is out of range, so live metrics are unavailable. The view
/// shows the user's registered cloud devices and offers a way back to the
/// LAN connection.
class _RemoteDashboard extends ConsumerWidget {
  const _RemoteDashboard();

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final auth = ref.watch(supabaseAuthProvider);
    final devices = ref.watch(cloudDevicesProvider);

    return RefreshIndicator(
      onRefresh: () async => ref.invalidate(cloudDevicesProvider),
      child: ListView(
        physics: const AlwaysScrollableScrollPhysics(),
        padding: const EdgeInsets.all(16),
        children: [
          GlassCard(
            padding: const EdgeInsets.all(18),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Icon(Icons.cloud, color: AppColors.secondary),
                    const SizedBox(width: 10),
                    Text(
                      'Remote mode',
                      style: Theme.of(context).textTheme.titleMedium,
                    ),
                    const Spacer(),
                    const StatusBadge(
                      label: 'Cloud',
                      tone: BadgeTone.accent,
                      icon: Icons.cloud_done_outlined,
                    ),
                  ],
                ),
                const SizedBox(height: 10),
                Text(
                  auth.isSignedIn
                      ? 'Signed in as ${auth.email ?? ''}'
                      : 'Not signed in',
                  style: Theme.of(context).textTheme.bodyMedium,
                ),
                const SizedBox(height: 6),
                const Text(
                  'Your AURA device is out of range, so live metrics are '
                  'unavailable. Bring the device onto your network to see '
                  'real-time data, or manage it through your cloud account.',
                  style: TextStyle(
                    color: AppColors.textSecondary,
                    fontSize: 13,
                  ),
                ),
                const SizedBox(height: 16),
                FilledButton.tonalIcon(
                  onPressed: () =>
                      ref.read(connectionProvider.notifier).switchToLocal(),
                  icon: const Icon(Icons.devices_other),
                  label: const Text('Try local device'),
                ),
              ],
            ),
          ),
          const SizedBox(height: 20),
          Text('Your devices', style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 10),
          ..._buildDeviceRows(context, devices),
        ],
      ),
    );
  }

  List<Widget> _buildDeviceRows(
    BuildContext context,
    AsyncValue<List<CloudDevice>> devices,
  ) {
    if (devices.isLoading) {
      return const [
        Center(
          child: Padding(
            padding: EdgeInsets.all(24),
            child: CircularProgressIndicator(),
          ),
        ),
      ];
    }
    final list = devices.value ?? const <CloudDevice>[];
    if (list.isEmpty) {
      return [
        GlassCard(
          padding: const EdgeInsets.all(18),
          child: Column(
            children: [
              const Icon(
                Icons.devices_other,
                color: AppColors.textMuted,
                size: 40,
              ),
              const SizedBox(height: 10),
              Text(
                'No devices registered',
                style: Theme.of(context).textTheme.titleSmall,
              ),
              const SizedBox(height: 4),
              const Text(
                'Add your first AURA device to this account to keep it in sync.',
                textAlign: TextAlign.center,
                style: TextStyle(color: AppColors.textSecondary, fontSize: 13),
              ),
            ],
          ),
        ),
      ];
    }
    return list.map((device) => _CloudDeviceTile(device: device)).toList();
  }
}

class _CloudDeviceTile extends StatelessWidget {
  const _CloudDeviceTile({required this.device});

  final CloudDevice device;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 8),
      child: GlassCard(
        blur: false,
        padding: const EdgeInsets.all(14),
        child: Row(
          children: [
            const Icon(Icons.developer_board, color: AppColors.primary),
            const SizedBox(width: 12),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    device.name,
                    style: const TextStyle(
                      color: AppColors.textPrimary,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                  const SizedBox(height: 2),
                  Text(
                    device.mark.isNotEmpty
                        ? 'Mark ${device.mark} · ${device.codename}'
                        : device.deviceId,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(
                      color: AppColors.textSecondary,
                      fontSize: 12,
                    ),
                  ),
                ],
              ),
            ),
            StatusBadge(
              label: device.isOnline ? 'Online' : 'Offline',
              tone: device.isOnline ? BadgeTone.success : BadgeTone.neutral,
              icon: device.isOnline ? Icons.circle : Icons.circle_outlined,
            ),
          ],
        ),
      ),
    );
  }
}
