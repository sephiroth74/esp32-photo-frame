import 'package:flutter/foundation.dart';
import 'package:photoframe_flutter/platform/platform_detector.dart';
import 'package:photoframe_flutter/presentation/widget_factory.dart';

/// Provider per il WidgetFactory della piattaforma corrente.
class WidgetFactoryProvider with ChangeNotifier {
  late final WidgetFactory _factory;

  WidgetFactoryProvider() {
    _factory = WidgetFactoryResolver.current();
  }

  WidgetFactory get factory => _factory;
  AppPlatform get currentPlatform => PlatformDetector.current;
}
