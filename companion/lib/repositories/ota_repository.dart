import '../api/api_exception.dart';
import '../api/api_service.dart';
import '../core/constants/api_paths.dart';
import '../core/services/logger.dart';
import '../models/ota_info.dart';

/// Firmware update operations: version query and firmware upload.
///
/// The firmware exposes the current version through `/api/version` and
/// accepts firmware as a browser-style multipart upload at `/ota`. The JSON
/// OTA state API is not implemented on the device, so progress is derived
/// from the upload transfer.
class OtaRepository {
  OtaRepository(this._api);

  final ApiService _api;

  /// Queries the current firmware version.
  Future<OtaInfo> fetchInfo() async {
    try {
      final version = await _api.fetchVersion();
      return OtaInfo(
        currentVersion: version.version,
        state: OtaState.idle,
        message: version.codename.isEmpty ? '' : version.codename,
      );
    } on ApiException catch (error) {
      Logger.warning('Failed to query firmware version: ${error.message}');
      return const OtaInfo.unknown();
    }
  }

  /// Uploads firmware bytes (`.bin`) and reports progress in real time.
  Future<void> uploadFirmware(
    List<int> bytes,
    String filename, {
    void Function(double progress)? onProgress,
  }) async {
    await _api.upload(
      ApiPaths.otaUpload,
      bytes,
      filename,
      field: 'firmware',
      onProgress: onProgress,
    );
  }
}
