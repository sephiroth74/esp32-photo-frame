import 'package:flutter/widgets.dart';
import 'package:shared_preferences/shared_preferences.dart';

class Preferences {
  static final Map<String, String> _memoryCache = {};
  static SharedPreferences? _prefs;
  static bool _initialized = false;

  static Future<void> initialize() async {
    if (_initialized) return;
    try {
      _prefs = await SharedPreferences.getInstance();
      _initialized = true;
    } catch (e) {
      debugPrint('Failed to initialize Preferences: $e');
    }
  }

  static Future<void> setString(String key, String value) async {
    _memoryCache[key] = value;
    if (_prefs != null) {
      try {
        await _prefs!.setString(key, value);
      } catch (e) {
        debugPrint('Failed to save preference for key $key: $e');
      }
    }
  }

  static String? getString(String key) {
    if (_memoryCache.containsKey(key)) {
      return _memoryCache[key];
    }
    if (_prefs != null) {
      final value = _prefs!.getString(key);
      if (value != null) {
        _memoryCache[key] = value;
        return value;
      }
    }
    return null;
  }

  static Future<void> clearAll() async {
    _memoryCache.clear();
    if (_prefs != null) {
      try {
        await _prefs!.clear();
      } catch (e) {
        debugPrint('Failed to clear preferences: $e');
      }
    }
  }

  /// Clear specific picker history
  static Future<void> clear(String key) async {
    _memoryCache.remove(key);

    if (_prefs != null) {
      try {
        await _prefs!.remove(key);
      } catch (e) {
        debugPrint('Failed to clear preference for key $key: $e');
      }
    }
  }
}
