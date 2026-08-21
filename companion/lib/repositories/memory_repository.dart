import '../api/api_exception.dart';
import '../api/api_service.dart';
import '../core/constants/api_paths.dart';
import '../core/services/logger.dart';
import '../models/memory_item.dart';

/// Read/search operations for the device memory store.
class MemoryRepository {
  MemoryRepository(this._api);

  final ApiService _api;

  /// Fetches the ranked memories list.
  Future<List<MemoryItem>> fetchAll() async {
    try {
      final json = await _api.getJson(ApiPaths.memoriesRanked);
      return _parseList(json);
    } on ApiException catch (error) {
      Logger.warning('Failed to fetch memories: ${error.message}');
      return const [];
    }
  }

  /// Searches memories by query string.
  Future<List<MemoryItem>> search(String query) async {
    try {
      final json = await _api.getJson(
        ApiPaths.memoriesSearch,
        query: {'q': query},
      );
      return _parseList(json);
    } on ApiException catch (error) {
      Logger.warning('Failed to search memories: ${error.message}');
      return const [];
    }
  }

  /// Fetches pinned memories.
  Future<List<MemoryItem>> fetchPinned() async {
    return _fetchSubset(ApiPaths.memoryPinned);
  }

  /// Pins a memory to the top of the ranked list.
  Future<void> pin(MemoryItem memory) async {
    await _api.postJson(ApiPaths.memoryPin, query: {'id': memory.id});
  }

  /// Restores an archived memory by revision.
  Future<void> restore(MemoryItem memory) async {
    await _api.postJson(
      ApiPaths.memoryRestore,
      query: {'revisionId': memory.id},
    );
  }

  List<MemoryItem> _parseList(Map<String, dynamic> json) {
    final candidates = <List?>[
      json['memories'] as List?,
      json['results'] as List?,
      json['entries'] as List?,
    ];
    for (final list in candidates) {
      if (list != null) {
        return list
            .whereType<Map<String, dynamic>>()
            .map(MemoryItem.fromJson)
            .toList();
      }
    }
    return const [];
  }

  Future<List<MemoryItem>> _fetchSubset(String path) async {
    try {
      final json = await _api.getJson(path);
      return _parseList(json);
    } on ApiException catch (error) {
      Logger.warning('Failed to fetch memories: ${error.message}');
      return const [];
    }
  }
}
