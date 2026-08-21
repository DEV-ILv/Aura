/// Internal device addressing used by the transport layer.
///
/// These values are loaded internally and MUST NOT be surfaced to normal
/// users. Networking details are intentionally hidden behind the developer
/// settings screen.
///
/// Future support for `aura.local` and automatic discovery can be layered on
/// top of this class without changing callers.
abstract final class DeviceConfig {
  /// Default host probed when no address has been configured yet.
  ///
  /// This is the ESP32 Access Point address, which is also resolvable as
  /// `aura-<mac>.local` when mDNS is active on the LAN.
  static const String defaultHost = '192.168.4.1';

  /// Default REST port exposed by the AURA firmware web portal.
  static const int defaultRestPort = 80;

  /// Default WebSocket port exposed by the AURA firmware.
  static const int defaultWebSocketPort = 81;

  /// Builds the REST base URL for [host] and [port].
  static String restBaseUrl(String host, int port) => 'http://$host:$port';

  /// Builds the WebSocket feed URL for [host].
  static String webSocketUrl(String host, [int port = defaultWebSocketPort]) =>
      'ws://$host:$port';

  /// Human-readable chip family reported when the firmware does not expose a
  /// model string.
  static const String fallbackChip = 'ESP32';

  /// Display name shown while the real device identity is still loading.
  static const String fallbackDeviceName = 'AURA';

  /// Display name used for the device card before identity is resolved.
  static const String fallbackModel = 'V1 Prototype';
}
