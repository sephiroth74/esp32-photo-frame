import 'package:flutter/material.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:dynamic_color/dynamic_color.dart';

// Old implementation kept for reference
// import 'screens/home_screen.dart';
// import 'state/image_processing_state.dart';

// New implementation
import 'screens/new_home_screen.dart';
import 'services/deep_link_handler.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'l10n/app_localizations.dart';


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
    return DynamicColorBuilder(
      builder: (ColorScheme? lightDynamic, ColorScheme? darkDynamic) {
        // Use dynamic colors if available (Android 12+), fallback to indigo seed
        ColorScheme lightColorScheme;
        ColorScheme darkColorScheme;

        if (lightDynamic != null && darkDynamic != null) {
          // Use system palette colors (Android 12+ Material You)
          lightColorScheme = lightDynamic.harmonized();
          darkColorScheme = darkDynamic.harmonized();
        } else {
          // Fallback to default color scheme with indigo seed
          lightColorScheme = ColorScheme.fromSeed(seedColor: Colors.indigo, brightness: Brightness.light);
          darkColorScheme = ColorScheme.fromSeed(seedColor: Colors.indigo, brightness: Brightness.dark);
        }

        return MaterialApp(
          navigatorKey: navigatorKey,
          title: 'PhotoFrame Mobile',
          localizationsDelegates: const [
            AppLocalizations.delegate, // Add this line
            GlobalMaterialLocalizations.delegate,
            GlobalWidgetsLocalizations.delegate,
            GlobalCupertinoLocalizations.delegate,
          ],
          supportedLocales: [
            Locale('en'), // English
            Locale('it'), // Italian
          ],
          debugShowCheckedModeBanner: false,
          theme: ThemeData(useMaterial3: true, colorScheme: lightColorScheme, brightness: Brightness.light),
          darkTheme: ThemeData(useMaterial3: true, colorScheme: darkColorScheme, brightness: Brightness.dark),
          home: const NewHomeScreen(),
        );
      },
    );
  }
}
