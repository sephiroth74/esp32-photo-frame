import 'package:provider/provider.dart';
import 'package:appkit_ui_elements/appkit_ui_elements.dart';

import '../providers/processing_provider.dart';
import 'ble_upload_screen.dart';
import 'processing_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  late final SegmentedControllerSingle _tabController;

  @override
  void initState() {
    super.initState();
    _tabController = SegmentedControllerSingle(initialIndex: 0, length: 2);
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final provider = context.watch<ProcessingProvider>();
    final profileTitle = provider.currentProfileName != null
        ? 'ESP32 Photo Frame Processor - ${provider.currentProfileName}${provider.hasUnsavedChanges ? '*' : ''}'
        : 'ESP32 Photo Frame Processor';

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
                    labels: const ['Process Images', 'Bluetooth Upload'],
                    onSelectionChanged: (_, _) => setState(() {}),
                  ),
                  const SizedBox(height: 16),
                  Expanded(
                    child: IndexedStack(index: _tabController.index, children: const [ProcessingScreen(), BleUploadScreen()]),
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
