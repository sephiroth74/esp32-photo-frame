import 'package:flutter/widgets.dart';
import 'platform_detector.dart';

/// Window configuration per platform.
class PlatformWindowConfig {
  final Size defaultSize;
  final Size minSize;
  final bool centerOnScreen;

  const PlatformWindowConfig({required this.defaultSize, required this.minSize, this.centerOnScreen = true});

  static const PlatformWindowConfig macos = PlatformWindowConfig(defaultSize: Size(1200, 800), minSize: Size(900, 600));

  static const PlatformWindowConfig windows = PlatformWindowConfig(defaultSize: Size(1280, 800), minSize: Size(900, 600));

  static const PlatformWindowConfig linux = PlatformWindowConfig(defaultSize: Size(1200, 800), minSize: Size(900, 600));

  static const PlatformWindowConfig fallback = PlatformWindowConfig(defaultSize: Size(1200, 800), minSize: Size(900, 600));

  static PlatformWindowConfig forPlatform(AppPlatform platform) {
    switch (platform) {
      case AppPlatform.macos:
        return macos;
      case AppPlatform.windows:
        return windows;
      case AppPlatform.linux:
        return linux;
      case AppPlatform.unknown:
        return fallback;
    }
  }
}
