import 'package:shared_preferences/shared_preferences.dart';

/// Thin typed wrapper over [SharedPreferences].
///
/// Centralizing persistence keeps storage keys and serialization in one
/// place so the rest of the app depends on an interface, not on the platform
/// implementation.
class StorageService {
  StorageService(this._prefs);

  final SharedPreferences _prefs;

  /// Retrieves all stored keys. Useful for diagnostics.
  Future<void> clear() => _prefs.clear();

  Future<String?> readString(String key) async => _prefs.getString(key);

  Future<bool> readBool(String key, {bool fallback = false}) async =>
      _prefs.getBool(key) ?? fallback;

  Future<int> readInt(String key, {int fallback = 0}) async =>
      _prefs.getInt(key) ?? fallback;

  Future<double> readDouble(String key, {double fallback = 0}) async =>
      _prefs.getDouble(key) ?? fallback;

  Future<void> writeString(String key, String value) =>
      _prefs.setString(key, value);

  Future<void> writeBool(String key, bool value) => _prefs.setBool(key, value);

  Future<void> writeInt(String key, int value) => _prefs.setInt(key, value);

  Future<void> writeDouble(String key, double value) =>
      _prefs.setDouble(key, value);

  Future<void> remove(String key) => _prefs.remove(key);
}
