import 'dart:async';

import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../core/constants/app_constants.dart';
import '../core/services/logger.dart';
import '../core/services/notification_service.dart';
import '../core/utils/formatters.dart';
import '../models/dashboard_data.dart';
import '../repositories/device_repository.dart';
import '../websocket/websocket_service.dart';
import 'app_providers.dart';
import 'connection_provider.dart';
import 'settings_provider.dart';

/// Thresholds for proactive device alerts.
abstract final class _AlertThresholds {
  static const double lowBatteryPercent = 15;
  static const int lowFreeHeapBytes = 32 * 1024;
}

/// Dashboard view state.
class DashboardState {
  const DashboardState({
    required this.data,
    required this.isLoading,
    this.error,
    this.lastUpdated,
  });

  const DashboardState.initial()
    : data = const DashboardData.unknown(),
      isLoading = true,
      error = null,
      lastUpdated = null;

  final DashboardData data;
  final bool isLoading;
  final String? error;
  final DateTime? lastUpdated;

  DashboardState copyWith({
    DashboardData? data,
    bool? isLoading,
    String? error,
    DateTime? lastUpdated,
  }) {
    return DashboardState(
      data: data ?? this.data,
      isLoading: isLoading ?? this.isLoading,
      error: error,
      lastUpdated: lastUpdated ?? this.lastUpdated,
    );
  }
}

/// Loads and keeps the dashboard metrics fresh.
///
/// Uses the live WebSocket feed when available and falls back to a periodic
/// REST poll otherwise. All metric merging happens here, never in widgets.
class DashboardNotifier extends StateNotifier<DashboardState> {
  DashboardNotifier(this._ref, this._repository)
    : super(const DashboardState.initial()) {
    _subscribeLiveFeed();
  }

  final Ref _ref;
  final DeviceRepository _repository;

  Timer? _pollTimer;
  StreamSubscription<dynamic>? _liveSubscription;
  bool _lowBatteryNotified = false;
  bool _memoryWarned = false;

  void _subscribeLiveFeed() {
    final service = _ref.read(webSocketServiceProvider);
    _liveSubscription = service.events.listen(_onLiveEvent);
  }

  void _onLiveEvent(WebSocketEvent event) {
    if (event.type == 'module_status') {
      _applyModuleStatus(event.payload);
      return;
    }
    final metrics = state.data.performance;
    final merged = _repository.mergeLiveMetrics(metrics, event.payload);
    final status = state.data.status;
    state = state.copyWith(
      data: state.data.copyWith(performance: merged, status: status),
      isLoading: false,
      lastUpdated: DateTime.now(),
    );
  }

  /// Merges a live module-status change payload into the dashboard state.
  void _applyModuleStatus(Map<String, dynamic> payload) {
    final rawModules = payload['modules'];
    if (rawModules is! Map<String, dynamic>) {
      return;
    }
    final current = state.data.status;
    final modules = Map<String, String>.from(current.modules);
    var headless = current.headless;
    var mode = current.mode;
    rawModules.forEach((key, value) {
      if (key == 'system') {
        return;
      }
      modules[key.toString()] = value.toString();
    });
    // Headless flag can change at runtime (e.g. display hot-plug), mirror it.
    if (payload.containsKey('headless')) {
      headless = payload['headless'] == true;
    }
    if (payload.containsKey('mode')) {
      mode = payload['mode']?.toString() ?? mode;
    }
    final status = current.copyWith(
      modules: modules,
      headless: headless,
      mode: mode,
    );
    state = state.copyWith(
      data: state.data.copyWith(status: status),
      lastUpdated: DateTime.now(),
    );
  }

  /// Refreshes the full dashboard over REST.
  Future<void> refresh() async {
    state = state.copyWith(isLoading: true);
    try {
      final data = await _repository.loadDashboard();
      state = DashboardState(
        data: data,
        isLoading: false,
        lastUpdated: DateTime.now(),
      );
      _evaluateAlerts(data);
      _ensurePolling();
    } catch (error) {
      Logger.warning('Dashboard refresh failed: $error');
      state = state.copyWith(
        isLoading: false,
        error: 'Could not refresh device metrics.',
      );
      _ref
          .read(connectionProvider.notifier)
          .markError('Lost contact with the device.');
    }
  }

  /// Starts lightweight periodic polling as a safety net.
  void _ensurePolling() {
    _pollTimer ??= Timer.periodic(
      const Duration(milliseconds: AppConstants.metricsPollIntervalMs),
      (_) => _poll(),
    );
  }

  Future<void> _poll() async {
    final connection = _ref.read(connectionProvider);
    if (!connection.isConnected || connection.mode == ConnectionMode.remote) {
      return;
    }
    try {
      final performance = await _repository.fetchPerformance();
      final updated = state.data.copyWith(
        performance: performance,
        status: state.data.status,
      );
      state = state.copyWith(data: updated, lastUpdated: DateTime.now());
      _evaluateAlerts(updated);
    } catch (_) {
      // The connection provider owns reconnect signalling.
    }
  }

  /// Fires one-shot alerts for low battery and low heap, gated on the
  /// device-alert preference. Each alert re-arms once the metric recovers.
  void _evaluateAlerts(DashboardData data) {
    final notifications = _ref.read(notificationServiceProvider);
    if (!_ref.read(settingsProvider).settings.alertsEnabled) {
      return;
    }
    final perf = data.performance;

    final lowBattery =
        perf.battery > 0 && perf.battery < _AlertThresholds.lowBatteryPercent;
    if (lowBattery && !_lowBatteryNotified) {
      _lowBatteryNotified = true;
      notifications.showDeviceAlert(
        id: NotificationService.stableId('low_battery'),
        title: 'AURA battery low',
        body:
            'Battery is at ${perf.battery.toStringAsFixed(0)}%. Consider powering the device.',
      );
    } else if (!lowBattery) {
      _lowBatteryNotified = false;
    }

    final lowHeap =
        perf.freeHeap > 0 && perf.freeHeap < _AlertThresholds.lowFreeHeapBytes;
    if (lowHeap && !_memoryWarned) {
      _memoryWarned = true;
      notifications.showDeviceAlert(
        id: NotificationService.stableId('memory_warning'),
        title: 'AURA memory warning',
        body:
            'Free heap is critically low (${Formatters.bytes(perf.freeHeap)}).',
      );
    } else if (!lowHeap) {
      _memoryWarned = false;
    }
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    _liveSubscription?.cancel();
    super.dispose();
  }
}

final dashboardProvider =
    StateNotifierProvider<DashboardNotifier, DashboardState>((ref) {
      return DashboardNotifier(ref, ref.watch(deviceRepositoryProvider));
    });
