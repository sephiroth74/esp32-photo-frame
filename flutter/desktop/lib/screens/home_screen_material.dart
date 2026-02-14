import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:provider/provider.dart';

import '../core/providers/processing_provider.dart';
import 'ws_upload_screen.dart';
import 'processing_screen.dart';

class HomeScreenMaterial extends StatefulWidget {
  const HomeScreenMaterial({super.key});

  @override
  State<HomeScreenMaterial> createState() => _HomeScreenMaterialState();
}

class _HomeScreenMaterialState extends State<HomeScreenMaterial> {
  int _tabIndex = 0;
  String _appVersion = '';

  @override
  void initState() {
    super.initState();
    _loadAppVersion();
  }

  Future<void> _loadAppVersion() async {
    final packageInfo = await PackageInfo.fromPlatform();
    setState(() {
      _appVersion = packageInfo.version;
    });
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final versionSuffix = _appVersion.isNotEmpty ? ' v$_appVersion' : '';
    final profileTitle = provider.currentProfileName != null
        ? 'ESP32 Photo Frame Processor$versionSuffix - ${provider.currentProfileName}${provider.hasUnsavedChanges ? '*' : ''}'
        : 'ESP32 Photo Frame Processor$versionSuffix';

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
                Padding(padding: EdgeInsets.symmetric(horizontal: 12), child: Text('Upload to Device')),
              ],
            ),
            const SizedBox(height: 16),
            Expanded(
              child: IndexedStack(index: _tabIndex, children: const [ProcessingScreen(), WsUploadScreen()]),
            ),
          ],
        ),
      ),
    );
  }
}
