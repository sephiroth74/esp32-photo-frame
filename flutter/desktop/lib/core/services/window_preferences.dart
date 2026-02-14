import 'dart:convert';
import 'dart:io';
import 'dart:ui';
import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';
import 'package:window_manager/window_manager.dart';

class WindowPreferences {
  static const String _filename = 'window_preferences.json';

  static Future<void> saveWindowSize() async {
    try {
      final size = await windowManager.getSize();
      final position = await windowManager.getPosition();

      final prefs = {
        'width': size.width,
        'height': size.height,
        'x': position.dx,
        'y': position.dy,
      };

      final directory = await getApplicationSupportDirectory();
      final file = File('${directory.path}/$_filename');
      await file.writeAsString(jsonEncode(prefs));
    } catch (e) {
      debugPrint('Failed to save window preferences: $e');
    }
  }

  static Future<void> restoreWindowSize() async {
    try {
      final directory = await getApplicationSupportDirectory();
      final file = File('${directory.path}/$_filename');

      if (!await file.exists()) {
        // Set default size if no preferences exist
        await windowManager.setSize(const Size(900, 800));
        await windowManager.center();
        return;
      }

      final json = jsonDecode(await file.readAsString());
      final width = json['width'] as double;
      final height = json['height'] as double;
      final x = json['x'] as double;
      final y = json['y'] as double;

      await windowManager.setSize(Size(width, height));
      await windowManager.setPosition(Offset(x, y));
    } catch (e) {
      debugPrint('Failed to restore window preferences: $e');
      // Fall back to default size
      await windowManager.setSize(const Size(900, 800));
      await windowManager.center();
    }
  }
}
