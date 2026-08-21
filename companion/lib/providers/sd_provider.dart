import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../core/services/logger.dart';
import '../models/sd_file.dart';
import '../repositories/sd_repository.dart';
import 'app_providers.dart';

/// SD browser state.
class SdState {
  const SdState({
    required this.path,
    required this.files,
    required this.isLoading,
    this.isUploading = false,
    this.uploadProgress = 0,
    this.error,
  });

  const SdState.initial()
    : path = '/',
      files = const [],
      isLoading = true,
      isUploading = false,
      uploadProgress = 0,
      error = null;

  final String path;
  final List<SdFile> files;
  final bool isLoading;
  final bool isUploading;
  final double uploadProgress;
  final String? error;

  SdState copyWith({
    String? path,
    List<SdFile>? files,
    bool? isLoading,
    bool? isUploading,
    double? uploadProgress,
    String? error,
  }) {
    return SdState(
      path: path ?? this.path,
      files: files ?? this.files,
      isLoading: isLoading ?? this.isLoading,
      isUploading: isUploading ?? this.isUploading,
      uploadProgress: uploadProgress ?? this.uploadProgress,
      error: error ?? this.error,
    );
  }
}

/// Owns SD card browsing, upload, download and deletion.
class SdNotifier extends StateNotifier<SdState> {
  SdNotifier(this._repository) : super(const SdState.initial());

  final SdRepository _repository;
  final List<String> _history = [];

  Future<void> refresh() => getNext('');

  /// Loads files at [path]; when [path] is empty, keeps the current path.
  Future<void> getNext(String path) async {
    final target = path.isEmpty ? state.path : path;
    state = state.copyWith(isLoading: true);
    try {
      final files = await _repository.list(target);
      state = SdState(path: target, files: files, isLoading: false);
    } on Exception catch (error) {
      Logger.warning('SD listing failed: $error');
      state = state.copyWith(
        isLoading: false,
        error: 'Could not load the directory. Check the connection.',
      );
    }
  }

  /// Refreshes the current directory listing.
  Future<void> reload() => getNext(state.path);

  /// Navigates into [directory], stacking the previous path.
  Future<void> enterDirectory(SdFile directory) async {
    _history.add(state.path);
    await getNext(directory.path);
  }

  /// Navigates back one directory level.
  Future<void> goUp() async {
    if (_history.isNotEmpty) {
      final target = _history.removeLast();
      await getNext(target);
      return;
    }
    final parts = state.path.split('/').where((p) => p.isNotEmpty).toList();
    if (parts.length <= 1) {
      await getNext('/');
      return;
    }
    parts.removeLast();
    await getNext('/${parts.join('/')}');
  }

  Future<void> delete(String path) async {
    try {
      await _repository.delete(path);
      await reload();
    } on Exception catch (error) {
      Logger.warning('SD delete failed: $error');
      state = state.copyWith(error: 'Could not delete the file.');
    }
  }

  /// Uploads bytes from the caller (see screen for file picking).
  Future<void> upload(List<int> bytes, String filename) async {
    state = state.copyWith(isUploading: true, uploadProgress: 0);
    try {
      await _repository.upload(
        bytes,
        filename,
        onProgress: (progress) {
          state = state.copyWith(uploadProgress: progress);
        },
      );
      state = state.copyWith(isUploading: false, uploadProgress: 1);
      await reload();
    } on Exception catch (error) {
      Logger.warning('SD upload failed: $error');
      state = state.copyWith(
        isUploading: false,
        error: 'Upload failed. Check the connection.',
      );
    }
  }

  /// Downloads bytes for the file at [path].
  Future<List<int>> download(String path) {
    return _repository.download(path);
  }
}

final sdProvider = StateNotifierProvider<SdNotifier, SdState>((ref) {
  return SdNotifier(ref.watch(sdRepositoryProvider));
});
