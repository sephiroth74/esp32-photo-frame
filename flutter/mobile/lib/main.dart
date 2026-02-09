import 'package:flutter/material.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:provider/provider.dart';

// Old implementation kept for reference
// import 'screens/home_screen.dart';
// import 'state/image_processing_state.dart';

// New implementation
import 'screens/new_home_screen.dart';
import 'services/deep_link_handler.dart';

final GlobalKey<NavigatorState> navigatorKey = GlobalKey<NavigatorState>();
final DeepLinkHandler deepLinkHandler = DeepLinkHandler();

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  initLogging();

  // Initialize deep link handler
  await deepLinkHandler.initialize();

  runApp(const PhotoframeApp());
}

class PhotoframeApp extends StatelessWidget {
  const PhotoframeApp({super.key});

  @override
  Widget build(BuildContext context) {
    // Old implementation with ChangeNotifierProvider commented out
    return MaterialApp(
      navigatorKey: navigatorKey,
      title: 'PhotoFrame Mobile',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(useMaterial3: true, colorSchemeSeed: Colors.indigo, brightness: Brightness.light),
      home: const NewHomeScreen(),
    );
  }
}
