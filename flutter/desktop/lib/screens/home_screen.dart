import 'package:flutter/material.dart';

import '../platform/platform_detector.dart';
import 'home_screen_macos.dart';
import 'home_screen_material.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return PlatformDetector.current == AppPlatform.macos ? const HomeScreenMacos() : const HomeScreenMaterial();
  }
}
