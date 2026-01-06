import 'package:flutter/services.dart';

class FontService {
  static const _channel = MethodChannel('com.photoframe/fonts');

  static Future<List<String>> getSystemFonts() async {
    try {
      final List<dynamic> fonts = await _channel.invokeMethod('getSystemFonts');
      return fonts.cast<String>();
    } catch (e) {
      // Fallback to common fonts if method channel fails
      return [
        'Arial',
        'Arial Black',
        'Helvetica',
        'Helvetica Neue',
        'Times New Roman',
        'Courier',
        'Courier New',
        'Georgia',
        'Verdana',
        'Trebuchet MS',
        'Impact',
        'Comic Sans MS',
        'Palatino',
        'Lucida Grande',
        'Monaco',
        'Menlo',
      ];
    }
  }
}
