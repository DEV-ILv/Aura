/// Performance metrics reported by the firmware `/api/performance` endpoint
/// and the live WebSocket dashboard broadcast.
class PerformanceMetrics {
  const PerformanceMetrics({
    this.freeHeap = 0,
    this.minHeap = 0,
    this.maxAlloc = 0,
    this.cpuUsage = 0,
    this.cpuMhz = 0,
    this.wifiRssi = 0,
    this.apiLatencyMs = 0,
    this.memories = 0,
    this.conversations = 0,
    this.fragmentationPercent = 0,
    this.temperature = 0,
    this.battery = 0,
    this.storageUsed = 0,
    this.storageTotal = 0,
    this.lastSyncEpoch = 0,
  });

  const PerformanceMetrics.unknown()
    : freeHeap = 0,
      minHeap = 0,
      maxAlloc = 0,
      cpuUsage = 0,
      cpuMhz = 0,
      wifiRssi = 0,
      apiLatencyMs = 0,
      memories = 0,
      conversations = 0,
      fragmentationPercent = 0,
      temperature = 0,
      battery = 0,
      storageUsed = 0,
      storageTotal = 0,
      lastSyncEpoch = 0;

  final int freeHeap;
  final int minHeap;
  final int maxAlloc;
  final double cpuUsage;
  final double cpuMhz;
  final int wifiRssi;
  final int apiLatencyMs;
  final int memories;
  final int conversations;
  final double fragmentationPercent;
  final double temperature;
  final double battery;
  final int storageUsed;
  final int storageTotal;
  final int lastSyncEpoch;

  PerformanceMetrics copyWith({
    int? freeHeap,
    int? wifiRssi,
    double? cpuUsage,
    int? apiLatencyMs,
    double? temperature,
    double? battery,
    int? storageUsed,
    int? storageTotal,
    int? lastSyncEpoch,
  }) {
    return PerformanceMetrics(
      freeHeap: freeHeap ?? this.freeHeap,
      minHeap: minHeap,
      maxAlloc: maxAlloc,
      cpuUsage: cpuUsage ?? this.cpuUsage,
      cpuMhz: cpuMhz,
      wifiRssi: wifiRssi ?? this.wifiRssi,
      apiLatencyMs: apiLatencyMs ?? this.apiLatencyMs,
      memories: memories,
      conversations: conversations,
      fragmentationPercent: fragmentationPercent,
      temperature: temperature ?? this.temperature,
      battery: battery ?? this.battery,
      storageUsed: storageUsed ?? this.storageUsed,
      storageTotal: storageTotal ?? this.storageTotal,
      lastSyncEpoch: lastSyncEpoch ?? this.lastSyncEpoch,
    );
  }

  factory PerformanceMetrics.fromJson(Map<String, dynamic> json) {
    return PerformanceMetrics(
      freeHeap: (json['free_heap'] as num?)?.toInt() ?? 0,
      minHeap: (json['min_heap'] as num?)?.toInt() ?? 0,
      maxAlloc: (json['max_alloc'] as num?)?.toInt() ?? 0,
      cpuUsage: (json['cpu_usage'] as num?)?.toDouble() ?? 0,
      cpuMhz: (json['cpu_mhz'] as num?)?.toDouble() ?? 0,
      wifiRssi: (json['wifi_rssi'] as num?)?.toInt() ?? 0,
      apiLatencyMs: (json['api_latency_ms'] as num?)?.toInt() ?? 0,
      memories: (json['memories'] as num?)?.toInt() ?? 0,
      conversations: (json['conversations'] as num?)?.toInt() ?? 0,
      fragmentationPercent: (json['frag_pct'] as num?)?.toDouble() ?? 0,
      temperature: (json['temperature'] as num?)?.toDouble() ?? 0,
      battery: (json['battery'] as num?)?.toDouble() ?? 0,
      storageUsed: (json['storage_used'] as num?)?.toInt() ?? 0,
      storageTotal: (json['storage_total'] as num?)?.toInt() ?? 0,
      lastSyncEpoch: (json['last_sync'] as num?)?.toInt() ?? 0,
    );
  }
}
