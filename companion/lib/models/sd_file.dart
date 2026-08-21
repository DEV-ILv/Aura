/// A file or directory entry on the device SD card / storage.
class SdFile {
  const SdFile({
    required this.name,
    required this.path,
    required this.isDirectory,
    this.size = 0,
  });

  final String name;
  final String path;
  final bool isDirectory;
  final int size;

  factory SdFile.fromJson(Map<String, dynamic> json) {
    return SdFile(
      name: json['name'] as String? ?? '',
      path: json['path'] as String? ?? json['name'] as String? ?? '',
      isDirectory:
          json['dir'] as bool? ??
          json['is_dir'] as bool? ??
          json['directory'] as bool? ??
          false,
      size: (json['size'] as num?)?.toInt() ?? 0,
    );
  }
}
