import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../core/services/logger.dart';
import '../models/memory_item.dart';
import '../repositories/memory_repository.dart';
import 'app_providers.dart';

/// Memory browser state.
class MemoryState {
  const MemoryState({
    required this.memories,
    required this.isLoading,
    this.query = '',
    this.activeCategory = 'All',
  });

  const MemoryState.initial()
    : memories = const [],
      isLoading = true,
      query = '',
      activeCategory = 'All';

  final List<MemoryItem> memories;
  final bool isLoading;
  final String query;
  final String activeCategory;

  /// Distinct categories present in the current data set.
  List<String> get categories {
    final set = <String>{'All'};
    for (final memory in memories) {
      set.add(memory.category);
    }
    return set.toList()..sort();
  }

  /// Memories filtered by active category and query.
  List<MemoryItem> get filtered {
    var result = memories;
    if (activeCategory != 'All') {
      result = result.where((m) => m.category == activeCategory).toList();
    }
    if (query.trim().isNotEmpty) {
      final q = query.trim().toLowerCase();
      result = result
          .where((m) => m.content.toLowerCase().contains(q))
          .toList();
    }
    return result;
  }

  MemoryState copyWith({
    List<MemoryItem>? memories,
    bool? isLoading,
    String? query,
    String? activeCategory,
  }) {
    final resolvedCategory = activeCategory;
    return MemoryState(
      memories: memories ?? this.memories,
      isLoading: isLoading ?? this.isLoading,
      query: query ?? this.query,
      activeCategory: resolvedCategory ?? this.activeCategory,
    );
  }
}

/// Owns the memory list, search and category filtering.
class MemoryNotifier extends StateNotifier<MemoryState> {
  MemoryNotifier(this._repository) : super(const MemoryState.initial());

  final MemoryRepository _repository;

  Future<void> load() async {
    state = const MemoryState(memories: [], isLoading: true);
    try {
      final memories = await _repository.fetchAll();
      state = MemoryState(memories: memories, isLoading: false);
    } catch (error) {
      Logger.warning('Memory load failed: $error');
      state = const MemoryState(memories: [], isLoading: false);
    }
  }

  Future<void> search(String query) async {
    state = state.copyWith(query: query);
    final trimmed = query.trim();
    if (trimmed.isEmpty) {
      try {
        state = state.copyWith(memories: await _repository.fetchAll());
      } catch (error) {
        Logger.warning('Memory reload failed: $error');
        state = state.copyWith(isLoading: false);
      }
      return;
    }
    try {
      final results = await _repository.search(trimmed);
      state = MemoryState(memories: results, isLoading: false, query: query);
    } catch (error) {
      Logger.warning('Memory search failed: $error');
      state = state.copyWith(isLoading: false, query: query);
    }
  }

  Future<void> setCategory(String category) async {
    state = state.copyWith(activeCategory: category);
  }

  /// Pins a memory to the top of the ranked list.
  Future<void> pin(MemoryItem memory) async {
    try {
      await _repository.pin(memory);
    } catch (_) {
      // Best-effort; the device may not support pinning.
    }
    await load();
  }

  /// Restores an archived memory.
  Future<void> restore(MemoryItem memory) async {
    try {
      await _repository.restore(memory);
    } catch (_) {
      // Best-effort; the device may not support restore.
    }
    await load();
  }
}

final memoryProvider = StateNotifierProvider<MemoryNotifier, MemoryState>((
  ref,
) {
  return MemoryNotifier(ref.watch(memoryRepositoryProvider));
});
