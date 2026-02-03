import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:provider/provider.dart';

import '../core/providers/processing_provider.dart';
import '../core/providers/widget_factory_provider.dart';
import '../platform/platform_detector.dart';
import 'ble_upload_screen.dart';
import 'processing_screen.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  SegmentedControllerSingle? _tabController;
  int _tabIndex = 0;
  String _appVersion = '';

  @override
  void initState() {
    super.initState();
    if (PlatformDetector.current == AppPlatform.macos) {
      _tabController = SegmentedControllerSingle(initialIndex: 0, length: 2);
    }
    _loadAppVersion();
  }

  Future<void> _loadAppVersion() async {
    final packageInfo = await PackageInfo.fromPlatform();
    setState(() {
      _appVersion = packageInfo.version;
    });
  }

  @override
  void dispose() {
    _tabController?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final versionSuffix = _appVersion.isNotEmpty ? ' v$_appVersion' : '';
    final profileTitle = provider.currentProfileName != null
        ? 'ESP32 Photo Frame Processor$versionSuffix - ${provider.currentProfileName}${provider.hasUnsavedChanges ? '*' : ''}'
        : 'ESP32 Photo Frame Processor$versionSuffix';

    if (PlatformDetector.current == AppPlatform.macos) {
      return AppKitScaffold(
        toolBar: AppKitToolBar(title: Text(profileTitle), titleWidth: 400),
        children: [
          AppKitContentArea(
            builder: (context, scrollController) {
              return Padding(
                padding: const EdgeInsets.all(20),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    AppKitSegmentedControl(
                      controller: _tabController!,
                      labels: const ['Process Images', 'Bluetooth Upload'],
                      onSelectionChanged: (_, _) => setState(() {}),
                    ),
                    const SizedBox(height: 16),
                    Expanded(
                      child: IndexedStack(index: _tabController!.index, children: const [ProcessingScreen(), BleUploadScreen()]),
                    ),
                  ],
                ),
              );
            },
          ),
        ],
      );
    }

    return Scaffold(
      appBar: AppBar(title: Text(profileTitle)),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            ToggleButtons(
              isSelected: [_tabIndex == 0, _tabIndex == 1],
              onPressed: (index) => setState(() => _tabIndex = index),
              children: const [
                Padding(padding: EdgeInsets.symmetric(horizontal: 12), child: Text('Process Images')),
                Padding(padding: EdgeInsets.symmetric(horizontal: 12), child: Text('Bluetooth Upload')),
              ],
            ),
            const SizedBox(height: 16),
            Expanded(
              child: IndexedStack(index: _tabIndex, children: const [ProcessingScreen(), BleUploadScreen()]),
            ),
          ],
        ),
      ),
    );
  }
}
