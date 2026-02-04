import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../core/providers/widget_factory_provider.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    // Utilizziamo la factory per creare la schermata home specifica per la piattaforma.
    // Questo rimuove la dipendenza diretta dai file specifici (es. home_screen_macos.dart)
    // e garantisce l'uso delle astrazioni preparate.
    return context.watch<WidgetFactoryProvider>().factory.createHomeScreen();
  }
}
