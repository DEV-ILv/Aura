import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../models/ota_info.dart';
import '../repositories/ota_repository.dart';
import 'app_providers.dart';

/// OTA controller state.
class FirmwareState {
  const FirmwareState({
    this.info = const OtaInfo.unknown(),
    this.isUploading = false,
    this.uploadProgress = 0,
    this.error,
  });

  final OtaInfo info;
  final bool isUploading;
  final double uploadProgress;
  final String? error;

  FirmwareState copyWith({
    OtaInfo? info,
    bool? isUploading,
    double? uploadProgress,
    String? error,
  }) {
    return FirmwareState(
      info: info ?? this.info,
      isUploading: isUploading ?? this.isUploading,
      uploadProgress: uploadProgress ?? this.uploadProgress,
      error: error ?? this.error,
    );
  }
}

/// Owns firmware version checks and OTA uploads with progress reporting.
class OtaNotifier extends StateNotifier<FirmwareState> {
  OtaNotifier(this._repository) : super(const FirmwareState());

  final OtaRepository _repository;

  Future<void> refresh() async {
    try {
      final info = await _repository.fetchInfo();
      state = state.copyWith(info: info);
    } catch (_) {
      // Keep the last known version info; the OTA screen already renders a
      // "version unavailable" state when the connection is down.
    }
  }

  Future<void> upload(List<int> bytes, String filename) async {
    state = state.copyWith(isUploading: true, uploadProgress: 0);
    try {
      await _repository.uploadFirmware(
        bytes,
        filename,
        onProgress: (progress) {
          state = state.copyWith(uploadProgress: progress);
        },
      );
      state = state.copyWith(
        isUploading: false,
        uploadProgress: 1,
        info: state.info.copyWith(
          state: OtaState.complete,
          progress: 1,
          message: 'Firmware uploaded successfully.',
        ),
      );
    } catch (_) {
      state = state.copyWith(
        isUploading: false,
        error: 'Upload failed. Check the connection and try again.',
      );
    }
  }
}

final otaProvider = StateNotifierProvider<OtaNotifier, FirmwareState>((ref) {
  return OtaNotifier(ref.watch(otaRepositoryProvider));
});
