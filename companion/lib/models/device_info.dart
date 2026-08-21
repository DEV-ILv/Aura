import '../core/config/device_config.dart';

/// Identity of a connected AURA device.
class DeviceInfo {
  const DeviceInfo({
    this.name = DeviceConfig.fallbackDeviceName,
    this.version = '1.0.0',
    this.mark = '',
    this.codename = '',
    this.channel = '',
    this.buildDate = '',
    this.buildTime = '',
    this.chip = DeviceConfig.fallbackChip,
    this.hasApiKey = false,
  });

  const DeviceInfo.unknown()
    : name = DeviceConfig.fallbackDeviceName,
      version = 'unknown',
      mark = '',
      codename = '',
      channel = '',
      buildDate = '',
      buildTime = '',
      chip = DeviceConfig.fallbackChip,
      hasApiKey = false;

  final String name;
  final String version;

  /// Firmware generation mark, e.g. `III` (from `/api/version`).
  final String mark;

  /// Firmware codename, e.g. `Phoenix` (from `/api/version`).
  final String codename;

  /// Release channel, e.g. `Development` (from `/api/version`).
  final String channel;

  final String buildDate;
  final String buildTime;

  /// Hardware platform (ESP32 family) reported by the device.
  final String chip;

  final bool hasApiKey;

  /// Model label shown on the device card, e.g. `AURA V1 Prototype`.
  String get modelLabel {
    final base = '$name V1 Prototype';
    if (mark.isEmpty && codename.isEmpty) {
      return base;
    }
    final parts = [mark, codename].where((p) => p.isNotEmpty).join(' ');
    return '$base — $parts';
  }

  DeviceInfo copyWith({
    String? name,
    String? version,
    String? mark,
    String? codename,
    String? channel,
    String? buildDate,
    String? buildTime,
    String? chip,
    bool? hasApiKey,
  }) {
    return DeviceInfo(
      name: name ?? this.name,
      version: version ?? this.version,
      mark: mark ?? this.mark,
      codename: codename ?? this.codename,
      channel: channel ?? this.channel,
      buildDate: buildDate ?? this.buildDate,
      buildTime: buildTime ?? this.buildTime,
      chip: chip ?? this.chip,
      hasApiKey: hasApiKey ?? this.hasApiKey,
    );
  }

  /// Builds a [DeviceInfo] from the firmware `/api/settings` payload.
  factory DeviceInfo.fromJson(Map<String, dynamic> json) {
    return DeviceInfo(
      name: json['device_name'] as String? ?? DeviceConfig.fallbackDeviceName,
      version: json['version'] as String? ?? 'unknown',
      buildDate: json['build_date'] as String? ?? '',
      buildTime: json['build_time'] as String? ?? '',
      hasApiKey: json['has_key'] as bool? ?? false,
    );
  }

  /// Builds a [DeviceInfo] from the firmware `/api/version` payload.
  factory DeviceInfo.fromVersionJson(Map<String, dynamic> json) {
    return DeviceInfo(
      version: json['version'] as String? ?? 'unknown',
      mark: json['mark'] as String? ?? '',
      codename: json['codename'] as String? ?? '',
      channel: json['channel'] as String? ?? '',
      buildDate: json['build'] as String? ?? '',
    );
  }
}
