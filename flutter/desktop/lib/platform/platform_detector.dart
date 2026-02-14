import 'dart:io' show Platform;
import 'package:flutter/foundation.dart';

/// Supported desktop platforms for the app.
enum AppPlatform { macos, windows, linux, unknown }

/// Utility to detect the current platform.
class PlatformDetector {
  static AppPlatform get current {
    if (kIsWeb) {
      return AppPlatform.unknown;
    }

    if (Platform.isMacOS) return AppPlatform.macos;
    if (Platform.isWindows) return AppPlatform.windows;
    if (Platform.isLinux) return AppPlatform.linux;

    return AppPlatform.unknown;
  }

  static bool get isDesktop =>
      current == AppPlatform.macos ||
      current == AppPlatform.windows ||
      current == AppPlatform.linux;
}
