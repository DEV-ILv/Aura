/// State machine for a firmware update.
enum OtaState { idle, downloading, validating, applying, complete, error }

/// Current over-the-air update information.
class OtaInfo {
  const OtaInfo({
    required this.currentVersion,
    required this.state,
    this.progress = 0,
    this.message = '',
  });

  const OtaInfo.unknown()
    : currentVersion = 'unknown',
      state = OtaState.idle,
      progress = 0,
      message = '';

  final String currentVersion;
  final OtaState state;

  /// 0.0 - 1.0 progress of the current update.
  final double progress;
  final String message;

  bool get isBusy =>
      state == OtaState.downloading ||
      state == OtaState.validating ||
      state == OtaState.applying;

  OtaInfo copyWith({
    String? currentVersion,
    OtaState? state,
    double? progress,
    String? message,
  }) {
    return OtaInfo(
      currentVersion: currentVersion ?? this.currentVersion,
      state: state ?? this.state,
      progress: progress ?? this.progress,
      message: message ?? this.message,
    );
  }

  factory OtaInfo.fromJson(Map<String, dynamic> json) {
    return OtaInfo(
      currentVersion:
          json['version'] as String? ??
          json['current_version'] as String? ??
          'unknown',
      state: _parseState(json['state'] as String?),
      progress: ((json['progress'] as num?)?.toDouble() ?? 0).clamp(0, 1),
      message: json['message'] as String? ?? '',
    );
  }

  static OtaState _parseState(String? raw) {
    switch (raw) {
      case 'downloading':
        return OtaState.downloading;
      case 'validating':
        return OtaState.validating;
      case 'applying':
        return OtaState.applying;
      case 'complete':
        return OtaState.complete;
      case 'error':
        return OtaState.error;
      default:
        return OtaState.idle;
    }
  }
}
