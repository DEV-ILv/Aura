/// Validation helpers for user input.
abstract final class Validators {
  /// Validates an IPv4 address or a hostname.
  ///
  /// Returns an error message when the value is not acceptable, otherwise
  /// `null` (which marks the value as valid for `Form` fields).
  static String? hostname(String? value) {
    final input = value?.trim() ?? '';
    if (input.isEmpty) {
      return 'Enter a host or IP address';
    }
    if (RegExp(r'^[0-9]{1,3}(\.[0-9]{1,3}){3}$').hasMatch(input)) {
      for (final part in input.split('.')) {
        final num = int.tryParse(part);
        if (num == null || num < 0 || num > 255) {
          return 'Enter a valid IPv4 address';
        }
      }
      return null;
    }
    if (RegExp(
      r'^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?(\.[a-zA-Z0-9-]+)*$',
    ).hasMatch(input)) {
      return null;
    }
    return 'Enter a valid IP address or hostname';
  }

  /// Validates a port number in the range [1, 65535].
  static String? port(String? value) {
    final input = value?.trim() ?? '';
    if (input.isEmpty) {
      return 'Enter a port';
    }
    final num = int.tryParse(input);
    if (num == null || num < 1 || num > 65535) {
      return 'Enter a port between 1 and 65535';
    }
    return null;
  }

  /// Validates a request timeout in milliseconds.
  static String? timeout(String? value) {
    final input = value?.trim() ?? '';
    if (input.isEmpty) {
      return 'Enter a timeout';
    }
    final num = int.tryParse(input);
    if (num == null || num < 500 || num > 60000) {
      return 'Enter a value between 500 and 60000 ms';
    }
    return null;
  }
}
