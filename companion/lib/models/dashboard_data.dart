import '../models/device_info.dart';
import '../models/performance_metrics.dart';
import '../models/system_status.dart';
import '../models/wifi_status.dart';

/// Aggregated snapshot shown on the dashboard.
class DashboardData {
  const DashboardData({
    required this.status,
    required this.wifi,
    required this.performance,
    required this.device,
  });

  const DashboardData.unknown()
    : status = const SystemStatus.unknown(),
      wifi = const WifiStatus.unknown(),
      performance = const PerformanceMetrics.unknown(),
      device = const DeviceInfo.unknown();

  final SystemStatus status;
  final WifiStatus wifi;
  final PerformanceMetrics performance;
  final DeviceInfo device;

  /// Whether any live metric was observed at all.
  bool get hasData =>
      status.running || wifi.connected || performance.freeHeap > 0;

  DashboardData copyWith({
    SystemStatus? status,
    WifiStatus? wifi,
    PerformanceMetrics? performance,
    DeviceInfo? device,
  }) {
    return DashboardData(
      status: status ?? this.status,
      wifi: wifi ?? this.wifi,
      performance: performance ?? this.performance,
      device: device ?? this.device,
    );
  }
}
