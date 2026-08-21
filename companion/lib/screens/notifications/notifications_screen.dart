import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/services/notification_service.dart';
import '../../core/theme/app_colors.dart';
import '../../providers/app_providers.dart';

/// Local notifications settings and testing.
///
/// Reminder notifications are scheduled automatically by the reminder
/// workflow; this screen lets the user verify delivery and opt into alerts.
class NotificationsScreen extends ConsumerStatefulWidget {
  const NotificationsScreen({super.key});

  @override
  ConsumerState<NotificationsScreen> createState() =>
      _NotificationsScreenState();
}

class _NotificationsScreenState extends ConsumerState<NotificationsScreen> {
  bool _deviceAlerts = true;
  bool _soundEnabled = true;
  bool _sending = false;

  Future<void> _sendTest() async {
    setState(() => _sending = true);
    final service = ref.read(notificationServiceProvider);
    try {
      await service.showDeviceAlert(
        id: NotificationService.deviceAlertId,
        title: 'AURA Alert',
        body: 'This is a test notification from AURA Companion.',
      );
    } finally {
      if (mounted) {
        setState(() => _sending = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Notifications')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Column(
              children: [
                SwitchListTile(
                  secondary: const Icon(Icons.volume_up),
                  title: const Text('Device alerts'),
                  subtitle: const Text('Sound alerts for device events.'),
                  value: _deviceAlerts,
                  onChanged: (value) => setState(() => _deviceAlerts = value),
                ),
                const Divider(height: 1),
                SwitchListTile(
                  secondary: const Icon(Icons.notifications_active),
                  title: const Text('Alert sound / vibration'),
                  subtitle: const Text(
                    'Use sound and vibration for reminders.',
                  ),
                  value: _soundEnabled,
                  onChanged: (value) => setState(() => _soundEnabled = value),
                ),
              ],
            ),
          ),
          const SizedBox(height: 16),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('Test', style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 4),
                  const Text(
                    'Send a local notification to confirm that alerts are '
                    'delivered correctly on this device.',
                    style: TextStyle(color: AppColors.textMuted, fontSize: 13),
                  ),
                  const SizedBox(height: 12),
                  FilledButton.icon(
                    onPressed: _sending ? null : _sendTest,
                    icon: const Icon(Icons.notifications),
                    label: Text(
                      _sending ? 'Sending…' : 'Send test notification',
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }
}
