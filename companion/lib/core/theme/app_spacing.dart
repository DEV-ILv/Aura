/// Spacing scale used across the entire application.
///
/// No widget should hardcode raw numbers where a token exists.
abstract final class AppSpacing {
  static const double xs = 4;
  static const double sm = 8;
  static const double md = 12;
  static const double lg = 16;
  static const double xl = 24;
  static const double xxl = 32;

  /// Page edge padding for scrollable screens.
  static const double page = 16;

  /// Standard padding inside a glass card.
  static const double card = 16;

  /// Gap between grouped sections.
  static const double section = 24;

  /// Grid spacing between cards.
  static const double grid = 12;
}
