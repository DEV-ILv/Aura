/// Storage usage information for the device.
class StorageInfo {
  const StorageInfo({
    this.usedBytes = 0,
    this.totalBytes = 0,
    this.freeBytes = 0,
  });

  const StorageInfo.unknown() : usedBytes = 0, totalBytes = 0, freeBytes = 0;

  final int usedBytes;
  final int totalBytes;
  final int freeBytes;

  /// Used fraction between 0 and 1, clamping to avoid malformed data.
  double get usageFraction {
    if (totalBytes <= 0) {
      return 0;
    }
    return (usedBytes / totalBytes).clamp(0.0, 1.0);
  }

  factory StorageInfo.fromJson(Map<String, dynamic> json) {
    final used = (json['used'] as num?)?.toInt() ?? 0;
    final total = (json['total'] as num?)?.toInt() ?? 0;
    return StorageInfo(
      usedBytes: used,
      totalBytes: total,
      freeBytes: (json['free'] as num?)?.toInt() ?? (total - used),
    );
  }
}
