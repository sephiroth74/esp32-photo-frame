import 'package:path/path.dart' as p;
import 'package:shared_preferences/shared_preferences.dart';

class FilePickerHistory {
  static final Map<String, String> _memoryCache = {};
  static SharedPreferences? _prefs;
  static bool _initialized = false;

  // Storage key prefix for file picker directories
  static const String _keyPrefix = 'file_picker_history_';

  /// Initialize shared preferences (call once at app startup)
  static Future<void> initialize() async {
    if (_initialized) return;
    try {
      _prefs = await SharedPreferences.getInstance();
      _initialized = true;
    } catch (e) {
      print('Failed to initialize FilePickerHistory: $e');
    }
  }

  /// Get initial directory for file picker
  static String? initialDir(String key) {
    // First check memory cache (fastest)
    if (_memoryCache.containsKey(key)) {
      return _memoryCache[key];
    }

    // Then check persistent storage
    if (_prefs != null) {
      final savedPath = _prefs!.getString(_keyPrefix + key);
      if (savedPath != null && savedPath.isNotEmpty) {
        _memoryCache[key] = savedPath;
        return savedPath;
      }
    }

    return null;
  }

  /// Remember directory for future file picker calls
  static Future<void> rememberDirectory(String key, String directoryPath) async {
    if (directoryPath.isEmpty) return;

    // Save to memory cache
    _memoryCache[key] = directoryPath;

    // Save to persistent storage
    if (_prefs != null) {
      try {
        await _prefs!.setString(_keyPrefix + key, directoryPath);
      } catch (e) {
        print('Failed to save directory to preferences: $e');
      }
    }
  }

  /// Remember file (extracts directory and saves it)
  static Future<void> rememberFile(String key, String filePath) async {
    final directory = p.dirname(filePath);
    await rememberDirectory(key, directory);
  }

  /// Clear all saved picker histories
  static Future<void> clearAll() async {
    _memoryCache.clear();

    if (_prefs != null) {
      try {
        final keys = _prefs!.getKeys();
        for (final key in keys) {
          if (key.startsWith(_keyPrefix)) {
            await _prefs!.remove(key);
          }
        }
      } catch (e) {
        print('Failed to clear preferences: $e');
      }
    }
  }

  /// Clear specific picker history
  static Future<void> clear(String key) async {
    _memoryCache.remove(key);

    if (_prefs != null) {
      try {
        await _prefs!.remove(_keyPrefix + key);
      } catch (e) {
        print('Failed to clear preference for key $key: $e');
      }
    }
  }
}
