import 'package:intl/intl.dart';

/// Formatting helpers for human readable output.
abstract final class Formatters {
  /// Formats a byte count as a human readable size string.
  static String bytes(int value) {
    if (value < 1024) {
      return '$value B';
    }
    const units = <String>['KB', 'MB', 'GB', 'TB'];
    var size = value.toDouble();
    var unit = -1;
    do {
      size /= 1024;
      unit++;
    } while (size >= 1024 && unit < units.length - 1);
    return '${size.toStringAsFixed(size >= 100 ? 0 : 1)} ${units[unit]}';
  }

  /// Formats a value in seconds as a compact uptime string.
  static String uptime(int totalSeconds) {
    if (totalSeconds < 0) {
      return '—';
    }
    final days = totalSeconds ~/ 86400;
    final hours = (totalSeconds % 86400) ~/ 3600;
    final minutes = (totalSeconds % 3600) ~/ 60;
    final seconds = totalSeconds % 60;

    if (days > 0) {
      return '${days}d ${hours}h ${minutes}m';
    }
    if (hours > 0) {
      return '${hours}h ${minutes}m ${seconds}s';
    }
    if (minutes > 0) {
      return '${minutes}m ${seconds}s';
    }
    return '${seconds}s';
  }

  /// Formats a Unix timestamp (seconds) using the device local time.
  static String timestamp(DateTime time) {
    return DateFormat('HH:mm:ss').format(time.toLocal());
  }

  /// Formats a date + time for scheduling displays.
  static String dateTime(DateTime time) {
    return DateFormat('EEE, MMM d • HH:mm').format(time.toLocal());
  }

  /// Formats a Unix timestamp (seconds) using the device local time.
  static String timestampFromSeconds(int? epochSeconds) {
    if (epochSeconds == null || epochSeconds <= 0) {
      return '—';
    }
    return timestamp(DateTime.fromMillisecondsSinceEpoch(epochSeconds * 1000));
  }

  /// Describes a WiFi signal strength in dBm as a qualitative label.
  static String signalLabel(int rssi) {
    if (rssi >= -50) {
      return 'Excellent';
    }
    if (rssi >= -60) {
      return 'Good';
    }
    if (rssi >= -70) {
      return 'Fair';
    }
    return 'Weak';
  }

  /// Sanitizes a raw host string into a canonical base URL.
  static String normalizeHost(String raw) {
    var host = raw.trim().toLowerCase();
    if (host.startsWith('http://')) {
      host = host.substring(7);
    } else if (host.startsWith('https://')) {
      host = host.substring(8);
    }
    host = host.replaceAll(RegExp(r'[/]+$'), '');
    return host;
  }
}
