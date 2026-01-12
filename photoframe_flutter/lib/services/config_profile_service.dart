import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:path/path.dart' as path;
import 'package:path_provider/path_provider.dart';
import '../models/config_profile.dart';
import '../models/processing_config.dart';

class ConfigProfileService {
  static const String _preferencesFileName = 'app_preferences.json';
  static const String _configFileExtension = '.pfconfig';

  static Future<Directory> _getConfigDirectory() async {
    final appSupportDir = await getApplicationSupportDirectory();
    final configDir = Directory(path.join(appSupportDir.path, 'PhotoFrameProcessor', 'configs'));
    if (!await configDir.exists()) {
      await configDir.create(recursive: true);
    }
    return configDir;
  }

  static Future<File> _getPreferencesFile() async {
    final appSupportDir = await getApplicationSupportDirectory();
    final prefsDir = Directory(path.join(appSupportDir.path, 'PhotoFrameProcessor'));
    if (!await prefsDir.exists()) {
      await prefsDir.create(recursive: true);
    }
    return File(path.join(prefsDir.path, _preferencesFileName));
  }

  // Load app preferences
  static Future<AppPreferences> loadPreferences() async {
    try {
      final file = await _getPreferencesFile();
      if (await file.exists()) {
        final contents = await file.readAsString();
        final json = jsonDecode(contents);
        return AppPreferences.fromJson(json);
      }
    } catch (e) {
      debugPrint('Error loading preferences: $e');
    }
    return const AppPreferences();
  }

  // Save app preferences
  static Future<void> savePreferences(AppPreferences preferences) async {
    try {
      final file = await _getPreferencesFile();
      final json = jsonEncode(preferences.toJson());
      await file.writeAsString(json, flush: true);
    } catch (e) {
      debugPrint('Error saving preferences: $e');
    }
  }

  // Add to recent files
  static Future<void> addToRecentFiles(String filePath) async {
    final prefs = await loadPreferences();
    final fileName = path.basename(filePath);

    // Remove if already exists (to move it to top)
    final recentFiles = List<RecentFile>.from(prefs.recentFiles);
    recentFiles.removeWhere((f) => f.path == filePath);

    // Add to beginning
    recentFiles.insert(0, RecentFile(
      path: filePath,
      name: fileName.replaceAll(_configFileExtension, ''),
      lastOpened: DateTime.now(),
    ));

    // Limit to max recent files
    if (recentFiles.length > prefs.maxRecentFiles) {
      recentFiles.removeRange(prefs.maxRecentFiles, recentFiles.length);
    }

    await savePreferences(prefs.copyWith(
      recentFiles: recentFiles,
      lastOpenedProfile: filePath,
    ));
  }

  // Clear recent files
  static Future<void> clearRecentFiles() async {
    final prefs = await loadPreferences();
    await savePreferences(prefs.copyWith(recentFiles: []));
  }

  // Save configuration profile
  static Future<String> saveProfile(ProcessingConfig config, {String? filePath, String? name}) async {
    try {
      String targetPath;
      String profileName;

      if (filePath != null) {
        targetPath = filePath;
        profileName = path.basenameWithoutExtension(filePath);
      } else {
        final configDir = await _getConfigDirectory();
        profileName = name ?? 'config_${DateTime.now().millisecondsSinceEpoch}';
        targetPath = path.join(configDir.path, '$profileName$_configFileExtension');
      }

      final profile = ConfigProfile(
        name: profileName,
        filePath: targetPath,
        lastModified: DateTime.now(),
        config: config,
      );

      final file = File(targetPath);
      final json = const JsonEncoder.withIndent('  ').convert(profile.toJson());
      await file.writeAsString(json, flush: true);

      // Add to recent files
      await addToRecentFiles(targetPath);

      return targetPath;
    } catch (e) {
      throw Exception('Failed to save configuration: $e');
    }
  }

  // Load configuration profile
  static Future<ConfigProfile> loadProfile(String filePath) async {
    try {
      final file = File(filePath);
      if (!await file.exists()) {
        throw Exception('Configuration file not found: $filePath');
      }

      final contents = await file.readAsString();
      final json = jsonDecode(contents);
      final profile = ConfigProfile.fromJson(json);

      // Add to recent files
      await addToRecentFiles(filePath);

      return profile;
    } catch (e) {
      throw Exception('Failed to load configuration: $e');
    }
  }

  // Get list of saved profiles
  static Future<List<ConfigProfile>> getSavedProfiles() async {
    try {
      final configDir = await _getConfigDirectory();
      final files = await configDir
          .list()
          .where((entity) => entity is File && entity.path.endsWith(_configFileExtension))
          .cast<File>()
          .toList();

      final profiles = <ConfigProfile>[];
      for (final file in files) {
        try {
          final contents = await file.readAsString();
          final json = jsonDecode(contents);
          profiles.add(ConfigProfile.fromJson(json));
        } catch (e) {
          debugPrint('Error loading profile ${file.path}: $e');
        }
      }

      // Sort by last modified
      profiles.sort((a, b) => b.lastModified.compareTo(a.lastModified));
      return profiles;
    } catch (e) {
      debugPrint('Error getting saved profiles: $e');
      return [];
    }
  }

  // Delete configuration profile
  static Future<void> deleteProfile(String filePath) async {
    try {
      final file = File(filePath);
      if (await file.exists()) {
        await file.delete();
      }

      // Remove from recent files
      final prefs = await loadPreferences();
      final recentFiles = List<RecentFile>.from(prefs.recentFiles);
      recentFiles.removeWhere((f) => f.path == filePath);
      await savePreferences(prefs.copyWith(recentFiles: recentFiles));
    } catch (e) {
      throw Exception('Failed to delete configuration: $e');
    }
  }

  // Export configuration to any location
  static Future<void> exportProfile(ConfigProfile profile, String destinationPath) async {
    try {
      final file = File(destinationPath);
      final json = const JsonEncoder.withIndent('  ').convert(profile.toJson());
      await file.writeAsString(json, flush: true);
    } catch (e) {
      throw Exception('Failed to export configuration: $e');
    }
  }
}