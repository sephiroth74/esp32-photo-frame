import 'package:appkit_ui_elements/appkit_ui_elements.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:provider/provider.dart';

import '../core/providers/processing_provider.dart';
import 'ws_upload_screen.dart';
import 'processing_screen.dart';

class HomeScreenMacos extends StatefulWidget {
  const HomeScreenMacos({super.key});

  @override
  State<HomeScreenMacos> createState() => _HomeScreenMacosState();
}

class _HomeScreenMacosState extends State<HomeScreenMacos> {
  late final SegmentedControllerSingle _tabController;
  String _appVersion = '';

  @override
  void initState() {
    super.initState();
    _tabController = SegmentedControllerSingle(initialIndex: 0, length: 2);
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
    _tabController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final versionSuffix = _appVersion.isNotEmpty ? ' v$_appVersion' : '';
    final profileTitle = provider.currentProfileName != null
        ? 'ESP32 Photo Frame Processor$versionSuffix - ${provider.currentProfileName}${provider.hasUnsavedChanges ? '*' : ''}'
        : 'ESP32 Photo Frame Processor$versionSuffix';

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
                    controller: _tabController,
                    labels: const ['Process Images', 'Upload to Device'],
                    onSelectionChanged: (_, _) => setState(() {}),
                  ),
                  const SizedBox(height: 16),
                  Expanded(
                    child: IndexedStack(index: _tabController.index, children: const [ProcessingScreen(), WsUploadScreen()]),
                  ),
                ],
              ),
            );
          },
        ),
      ],
    );
  }
}
