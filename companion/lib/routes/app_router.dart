import 'package:flutter/material.dart';

import '../screens/connection/connection_screen.dart';
import '../screens/developer/developer_screen.dart';
import '../screens/home/app_shell.dart';
import '../screens/splash/splash_screen.dart';
import 'app_routes.dart';

/// Central route table for the application.
abstract final class AppRouter {
  static Route<dynamic> onGenerateRoute(RouteSettings settings) {
    switch (settings.name) {
      case AppRoutes.splash:
        return _page(const SplashScreen());
      case AppRoutes.home:
        return _page(const AppShell());
      case AppRoutes.connection:
        return _page(const ConnectionScreen());
      case AppRoutes.developer:
        return _page(const DeveloperScreen());
      default:
        return _page(const SplashScreen());
    }
  }

  static MaterialPageRoute<dynamic> _page(Widget child) {
    return MaterialPageRoute<dynamic>(builder: (_) => child);
  }
}
