import 'dart:io';

import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:window_manager/window_manager.dart';
import 'providers/ble_provider.dart';
import 'providers/processing_provider.dart';
import 'screens/home_screen.dart';
import 'services/window_preferences.dart';
import 'services/file_picker_history.dart';
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
  assert(Platform.isMacOS && !kIsWeb, 'This example is intended to run on macOS desktop only.');

  // Initialize window manager
  await windowManager.ensureInitialized();

  await _configureMacosWindowUtils();
  await AppKitUiElements.ensureInitialized(debug: true, useWindowManager: true);

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
    return MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => ProcessingProvider()),
        ChangeNotifierProvider(create: (_) => BleUploadState()),
      ],
      child: AppKitMacosApp(
        debugShowCheckedModeBanner: false,
        theme: AppKitThemeData.light(),
        darkTheme: AppKitThemeData.dark(),
        themeMode: ThemeMode.system,
        builder: (context, child) => AppMenuBar(child: child ?? const SizedBox.shrink()),
        home: const HomeScreen(),
      ),
    );
  }
}
