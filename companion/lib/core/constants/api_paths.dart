/// REST resource paths exposed by the AURA firmware web portal.
abstract final class ApiPaths {
  static const String status = '/api/status';
  static const String wifi = '/api/wifi';
  static const String settings = '/api/settings';
  static const String performance = '/api/performance';
  static const String version = '/api/version';
  static const String dashboardSummary = '/api/dashboard/summary';
  static const String authLogin = '/api/auth/login';
  static const String authLogout = '/api/auth/logout';
  static const String authStatus = '/api/auth/status';
  static const String authChangePassword = '/api/auth/change-password';

  /// Conversation endpoint. The firmware exposes text messaging through this
  /// path; the client keeps the path isolated so streaming can be introduced
  /// later without touching callers.
  static const String chat = '/api/chat';

  // --- Phase 2 ---

  // Reminders (the firmware reminder surface is bootstrapped via this path).
  static const String reminders = '/api/reminders';
  static const String remindersCreate = '/api/reminders';
  static const String remindersDelete = '/api/reminders/delete';

  // Memories.
  static const String memoriesRanked = '/api/memories/ranked';
  static const String memoriesSearch = '/api/memories/search';
  static const String memoryPin = '/api/memory/pin';
  static const String memoryPinned = '/api/memory/pinned';
  static const String memoryArchived = '/api/memory/archived';
  static const String memoryRestore = '/api/memory/restore';

  // OTA. The firmware exposes a single browser-style multipart upload at
  // `/ota`; the JSON OTA state API is not implemented on the device yet.
  static const String ota = '/api/ota';
  static const String otaUpload = '/ota';
  static const String otaProgress = '/api/ota/progress';

  // SD card / file storage.
  static const String sdList = '/api/storage';
  static const String sdDownload = '/api/storage/download';
  static const String sdFiles = '/api/storage/files';
  static const String sdUpload = '/api/storage/upload';
  static const String sdDelete = '/api/storage/delete';
  static const String sdLogs = '/api/storage/logs';

  // Firmware state / monitor.
  static const String developer = '/api/developer';
  static const String developerExport = '/api/developer/export';

  // --- V2 Companion Device Control ---

  /// Live uptime stats (session + lifetime, boot count, reset reason).
  static const String uptime = '/api/uptime';

  /// Device physical controls (new in V2; all require a local LAN session).
  static const String wifiScan = '/api/wifi/scan';
  static const String wifiForget = '/api/wifi/forget';
  static const String displayControl = '/api/display/control';
  static const String ledControl = '/api/led/control';
  static const String audioControl = '/api/audio/control';
  static const String micControl = '/api/mic/control';
  static const String micLevel = '/api/mic/level';

  /// Existing system action endpoints.
  static const String restart = '/restart';
  static const String factoryReset = '/factory-reset';
}
