import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../providers/app_providers.dart';
import '../../providers/connection_provider.dart';
import '../../providers/dashboard_provider.dart';
import '../../providers/settings_provider.dart';
import '../../widgets/status_badge.dart';
import '../chat/chat_screen.dart';
import '../dashboard/dashboard_screen.dart';
import '../notifications/notifications_screen.dart';
import '../settings/settings_screen.dart';
import '../tools/tools_screen.dart';

/// Root scaffold hosting the primary destinations.
///
/// Uses a [NavigationRail] on wide layouts (desktop) and a
/// [NavigationBar] on narrow layouts (Android phones).
class AppShell extends ConsumerStatefulWidget {
  const AppShell({super.key});

  @override
  ConsumerState<AppShell> createState() => _AppShellState();
}

class _AppShellState extends ConsumerState<AppShell> {
  int _index = 0;

  static const List<Widget> _pages = [
    DashboardScreen(),
    ChatScreen(),
    ToolsScreen(),
    NotificationsScreen(),
    SettingsScreen(),
  ];

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final settings = ref.read(settingsProvider).settings;
      final notifications = ref.read(notificationServiceProvider);
      notifications.alertsEnabled = settings.alertsEnabled;
      notifications.remindersEnabled = settings.remindersEnabled;
      ref.read(notificationServiceProvider).initialize();
    });
  }

  @override
  Widget build(BuildContext context) {
    final connection = ref.watch(connectionProvider);
    final dashboard = ref.watch(dashboardProvider);
    final wide = MediaQuery.sizeOf(context).width >= 900;

    return Scaffold(
      appBar: AppBar(
        title: const Text('A.U.R.A'),
        actions: [
          Padding(
            padding: const EdgeInsets.only(right: 12),
            child: Center(child: _ConnectionBadge(connection: connection)),
          ),
        ],
      ),
      body: Column(
        children: [
          if (dashboard.data.status.headless) const _HeadlessStrip(),
          Expanded(
            child: Row(
              children: [
                if (wide)
                  NavigationRail(
                    selectedIndex: _index,
                    onDestinationSelected: (index) =>
                        setState(() => _index = index),
                    labelType: NavigationRailLabelType.all,
                    leading: const SizedBox(height: 16),
                    destinations: const [
                      NavigationRailDestination(
                        icon: Icon(Icons.dashboard_outlined),
                        selectedIcon: Icon(Icons.dashboard),
                        label: Text('Dashboard'),
                      ),
                      NavigationRailDestination(
                        icon: Icon(Icons.chat_bubble_outline),
                        selectedIcon: Icon(Icons.chat_bubble),
                        label: Text('Chat'),
                      ),
                      NavigationRailDestination(
                        icon: Icon(Icons.grid_view_outlined),
                        selectedIcon: Icon(Icons.grid_view),
                        label: Text('Tools'),
                      ),
                      NavigationRailDestination(
                        icon: Icon(Icons.notifications_outlined),
                        selectedIcon: Icon(Icons.notifications),
                        label: Text('Alerts'),
                      ),
                      NavigationRailDestination(
                        icon: Icon(Icons.settings_outlined),
                        selectedIcon: Icon(Icons.settings),
                        label: Text('Settings'),
                      ),
                    ],
                  ),
                Expanded(
                  child: IndexedStack(index: _index, children: _pages),
                ),
              ],
            ),
          ),
        ],
      ),
      bottomNavigationBar: wide
          ? null
          : NavigationBar(
              selectedIndex: _index,
              onDestinationSelected: (index) => setState(() => _index = index),
              destinations: const [
                NavigationDestination(
                  icon: Icon(Icons.dashboard_outlined),
                  selectedIcon: Icon(Icons.dashboard),
                  label: 'Dashboard',
                ),
                NavigationDestination(
                  icon: Icon(Icons.chat_bubble_outline),
                  selectedIcon: Icon(Icons.chat_bubble),
                  label: 'Chat',
                ),
                NavigationDestination(
                  icon: Icon(Icons.grid_view_outlined),
                  selectedIcon: Icon(Icons.grid_view),
                  label: 'Tools',
                ),
                NavigationDestination(
                  icon: Icon(Icons.notifications_outlined),
                  selectedIcon: Icon(Icons.notifications),
                  label: 'Alerts',
                ),
                NavigationDestination(
                  icon: Icon(Icons.settings_outlined),
                  selectedIcon: Icon(Icons.settings),
                  label: 'Settings',
                ),
              ],
            ),
    );
  }
}

class _HeadlessStrip extends StatelessWidget {
  const _HeadlessStrip();

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      color: AppColors.warning.withValues(alpha: 0.18),
      child: const Row(
        children: [
          Icon(Icons.developer_mode, color: AppColors.warning, size: 18),
          SizedBox(width: 8),
          Expanded(
            child: Text(
              'Headless Development Mode — running without a display.',
              style: TextStyle(
                color: AppColors.warning,
                fontSize: 13,
                fontWeight: FontWeight.w600,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _ConnectionBadge extends StatelessWidget {
  const _ConnectionBadge({required this.connection});

  final ConnectionState connection;

  @override
  Widget build(BuildContext context) {
    final (label, tone) = switch (connection.phase) {
      ConnectionPhase.connected => (
        connection.mode == ConnectionMode.remote ? 'Cloud' : 'Connected',
        BadgeTone.success,
      ),
      ConnectionPhase.connecting => ('Connecting', BadgeTone.accent),
      ConnectionPhase.testing => ('Testing', BadgeTone.accent),
      ConnectionPhase.probing => ('Checking', BadgeTone.accent),
      ConnectionPhase.reconnecting => ('Reconnecting', BadgeTone.warning),
      ConnectionPhase.error => ('Offline', BadgeTone.danger),
      ConnectionPhase.disconnected => ('Offline', BadgeTone.danger),
      ConnectionPhase.unavailable => ('Unavailable', BadgeTone.danger),
      ConnectionPhase.unauthenticated => ('Signed out', BadgeTone.neutral),
      ConnectionPhase.idle => ('No device', BadgeTone.neutral),
      ConnectionPhase.waitingForDevice => ('Joining Wi-Fi', BadgeTone.accent),
    };
    return StatusBadge(label: label, tone: tone);
  }
}
