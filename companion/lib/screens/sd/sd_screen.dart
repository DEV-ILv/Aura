import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';

import '../../core/theme/app_colors.dart';
import '../../core/utils/formatters.dart';
import '../../models/sd_file.dart';
import '../../providers/sd_provider.dart';

/// SD card file manager.
class SdScreen extends ConsumerStatefulWidget {
  const SdScreen({super.key});

  @override
  ConsumerState<SdScreen> createState() => _SdScreenState();
}

class _SdScreenState extends ConsumerState<SdScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      ref.read(sdProvider.notifier).refresh();
    });
  }

  Future<void> _pickAndUpload() async {
    final result = await FilePicker.platform.pickFiles();
    if (result == null || result.files.isEmpty) {
      return;
    }
    final file = result.files.single;
    final bytes = await file.xFile.readAsBytes();
    await ref.read(sdProvider.notifier).upload(bytes, file.name);
  }

  Future<void> _download(SdFile file) async {
    if (file.isDirectory) {
      return;
    }
    final bytes = await ref.read(sdProvider.notifier).download(file.path);
    final directory = await getApplicationDocumentsDirectory();
    final target = File(
      '${directory.path}${Platform.pathSeparator}${file.name}',
    );
    await target.writeAsBytes(bytes);
    if (!mounted) {
      return;
    }
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text('Saved to ${target.path}')));
  }

  Future<void> _confirmDelete(SdFile file) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: Text('Delete ${file.name}?'),
        content: const Text('This cannot be undone.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Delete'),
          ),
        ],
      ),
    );
    if (confirmed == true) {
      await ref.read(sdProvider.notifier).delete(file.path);
    }
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(sdProvider);
    final notifier = ref.read(sdProvider.notifier);

    return Scaffold(
      appBar: AppBar(
        title: const Text('SD Card'),
        actions: [
          IconButton(
            tooltip: 'Refresh',
            onPressed: notifier.reload,
            icon: const Icon(Icons.refresh),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: state.isUploading ? null : _pickAndUpload,
        icon: const Icon(Icons.upload),
        label: const Text('Upload'),
      ),
      body: Column(
        children: [
          _BreadcrumbBar(
            path: state.path,
            onUp: notifier.goUp,
            canGoUp: state.path != '/',
          ),
          if (state.isUploading) ...[
            LinearProgressIndicator(value: state.uploadProgress, minHeight: 3),
          ],
          if (state.error != null) _ErrorBanner(message: state.error!),
          Expanded(child: _body(state)),
        ],
      ),
    );
  }

  Widget _body(SdState state) {
    if (state.isLoading) {
      return const Center(child: CircularProgressIndicator());
    }
    if (state.files.isEmpty) {
      return const Center(child: Text('This folder is empty'));
    }
    return ListView.separated(
      padding: const EdgeInsets.only(bottom: 96),
      itemCount: state.files.length,
      separatorBuilder: (_, _) => const Divider(height: 1),
      itemBuilder: (context, index) {
        final file = state.files[index];
        return _FileTile(
          file: file,
          onOpen: file.isDirectory
              ? () => ref.read(sdProvider.notifier).enterDirectory(file)
              : null,
          onDownload: file.isDirectory ? null : () => _download(file),
          onDelete: () => _confirmDelete(file),
        );
      },
    );
  }
}

class _BreadcrumbBar extends StatelessWidget {
  const _BreadcrumbBar({
    required this.path,
    required this.onUp,
    required this.canGoUp,
  });

  final String path;
  final VoidCallback onUp;
  final bool canGoUp;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      color: AppColors.surface,
      child: Row(
        children: [
          IconButton(
            icon: const Icon(Icons.arrow_upward),
            tooltip: 'Up one level',
            onPressed: canGoUp ? onUp : null,
          ),
          Expanded(
            child: Text(
              path,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }
}

class _FileTile extends StatelessWidget {
  const _FileTile({
    required this.file,
    this.onOpen,
    this.onDownload,
    required this.onDelete,
  });

  final SdFile file;
  final VoidCallback? onOpen;
  final VoidCallback? onDownload;
  final VoidCallback onDelete;

  @override
  Widget build(BuildContext context) {
    return ListTile(
      onTap: onOpen,
      leading: Icon(
        file.isDirectory ? Icons.folder : _iconForFile(file.name),
        color: file.isDirectory ? AppColors.warning : AppColors.primary,
      ),
      title: Text(file.name, maxLines: 1, overflow: TextOverflow.ellipsis),
      subtitle: file.isDirectory ? null : Text(Formatters.bytes(file.size)),
      trailing: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          if (onDownload != null)
            IconButton(
              tooltip: 'Download',
              onPressed: onDownload,
              icon: const Icon(Icons.download),
            ),
          IconButton(
            tooltip: 'Delete',
            onPressed: onDelete,
            icon: const Icon(Icons.delete_outline),
          ),
        ],
      ),
    );
  }
}

IconData _iconForFile(String name) {
  final lower = name.toLowerCase();
  if (lower.endsWith('.bin')) {
    return Icons.memory;
  }
  if (lower.endsWith('.mp3') ||
      lower.endsWith('.wav') ||
      lower.endsWith('.aac')) {
    return Icons.audiotrack;
  }
  if (lower.endsWith('.png') ||
      lower.endsWith('.jpg') ||
      lower.endsWith('.jpeg')) {
    return Icons.image;
  }
  return Icons.insert_drive_file;
}

class _ErrorBanner extends StatelessWidget {
  const _ErrorBanner({required this.message});

  final String message;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: double.infinity,
      color: AppColors.danger.withValues(alpha: 0.12),
      padding: const EdgeInsets.all(12),
      child: Text(
        message,
        style: const TextStyle(color: AppColors.danger, fontSize: 13),
      ),
    );
  }
}
