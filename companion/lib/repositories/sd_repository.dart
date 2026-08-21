import '../api/api_exception.dart';
import '../api/api_service.dart';
import '../core/constants/api_paths.dart';
import '../core/services/logger.dart';
import '../models/sd_file.dart';

/// Browse/upload/download/delete operations for device storage.
class SdRepository {
  SdRepository(this._api);

  final ApiService _api;

  /// Lists files at [path] (root when empty).
  Future<List<SdFile>> list(String path) async {
    try {
      final json = await _api.getJson(ApiPaths.sdList, query: {'path': path});
      final list = json['files'] as List? ?? json['entries'] as List? ?? [];
      return list
          .whereType<Map<String, dynamic>>()
          .map(SdFile.fromJson)
          .toList();
    } on ApiException catch (error) {
      Logger.warning('Failed to list SD files: ${error.message}');
      return const [];
    }
  }

  /// Uploads [bytes] as [filename] to currently-uploaded directory.
  Future<void> upload(
    List<int> bytes,
    String filename, {
    void Function(double progress)? onProgress,
  }) async {
    await _api.upload(
      ApiPaths.sdUpload,
      bytes,
      filename,
      onProgress: onProgress,
    );
  }

  /// Deletes a file or directory at [path].
  Future<void> delete(String path) async {
    await _api.postJson(ApiPaths.sdDelete, body: {'path': path});
  }

  /// Downloads raw bytes for the file at [path].
  Future<List<int>> download(String path) async {
    return _api.download(
      '${ApiPaths.sdDownload}?path=${Uri.encodeQueryComponent(path)}',
    );
  }
}
