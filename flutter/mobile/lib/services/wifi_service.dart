import 'dart:io';

import 'package:flutter/services.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import '../utils/app_logger.dart';

class WiFiService {
  static const MethodChannel _platform = MethodChannel('photoframe/wifi');
  final NetworkInfo _networkInfo = NetworkInfo();

  /// Static convenience method to get current SSID
  static Future<String?> getCurrentSSID() async {
    final service = WiFiService();
    return await service.getCurrentSsid();
  }

  /// Get the current WiFi SSID
  /// Returns null if not connected to WiFi or permission denied
  Future<String?> getCurrentSsid() async {
    try {
      // Request location permission (required on Android to get SSID)
      if (Platform.isAndroid) {
        final status = await Permission.locationWhenInUse.status;
        if (!status.isGranted) {
          logger.warning('Location permission not granted, cannot get WiFi SSID');
          return null;
        }
      }

      final ssid = await _networkInfo.getWifiName();

      if (ssid == null) {
        logger.info('Not connected to WiFi');
        return null;
      }

      // Remove quotes if present (iOS returns SSID with quotes)
      final cleanSsid = ssid.replaceAll('"', '');
      // logger.info('Current WiFi SSID: $cleanSsid');
      return cleanSsid;
    } catch (e) {
      logger.severe('Failed to get WiFi SSID: $e');
      return null;
    }
  }

  /// Check if the current WiFi matches the expected SSID
  Future<bool> isConnectedToWifi(String expectedSsid) async {
    final currentSsid = await getCurrentSsid();
    if (currentSsid == null) {
      return false;
    }

    final matches = currentSsid.toLowerCase() == expectedSsid.toLowerCase();
    logger.info('WiFi match: current=$currentSsid, expected=$expectedSsid, matches=$matches');
    return matches;
  }

  /// Request location permission (needed on Android to get WiFi info)
  Future<bool> requestLocationPermission() async {
    if (Platform.isAndroid) {
      final status = await Permission.locationWhenInUse.request();
      return status.isGranted;
    }
    // iOS doesn't need explicit permission for WiFi info
    return true;
  }

  /// Get the current WiFi IP address
  Future<String?> getWifiIpAddress() async {
    try {
      final ip = await _networkInfo.getWifiIP();
      logger.info('WiFi IP address: $ip');
      return ip;
    } catch (e) {
      logger.severe('Failed to get WiFi IP: $e');
      return null;
    }
  }

  /// Check if device is connected to WiFi
  Future<bool> isConnectedToWifiNetwork() async {
    final ssid = await getCurrentSsid();
    return ssid != null;
  }

  /// Bind process to WiFi network on Android to avoid cellular routing.
  Future<void> bindToWifiNetwork() async {
    if (!Platform.isAndroid) return;
    try {
      await _platform.invokeMethod('bindToWifi');
    } catch (e) {
      logger.warning('Failed to bind to WiFi network: $e');
    }
  }

  /// Clear WiFi binding on Android.
  Future<void> clearWifiBinding() async {
    if (!Platform.isAndroid) return;
    try {
      await _platform.invokeMethod('clearWifiBinding');
    } catch (e) {
      logger.warning('Failed to clear WiFi binding: $e');
    }
  }
}
