import 'package:flutter/material.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:provider/provider.dart';

import 'screens/home_screen.dart';
import 'state/image_processing_state.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  initLogging();
  runApp(const PhotoframeApp());
}

class PhotoframeApp extends StatelessWidget {
  const PhotoframeApp({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => ImageProcessingState(),
      child: MaterialApp(
        title: 'PhotoFrame Mobile',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(useMaterial3: true, colorSchemeSeed: Colors.indigo, brightness: Brightness.light),
        home: const HomeScreen(),
      ),
    );
  }
}
