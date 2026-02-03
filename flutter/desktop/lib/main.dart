import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:window_manager/window_manager.dart';
import 'core/providers/ble_provider.dart';
import 'core/providers/processing_provider.dart';
import 'core/providers/widget_factory_provider.dart';
import 'screens/home_screen.dart';
import 'platform/platform_detector.dart';
import 'platform/window_config.dart';
import 'core/services/window_preferences.dart';
import 'core/services/file_picker_history.dart';
import 'widgets/menu_bar.dart';

Future<void> _configureMacosWindowUtils() async {
  const config = AppKitWindowUtilsConfig(
    makeTitlebarTransparent: true,
    toolbarStyle: NSWindowToolbarStyle.automatic,
    autoHideToolbarAndMenuBarInFullScreenMode: false,
    enableFullSizeContentView: true,
    hideTitle: true,
    removeMenubarInFullScreenMode: true,
  );
  config.apply();
}

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Initialize window manager
  // Get platform-specific configuration
  final platform = PlatformDetector.current;
  final windowConfig = PlatformWindowConfig.forPlatform(platform);
  debugPrint('🖥️ Running on: $platform');

  // Initialize window manager
  await windowManager.ensureInitialized();

  if (platform == AppPlatform.macos) {
    await _configureMacosWindowUtils();
    await AppKitUiElements.ensureInitialized(debug: true, useWindowManager: true);
  } else {
    await windowManager.ensureInitialized();
  }

  // Restore window size and position
  await WindowPreferences.restoreWindowSize();

  // Initialize file picker history (persistent storage for last used directories)
  await FilePickerHistory.initialize();

  // Set minimum window size
  await windowManager.setMinimumSize(const Size(800, 600));

  // Show window
  await windowManager.show();

  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> with WindowListener {
  @override
  void initState() {
    super.initState();
    windowManager.addListener(this);
  }

  @override
  void dispose() {
    windowManager.removeListener(this);
    super.dispose();
  }

  @override
  void onWindowResized() {
    WindowPreferences.saveWindowSize();
  }

  @override
  void onWindowMoved() {
    WindowPreferences.saveWindowSize();
  }

  @override
  void onWindowClose() async {
    await WindowPreferences.saveWindowSize();
  }

  @override
  Widget build(BuildContext context) {
    final platform = PlatformDetector.current;
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => ProcessingProvider()),
        ChangeNotifierProvider(create: (_) => BleUploadState()),
        ChangeNotifierProvider(create: (_) => WidgetFactoryProvider()),
      ],
      child: platform == AppPlatform.macos
          ? AppKitMacosApp(
              debugShowCheckedModeBanner: false,
              theme: AppKitThemeData.light(),
              darkTheme: AppKitThemeData.dark(),
              themeMode: ThemeMode.system,
              builder: (context, child) => AppMenuBar(child: child ?? const SizedBox.shrink()),
              home: const HomeScreen(),
            )
          : MaterialApp(
              debugShowCheckedModeBanner: false,
              theme: ThemeData.light(useMaterial3: true),
              darkTheme: ThemeData.dark(useMaterial3: true),
              themeMode: ThemeMode.system,
              home: const HomeScreen(),
            ),
    );
  }
}
