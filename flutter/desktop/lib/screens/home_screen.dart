import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../core/providers/widget_factory_provider.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    // Use the factory to create the platform-specific home screen.
    // This removes direct dependency on specific files (e.g., home_screen_macos.dart)
    // and ensures the use of prepared abstractions.
    return context.watch<WidgetFactoryProvider>().factory.createHomeScreen();
  }
}
