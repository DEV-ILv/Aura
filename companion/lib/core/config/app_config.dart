/// Build-time configuration shared across the AURA companion.
abstract final class AppConfig {
  /// Whether this is a development build.
  ///
  /// When `true` the login form is prefilled with the well-known development
  /// credentials (`Devil` / `Devil`) so local testing against a dev-mode
  /// device is frictionless. The device firmware only accepts these when it is
  /// itself built with `AURA_DEVELOPMENT_MODE` enabled.
  ///
  /// Enable with: `flutter run --dart-define=AURA_DEVELOPMENT_MODE=true`
  ///
  /// MUST be `false` for release builds — the device generates its own admin
  /// password on first boot and prefilling a guessed value is misleading.
  static const bool kDevelopmentMode = bool.fromEnvironment(
    'AURA_DEVELOPMENT_MODE',
  );

  /// Well-known development credentials (local testing only).
  static const String kDevUsername = 'Devil';
  static const String kDevPassword = 'Devil';
}
