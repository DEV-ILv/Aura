import 'package:flutter/foundation.dart';

/// Lightweight, dependency-injectable logger.
///
/// Keeps application logging out of widget and business code so error
/// visibility can be added later without invasive changes.
///
/// In release builds every level is a compile-time no-op so no debug logging
/// is shipped to production.
abstract class Logger {
  /// Master switch honoured by every level. Toggled from the developer
  /// settings screen; defaults to enabled.
  static bool enabled = true;

  /// Prints a debug-level message.
  static void debug(String message) {
    if (enabled && kDebugMode) {
      // ignore: avoid_print
      print('[AURA][dbg] $message');
    }
  }

  /// Prints an informational message.
  static void info(String message) {
    if (enabled && kDebugMode) {
      // ignore: avoid_print
      print('[AURA][inf] $message');
    }
  }

  /// Prints a warning message.
  static void warning(String message) {
    if (enabled && kDebugMode) {
      // ignore: avoid_print
      print('[AURA][wrn] $message');
    }
  }

  /// Prints an error message, optionally with an error object.
  static void error(String message, [Object? error]) {
    if (enabled && kDebugMode) {
      // ignore: avoid_print
      print('[AURA][err] $message${error == null ? '' : ' > $error'}');
    }
  }
}
