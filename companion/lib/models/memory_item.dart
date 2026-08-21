/// A single stored memory with category metadata.
class MemoryItem {
  const MemoryItem({
    required this.id,
    required this.content,
    required this.createdAt,
    this.category = 'general',
    this.score = 0,
    this.pinned = false,
    this.archived = false,
  });

  final String id;
  final String content;
  final DateTime createdAt;
  final String category;
  final double score;
  final bool pinned;
  final bool archived;

  factory MemoryItem.fromJson(Map<String, dynamic> json) {
    // Timestamps may arrive as seconds, ms, or a JSON date string.
    DateTime createdAt;
    final ts = json['created_at'] ?? json['timestamp'];
    if (ts is num) {
      final ms = ts.abs() > 10000000000 ? ts : ts * 1000;
      createdAt = DateTime.fromMillisecondsSinceEpoch(ms.toInt());
    } else {
      createdAt = DateTime.tryParse(ts?.toString() ?? '') ?? DateTime.now();
    }
    return MemoryItem(
      id: json['id']?.toString() ?? '',
      content: json['content'] as String? ?? json['text'] as String? ?? '',
      createdAt: createdAt,
      category: json['category'] as String? ?? 'general',
      score: (json['score'] as num?)?.toDouble() ?? 0,
      pinned: json['pinned'] as bool? ?? false,
      archived: json['archived'] as bool? ?? false,
    );
  }
}
