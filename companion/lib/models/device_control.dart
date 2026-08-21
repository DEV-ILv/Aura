/// Models for the V2 device-control surface exposed by the firmware.
///
/// These wrap the JSON payloads of the new `/api/wifi/scan`,
/// `/api/display/control`, `/api/led/control`, `/api/audio/control`,
/// `/api/mic/control` and `/api/mic/level` endpoints.
library;

/// A single Wi-Fi network discovered by a device scan.
class WifiNetwork {
  const WifiNetwork({
    required this.ssid,
    required this.rssi,
    required this.channel,
    required this.isOpen,
  });

  final String ssid;
  final int rssi;
  final int channel;
  final bool isOpen;

  factory WifiNetwork.fromJson(Map<String, dynamic> json) {
    return WifiNetwork(
      ssid: (json['ssid'] as String?) ?? '',
      rssi: (json['rssi'] as num?)?.toInt() ?? 0,
      channel: (json['channel'] as num?)?.toInt() ?? 0,
      isOpen: json['open'] == true,
    );
  }
}

/// State of the OLED display returned by `/api/display/control` (GET).
class DisplayControlState {
  const DisplayControlState({
    required this.on,
    required this.brightness,
    required this.timeout,
    required this.nightMode,
  });

  const DisplayControlState.unknown()
    : on = true,
      brightness = 255,
      timeout = 30,
      nightMode = false;

  final bool on;
  final int brightness;
  final int timeout;
  final bool nightMode;

  factory DisplayControlState.fromJson(Map<String, dynamic> json) {
    return DisplayControlState(
      on: json['on'] == true,
      brightness: (json['brightness'] as num?)?.toInt() ?? 255,
      timeout: (json['timeout'] as num?)?.toInt() ?? 30,
      nightMode: json['night_mode'] == true,
    );
  }
}

/// State of the LED ring returned by `/api/led/control` (GET).
class LedControlState {
  const LedControlState({
    required this.enabled,
    required this.brightness,
    required this.mood,
    required this.r,
    required this.g,
    required this.b,
    this.disco = false,
    this.discoActive = false,
    this.discoBrightness = 80,
  });

  const LedControlState.unknown()
    : enabled = true,
      brightness = 255,
      mood = 'idle',
      r = 255,
      g = 255,
      b = 255,
      disco = false,
      discoActive = false,
      discoBrightness = 80;

  final bool enabled;
  final int brightness;
  final String mood;
  final int r;
  final int g;
  final int b;

  /// Disco Mode override requested by the companion app.
  final bool disco;

  /// Whether Disco animations are actually running (mood permits, ring on).
  final bool discoActive;

  /// Brightness percent (10-100) Disco Mode animates toward.
  final int discoBrightness;

  factory LedControlState.fromJson(Map<String, dynamic> json) {
    final color = (json['color'] as Map<String, dynamic>?) ?? const {};
    return LedControlState(
      enabled: json['enabled'] == true,
      brightness: (json['brightness'] as num?)?.toInt() ?? 255,
      mood: (json['mood'] as String?) ?? 'idle',
      r: (color['r'] as num?)?.toInt() ?? 255,
      g: (color['g'] as num?)?.toInt() ?? 255,
      b: (color['b'] as num?)?.toInt() ?? 255,
      disco: json['disco'] == true,
      discoActive: json['discoActive'] == true,
      discoBrightness: (json['discoBrightness'] as num?)?.toInt() ?? 80,
    );
  }
}

/// Live microphone energy sample from `/api/mic/level`.
class MicLiveLevel {
  const MicLiveLevel({
    required this.energy,
    required this.peak,
    required this.noiseFloor,
    required this.noiseThreshold,
    required this.recording,
  });

  const MicLiveLevel.unknown()
    : energy = 0,
      peak = 0,
      noiseFloor = 0,
      noiseThreshold = 0,
      recording = false;

  final double energy;
  final double peak;
  final int noiseFloor;
  final int noiseThreshold;
  final bool recording;

  factory MicLiveLevel.fromJson(Map<String, dynamic> json) {
    return MicLiveLevel(
      energy: (json['energy'] as num?)?.toDouble() ?? 0,
      peak: (json['peak'] as num?)?.toDouble() ?? 0,
      noiseFloor: (json['noise_floor'] as num?)?.toInt() ?? 0,
      noiseThreshold: (json['noise_threshold'] as num?)?.toInt() ?? 0,
      recording: json['recording'] == true,
    );
  }
}

/// The mood names the firmware LED ring understands, used for the selector.
const List<String> auraMoods = [
  'idle',
  'listening',
  'thinking',
  'processing',
  'speaking',
  'happy',
  'success',
  'reminder',
  'warning',
  'error',
  'critical',
  'ota',
  'offline',
  'sleep',
  'wake',
  'wifi_connecting',
  'wifi_connected',
];
