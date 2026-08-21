import 'dart:async';

import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../core/utils/formatters.dart';
import '../../models/performance_metrics.dart';
import '../../models/system_status.dart';
import '../../providers/connection_provider.dart';
import '../../providers/dashboard_provider.dart';
import '../../widgets/glass_card.dart';
import '../../widgets/metric_bar.dart';
import '../../widgets/metric_card.dart';
import '../../widgets/status_badge.dart';

/// Live system monitor with auto-refreshing device metrics.
class SystemScreen extends ConsumerStatefulWidget {
  const SystemScreen({super.key});

  @override
  ConsumerState<SystemScreen> createState() => _SystemScreenState();
}

class _SystemScreenState extends ConsumerState<SystemScreen> {
  Timer? _timer;

  /// Only poll the live device when actually connected to it locally; avoids
  /// triggering the auto remote-fallback path while browsing other screens.
  bool _isLocalConnected() {
    final connection = ref.read(connectionProvider);
    return connection.isConnected && connection.mode == ConnectionMode.local;
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_isLocalConnected()) {
        ref.read(dashboardProvider.notifier).refresh();
      }
    });
    _timer = Timer.periodic(const Duration(seconds: 3), (_) {
      if (_isLocalConnected()) {
        ref.read(dashboardProvider.notifier).refresh();
      }
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(dashboardProvider);
    final metrics = state.data.performance;

    return Scaffold(
      appBar: AppBar(
        title: const Text('System Monitor'),
        actions: [
          const Center(
            child: StatusBadge(label: 'LIVE', tone: BadgeTone.success),
          ),
          const SizedBox(width: 12),
        ],
      ),
      body: state.isLoading && state.data.performance.freeHeap == 0
          ? const Center(child: CircularProgressIndicator())
          : RefreshIndicator(
              onRefresh: () => ref.read(dashboardProvider.notifier).refresh(),
              child: ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  _Header(metrics: metrics),
                  const SizedBox(height: 16),
                  GridView.count(
                    crossAxisCount: _columns(MediaQuery.sizeOf(context).width),
                    shrinkWrap: true,
                    physics: const NeverScrollableScrollPhysics(),
                    mainAxisSpacing: 12,
                    crossAxisSpacing: 12,
                    children: [
                      MetricCard(
                        icon: Icons.speed,
                        label: 'CPU',
                        value:
                            '${metrics.cpuUsage.toStringAsFixed(0)}%'
                            ' @ ${metrics.cpuMhz.toStringAsFixed(0)} MHz',
                      ),
                      MetricCard(
                        icon: Icons.memory,
                        label: 'Heap',
                        value: Formatters.bytes(metrics.freeHeap),
                        subtitle: 'Free heap',
                      ),
                      MetricCard(
                        icon: Icons.thermostat,
                        label: 'Temperature',
                        value: '${metrics.temperature.toStringAsFixed(1)} °C',
                      ),
                      MetricCard(
                        icon: Icons.battery_std,
                        label: 'Battery',
                        value: metrics.battery > 0
                            ? '${metrics.battery.toStringAsFixed(0)}%'
                            : '—',
                      ),
                      MetricCard(
                        icon: Icons.wifi,
                        label: 'WiFi',
                        value: '${metrics.wifiRssi} dBm',
                        subtitle: Formatters.signalLabel(metrics.wifiRssi),
                      ),
                      MetricCard(
                        icon: Icons.storage,
                        label: 'Storage',
                        value: metrics.storageTotal > 0
                            ? '${Formatters.bytes(metrics.storageUsed)} / '
                                  '${Formatters.bytes(metrics.storageTotal)}'
                            : '—',
                      ),
                    ],
                  ),
                  const SizedBox(height: 20),
                  _GaugeSection(
                    title: 'Metrics',
                    children: [
                      _Gauge(
                        label: 'CPU usage',
                        fraction: metrics.cpuUsage / 100,
                      ),
                      _Gauge(
                        label: 'Fragmentation',
                        fraction: metrics.fragmentationPercent / 100,
                        color: AppColors.warning,
                      ),
                      _Gauge(
                        label: 'Battery',
                        fraction: metrics.battery / 100,
                        color: AppColors.success,
                      ),
                      _Gauge(
                        label: 'Storage',
                        fraction: metrics.storageTotal > 0
                            ? metrics.storageUsed / metrics.storageTotal
                            : 0,
                        color: AppColors.accentGlow,
                      ),
                    ],
                  ),
                  const SizedBox(height: 20),
                  _ModuleStatusSection(status: state.data.status),
                ],
              ),
            ),
    );
  }

  int _columns(double width) => width >= 900 ? 3 : 2;
}

class _Header extends StatelessWidget {
  const _Header({required this.metrics});

  final PerformanceMetrics metrics;

  @override
  Widget build(BuildContext context) {
    final total = metrics.maxAlloc > 0 ? metrics.maxAlloc : metrics.freeHeap;
    final heapFraction = (total - metrics.freeHeap) / total;
    return GlassCard(
      blur: false,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'Live telemetry',
            style: Theme.of(context).textTheme.titleMedium,
          ),
          const SizedBox(height: 8),
          Text(
            'Updated every 3s • persists: ${Formatters.timestamp(DateTime.now())}',
            style: const TextStyle(color: AppColors.textMuted, fontSize: 13),
          ),
          if (metrics.apiLatencyMs > 0)
            Text(
              'API latency: ${metrics.apiLatencyMs} ms',
              style: const TextStyle(color: AppColors.textMuted, fontSize: 13),
            ),
          const SizedBox(height: 12),
          _Gauge(label: 'Heap usage', fraction: heapFraction),
        ],
      ),
    );
  }
}

class _GaugeSection extends StatelessWidget {
  const _GaugeSection({required this.title, required this.children});

  final String title;
  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return GlassCard(
      blur: false,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(title, style: Theme.of(context).textTheme.titleMedium),
          const SizedBox(height: 16),
          ...children,
        ],
      ),
    );
  }
}

class _Gauge extends StatelessWidget {
  const _Gauge({
    required this.label,
    required this.fraction,
    this.color = AppColors.primary,
  });

  final String label;
  final double fraction;
  final Color color;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 14),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                label,
                style: const TextStyle(
                  color: AppColors.textSecondary,
                  fontSize: 13,
                ),
              ),
              Text(
                '${(fraction * 100).clamp(0, 100).toStringAsFixed(0)}%',
                style: const TextStyle(
                  color: AppColors.textMuted,
                  fontSize: 13,
                ),
              ),
            ],
          ),
          const SizedBox(height: 6),
          MetricBar(fraction: fraction, color: color),
        ],
      ),
    );
  }
}

class _ModuleStatusSection extends StatelessWidget {
  const _ModuleStatusSection({required this.status});

  final SystemStatus status;

  @override
  Widget build(BuildContext context) {
    final entries = status.modules.entries.toList()
      ..sort((a, b) => a.key.compareTo(b.key));

    return GlassCard(
      blur: false,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Text('Modules', style: Theme.of(context).textTheme.titleMedium),
              const Spacer(),
              if (status.headless)
                const StatusBadge(
                  label: 'HEADLESS',
                  tone: BadgeTone.warning,
                  icon: Icons.developer_mode,
                ),
            ],
          ),
          const SizedBox(height: 12),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [for (final entry in entries) _badge(entry)],
          ),
        ],
      ),
    );
  }

  Widget _badge(MapEntry<String, String> entry) {
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
