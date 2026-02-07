import 'package:flutter/rendering.dart';
import 'package:path/path.dart' as p;
import 'package:photoframe_flutter/core/services/preferences.dart';
import 'package:shared_preferences/shared_preferences.dart';

class FilePickerHistory {
  // Storage key prefix for file picker directories
  static const String _keyPrefix = 'file_picker_history_';

  /// Get initial directory for file picker
  static String? initialDir(String key) {
    // Then check persistent storage
    final savedPath = Preferences.getString(_keyPrefix + key);
    if (savedPath != null && savedPath.isNotEmpty) {
      return savedPath;
    }

    return null;
  }

  /// Remember directory for future file picker calls
  static Future<void> rememberDirectory(String key, String directoryPath) async {
    if (directoryPath.isEmpty) return;
    // Save to persistent storage
    try {
      await Preferences.setString(_keyPrefix + key, directoryPath);
    } catch (e) {
      debugPrint('Failed to save directory to preferences: $e');
    }
  }

  /// Remember file (extracts directory and saves it)
  static Future<void> rememberFile(String key, String filePath) async {
    final directory = p.dirname(filePath);
    await rememberDirectory(key, directory);
  }

  /// Clear all saved picker histories
  static Future<void> clearAll() async {
    Preferences.clearAll();
  }

  /// Clear specific picker history
  static Future<void> clear(String key) async {
    Preferences.clear(key);
  }
}
