import 'package:flutter/material.dart' hide ConnectionState;
import 'package:flutter_riverpod/flutter_riverpod.dart';

import '../../core/theme/app_colors.dart';
import '../../models/memory_item.dart';
import '../../providers/memory_provider.dart';
import '../../widgets/status_badge.dart';

/// Memory browser with search and category filtering.
class MemoryScreen extends ConsumerStatefulWidget {
  const MemoryScreen({super.key});

  @override
  ConsumerState<MemoryScreen> createState() => _MemoryScreenState();
}

class _MemoryScreenState extends ConsumerState<MemoryScreen> {
  final _searchController = TextEditingController();

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      ref.read(memoryProvider.notifier).load();
    });
  }

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final state = ref.watch(memoryProvider);
    final notifier = ref.read(memoryProvider.notifier);

    return Scaffold(
      appBar: AppBar(title: const Text('Memory')),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.fromLTRB(16, 12, 16, 4),
            child: TextField(
              controller: _searchController,
              onChanged: notifier.search,
              decoration: const InputDecoration(
                hintText: 'Search memories…',
                prefixIcon: Icon(Icons.search),
              ),
            ),
          ),
          _CategoryChips(
            categories: state.categories,
            active: state.activeCategory,
            onSelected: notifier.setCategory,
          ),
          Expanded(child: _body(state)),
        ],
      ),
    );
  }

  Widget _body(MemoryState state) {
    if (state.isLoading) {
      return const Center(child: CircularProgressIndicator());
    }
    final memories = state.filtered;
    if (memories.isEmpty) {
      return const _EmptyState();
    }
    return ListView.separated(
      padding: const EdgeInsets.all(16),
      itemCount: memories.length,
      separatorBuilder: (_, _) => const SizedBox(height: 8),
      itemBuilder: (context, index) => _MemoryTile(memory: memories[index]),
    );
  }
}

class _EmptyState extends StatelessWidget {
  const _EmptyState();

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Column(
        mainAxisAlignment: MainAxisAlignment.center,
        children: [
          const Icon(Icons.memory, color: AppColors.textMuted, size: 48),
          const SizedBox(height: 16),
          Text(
            'No memories found',
            style: Theme.of(context).textTheme.titleMedium,
          ),
        ],
      ),
    );
  }
}

class _CategoryChips extends StatelessWidget {
  const _CategoryChips({
    required this.categories,
    required this.active,
    required this.onSelected,
  });

  final List<String> categories;
  final String active;
  final ValueChanged<String> onSelected;

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 48,
      child: ListView.separated(
        scrollDirection: Axis.horizontal,
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        itemCount: categories.length,
        separatorBuilder: (_, _) => const SizedBox(width: 8),
        itemBuilder: (context, index) {
          final category = categories[index];
          return FilterChip(
            label: Text(category),
            selected: category == active,
            onSelected: (_) => onSelected(category),
          );
        },
      ),
    );
  }
}

class _MemoryTile extends ConsumerWidget {
  const _MemoryTile({required this.memory});

  final MemoryItem memory;

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    memory.content,
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                  if (memory.pinned) ...[
                    const SizedBox(height: 8),
                    const StatusBadge(
                      label: 'Pinned',
                      tone: BadgeTone.accent,
                      icon: Icons.push_pin,
                    ),
                  ],
                ],
              ),
            ),
            if (!memory.archived)
              IconButton(
                tooltip: memory.pinned ? 'Pinned' : 'Pin to top',
                onPressed: () => ref.read(memoryProvider.notifier).pin(memory),
                icon: Icon(
                  memory.pinned ? Icons.push_pin : Icons.push_pin_outlined,
                ),
              ),
            if (memory.archived)
              IconButton(
                tooltip: 'Restore from archive',
                onPressed: () =>
                    ref.read(memoryProvider.notifier).restore(memory),
                icon: const Icon(Icons.unarchive),
              ),
          ],
        ),
      ),
    );
  }
}
