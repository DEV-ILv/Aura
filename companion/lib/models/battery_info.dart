/// Battery information for a connected device.
class BatteryInfo {
  const BatteryInfo({
    this.level = 0,
    this.isCharging = false,
    this.voltage = 0,
    this.temperature = 0,
  });

  const BatteryInfo.unknown()
    : level = 0,
      isCharging = false,
      voltage = 0,
      temperature = 0;

  /// Battery charge as a percentage (0-100). 0 may mean "no battery".
  final double level;

  /// Whether the device is currently charging.
  final bool isCharging;

  /// Battery voltage in volts.
  final double voltage;

  /// Battery temperature in Celsius.
  final double temperature;

  /// Whether the device reports a battery at all.
  bool get isPresent => level > 0 || voltage > 0;

  factory BatteryInfo.fromJson(Map<String, dynamic> json) {
    return BatteryInfo(
      level: (json['level'] as num?)?.toDouble() ?? 0,
      isCharging:
          json['charging'] as bool? ?? json['is_charging'] as bool? ?? false,
      voltage: (json['voltage'] as num?)?.toDouble() ?? 0,
      temperature: (json['temperature'] as num?)?.toDouble() ?? 0,
    );
  }
}
