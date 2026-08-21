import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../models/ota_info.dart';
import '../../providers/ota_provider.dart';
import '../../widgets/status_badge.dart';

/// Firmware update screen.
class OtaScreen extends ConsumerWidget {
  const OtaScreen({super.key});

  Future<void> _pickAndUpload(BuildContext context, WidgetRef ref) async {
    final result = await FilePicker.platform.pickFiles(
      type: FileType.custom,
      allowedExtensions: const ['bin'],
    );
    if (result == null || result.files.isEmpty) {
      return;
    }
    final file = result.files.single;
    final bytes = await file.xFile.readAsBytes();
    await ref.read(otaProvider.notifier).upload(bytes, file.name);
  }

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final state = ref.watch(otaProvider);

    return Scaffold(
      appBar: AppBar(title: const Text('Firmware Update')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          Card(
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      const Icon(Icons.memory, color: AppColors.primary),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          _stateLabel(state.info.state),
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                      ),
                      StatusBadge(
                        label: state.info.state.name,
                        tone: _toneForState(state.info.state),
                      ),
                    ],
                  ),
                  const Divider(height: 24),
                  _InfoRow(
                    label: 'Current version',
                    value: state.info.currentVersion,
                  ),
                  const SizedBox(height: 8),
                  if (state.info.message.isNotEmpty)
                    _InfoRow(label: 'Status', value: state.info.message),
                  if (state.uploadProgress > 0) ...[
                    const SizedBox(height: 16),
                    LinearProgressIndicator(
                      value: state.uploadProgress,
                      minHeight: 8,
                      borderRadius: BorderRadius.circular(8),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      '${(state.uploadProgress * 100).toStringAsFixed(0)}%',
                      textAlign: TextAlign.center,
                      style: const TextStyle(
                        fontSize: 12,
                        color: AppColors.textMuted,
                      ),
                    ),
                  ],
                  if (state.error != null) ...[
                    const SizedBox(height: 16),
                    _ErrorBanner(message: state.error!),
                  ],
                ],
              ),
            ),
          ),
          const SizedBox(height: 24),
          FilledButton.icon(
            onPressed: state.isUploading
                ? null
                : () => _pickAndUpload(context, ref),
            icon: const Icon(Icons.upload_file),
            label: Text(
              state.isUploading ? 'Uploading…' : 'Flash Firmware (.bin)',
            ),
          ),
          const SizedBox(height: 12),
          OutlinedButton(
            onPressed: () => ref.read(otaProvider.notifier).refresh(),
            child: const Text('Check for updates'),
          ),
        ],
      ),
    );
  }

  String _stateLabel(OtaState state) {
    switch (state) {
      case OtaState.complete:
        return 'Firmware up to date';
      case OtaState.error:
        return 'Update failed';
      case OtaState.downloading:
        return 'Downloading update';
      case OtaState.validating:
        return 'Validating update';
      case OtaState.applying:
        return 'Applying update';
      case OtaState.idle:
        return 'Firmware';
    }
  }

  static BadgeTone _toneForState(OtaState state) {
    switch (state) {
      case OtaState.complete:
        return BadgeTone.success;
      case OtaState.error:
        return BadgeTone.danger;
      case OtaState.downloading:
      case OtaState.validating:
      case OtaState.applying:
        return BadgeTone.warning;
      case OtaState.idle:
        return BadgeTone.neutral;
    }
  }
}

class _InfoRow extends StatelessWidget {
  const _InfoRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          label,
          style: const TextStyle(color: AppColors.textMuted, fontSize: 13),
        ),
        const SizedBox(width: 12),
        Expanded(
          child: Text(
            value,
            textAlign: TextAlign.right,
            style: const TextStyle(color: AppColors.textPrimary),
          ),
        ),
      ],
    );
  }
}

class _ErrorBanner extends StatelessWidget {
  const _ErrorBanner({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppColors.danger.withValues(alpha: 0.12),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Text(
        message,
        style: const TextStyle(color: AppColors.danger, fontSize: 13),
      ),
    );
  }
}
