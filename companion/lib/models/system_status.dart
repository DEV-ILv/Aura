/// Response from the firmware `/api/status` endpoint.
class SystemStatus {
  const SystemStatus({
    required this.running,
    required this.uptimeSeconds,
    required this.freeHeap,
    required this.wifiConnected,
    required this.requestCount,
    this.headless = false,
    this.mode = 'normal',
    this.modules = const <String, String>{},
  });

  const SystemStatus.unknown()
    : running = false,
      uptimeSeconds = 0,
      freeHeap = 0,
      wifiConnected = false,
      requestCount = 0,
      headless = false,
      mode = 'normal',
      modules = const <String, String>{};

  final bool running;
  final int uptimeSeconds;
  final int freeHeap;
  final bool wifiConnected;
  final int requestCount;

  /// Whether the device is running in Headless Development Mode.
  final bool headless;

  /// How headless mode was activated: normal / auto / forced.
  final String mode;

  /// Per-module runtime status keyed by module name
  /// (e.g. {"display": "DISABLED", "wifi": "ONLINE"}).
  final Map<String, String> modules;

  /// Modules currently ONLINE.
  List<String> get connectedModules => modules.entries
      .where((e) => e.value == 'ONLINE')
      .map((e) => e.key)
      .toList(growable: false);

  /// Modules that are unavailable (disabled, offline or in error).
  List<String> get unavailableModules => modules.entries
      .where(
        (e) =>
            e.value == 'DISABLED' || e.value == 'OFFLINE' || e.value == 'ERROR',
      )
      .map((e) => e.key)
      .toList(growable: false);

  factory SystemStatus.fromJson(Map<String, dynamic> json) {
    final rawModules = json['modules'];
    final modules = rawModules is Map<String, dynamic>
        ? rawModules.map((k, v) => MapEntry(k, v.toString()))
        : <String, String>{};
    return SystemStatus(
      running: json['running'] as bool? ?? false,
      uptimeSeconds: (json['uptime'] as num?)?.toInt() ?? 0,
      freeHeap: (json['heap_free'] as num?)?.toInt() ?? 0,
      wifiConnected: json['wifi_connected'] as bool? ?? false,
      requestCount: (json['requests'] as num?)?.toInt() ?? 0,
      headless: json['headless'] as bool? ?? false,
      mode: json['mode'] as String? ?? 'normal',
      modules: modules,
    );
  }

  SystemStatus copyWith({
    bool? running,
    int? uptimeSeconds,
    int? freeHeap,
    bool? wifiConnected,
    int? requestCount,
    bool? headless,
    String? mode,
    Map<String, String>? modules,
  }) {
    return SystemStatus(
      running: running ?? this.running,
      uptimeSeconds: uptimeSeconds ?? this.uptimeSeconds,
      freeHeap: freeHeap ?? this.freeHeap,
      wifiConnected: wifiConnected ?? this.wifiConnected,
      requestCount: requestCount ?? this.requestCount,
      headless: headless ?? this.headless,
      mode: mode ?? this.mode,
      modules: modules ?? this.modules,
    );
  }
}
