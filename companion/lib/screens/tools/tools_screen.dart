import 'package:flutter/material.dart' hide ConnectionState;

import '../../core/theme/app_colors.dart';
import '../../widgets/glass_card.dart';
import '../device_control/device_control_screen.dart';
import '../disco_mode/disco_mode_screen.dart';
import '../memory/memory_screen.dart';
import '../ota/ota_screen.dart';
import '../reminders/reminders_screen.dart';
import '../sd/sd_screen.dart';
import '../system/system_screen.dart';

/// Hub that groups the AURA tool screens into a visual launchpad.
class ToolsScreen extends StatelessWidget {
  const ToolsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final tools = <_Tool>[
      _Tool(
        icon: Icons.memory,
        label: 'Memory',
        subtitle: 'Search stored memories',
        route: (_) => const MemoryScreen(),
      ),
      _Tool(
        icon: Icons.notifications_active,
        label: 'Reminders',
        subtitle: 'Schedule reminders',
        route: (_) => const RemindersScreen(),
      ),
      _Tool(
        icon: Icons.system_update,
        label: 'Firmware',
        subtitle: 'Flash OTA updates',
        route: (_) => const OtaScreen(),
      ),
      _Tool(
        icon: Icons.sd_storage,
        label: 'SD Card',
        subtitle: 'Browse & upload files',
        route: (_) => const SdScreen(),
      ),
      _Tool(
        icon: Icons.monitor_heart,
        label: 'Monitor',
        subtitle: 'Live system telemetry',
        route: (_) => const SystemScreen(),
      ),
      _Tool(
        icon: Icons.tune,
        label: 'Control',
        subtitle: 'OLED, LEDs, audio, network & system',
        route: (_) => const DeviceControlScreen(),
      ),
      _Tool(
        icon: Icons.auto_awesome,
        label: 'Disco Mode',
        subtitle: 'App-controlled party lights',
        route: (_) => const DiscoModeScreen(),
      ),
    ];

    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text('Tools', style: Theme.of(context).textTheme.titleLarge),
        const SizedBox(height: 4),
        const Text(
          'Long-press or tap to open a device tool.',
          style: TextStyle(color: AppColors.textMuted),
        ),
        const SizedBox(height: 16),
        GridView.count(
          crossAxisCount: _columns(MediaQuery.sizeOf(context).width),
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          mainAxisSpacing: 12,
          crossAxisSpacing: 12,
          childAspectRatio: 1.25,
          children: tools
              .map(
                (tool) => _ToolTile(
                  tool: tool,
                  onTap: () => Navigator.of(
                    context,
                  ).push(MaterialPageRoute(builder: tool.route)),
                ),
              )
              .toList(),
        ),
      ],
    );
  }

  int _columns(double width) => width >= 900 ? 4 : 2;
}

class _Tool {
  const _Tool({
    required this.icon,
    required this.label,
    required this.subtitle,
    required this.route,
  });

  final IconData icon;
  final String label;
  final String subtitle;
  final WidgetBuilder route;
}

class _ToolTile extends StatelessWidget {
  const _ToolTile({required this.tool, required this.onTap});

  final _Tool tool;
  final VoidCallback onTap;

  @override
  Widget build(BuildContext context) {
    return GlassCard(
      blur: false,
      onTap: onTap,
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Icon(tool.icon, color: AppColors.primary, size: 28),
          const Spacer(),
          Text(
            tool.label,
            style: Theme.of(context).textTheme.titleMedium,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
          ),
          const SizedBox(height: 4),
          Text(
            tool.subtitle,
            style: const TextStyle(color: AppColors.textMuted, fontSize: 12),
            maxLines: 2,
            overflow: TextOverflow.ellipsis,
          ),
        ],
      ),
    );
  }
}
