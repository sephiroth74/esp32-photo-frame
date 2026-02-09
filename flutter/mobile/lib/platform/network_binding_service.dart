import 'dart:io' show Platform;
import 'package:flutter/services.dart';
import '../utils/app_logger.dart';

/// Service to force Android to use WiFi network for WebSocket connections
/// even when the WiFi has no internet access (Android 12+ issue)
class NetworkBindingService {
  static const MethodChannel _channel = MethodChannel('photoframe/wifi');

  /// Bind all network traffic to the current WiFi network
  /// This forces Android to use WiFi even if it has no internet
  /// Required on Android 12+ to connect to ESP32 local WiFi
  static Future<bool> bindToWifi() async {
    if (!Platform.isAndroid) {
      logger.fine('Network binding not needed on non-Android platforms');
      return true;
    }

    try {
      logger.info('Binding process to WiFi network...');
      final result = await _channel.invokeMethod<bool>('bindToWifi');
      if (result == true) {
        logger.info('✓ Successfully bound to WiFi network');
        return true;
      } else {
        logger.warning('Network binding returned false');
        return false;
      }
    } on PlatformException catch (e) {
      logger.severe('Failed to bind to WiFi: ${e.message}');
      return false;
    } catch (e) {
      logger.severe('Unexpected error binding to WiFi: $e');
      return false;
    }
  }

  /// Clear network binding (restore default routing)
  static Future<void> clearWifiBinding() async {
    if (!Platform.isAndroid) {
      return;
    }

    try {
      logger.info('Clearing WiFi binding...');
      await _channel.invokeMethod('clearWifiBinding');
      logger.info('✓ WiFi binding cleared');
    } on PlatformException catch (e) {
      logger.warning('Failed to clear WiFi binding: ${e.message}');
    } catch (e) {
      logger.warning('Unexpected error clearing WiFi binding: $e');
    }
  }
}
