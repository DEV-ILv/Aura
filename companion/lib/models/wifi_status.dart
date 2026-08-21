/// WiFi state reported by the firmware `/api/wifi` endpoint.
class WifiStatus {
  const WifiStatus({
    this.connected = false,
    this.ssid = '',
    this.ip = '',
    this.gateway = '',
    this.signal = 0,
  });

  const WifiStatus.unknown()
    : connected = false,
      ssid = '',
      ip = '',
      gateway = '',
      signal = 0;

  final bool connected;
  final String ssid;
  final String ip;
  final String gateway;

  /// RSSI in dBm.
  final int signal;

  factory WifiStatus.fromJson(Map<String, dynamic> json) {
    return WifiStatus(
      connected: json['connected'] as bool? ?? false,
      ssid: json['ssid'] as String? ?? '',
      ip: json['ip'] as String? ?? '',
      gateway: json['gateway'] as String? ?? '',
      signal: (json['signal'] as num?)?.toInt() ?? 0,
    );
  }
}
