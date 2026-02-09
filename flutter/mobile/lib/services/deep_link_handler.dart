import 'dart:async';

import 'package:app_links/app_links.dart';
import '../utils/app_logger.dart';
import '../models/qr_code_data.dart';
import 'qr_code_data_service.dart';

// Old BLE connection info - kept for backward compatibility
class DeviceConnectionInfo {
  final String ip;
  final String ssid;
  final int port;
  final int? version;
  final int? displayType;
  final int? width;
  final int? height;

  const DeviceConnectionInfo({required this.ip, required this.ssid, this.port = 8080, this.version, this.displayType, this.width, this.height});

  @override
  String toString() {
    return 'DeviceConnectionInfo(ip: $ip, ssid: $ssid, port: $port)';
  }
}

class DeepLinkHandler {
  final AppLinks _appLinks = AppLinks();
  final _connectionStreamController = StreamController<DeviceConnectionInfo>.broadcast();
  final _qrCodeDataStreamController = StreamController<QRCodeData>.broadcast();
  final QRCodeDataService _qrCodeDataService = QRCodeDataService();

  Stream<DeviceConnectionInfo> get connectionStream => _connectionStreamController.stream;
  Stream<QRCodeData> get qrCodeDataStream => _qrCodeDataStreamController.stream;

  /// Initialize deep link handling
  /// Should be called once in the app's main method or initState
  Future<void> initialize() async {
    // Initialize QR code data service first
    try {
      await _qrCodeDataService.initialize();
    } catch (e) {
      logger.severe('Failed to initialize QRCodeDataService: $e');
    }

    // Handle initial link if app was opened from a link
    try {
      final initialUri = await _appLinks.getInitialAppLink();
      if (initialUri != null) {
        logger.info('App opened with initial link: $initialUri');
        _handleDeepLink(initialUri);
      }
    } catch (e) {
      logger.severe('Failed to get initial link: $e');
    }

    // Listen to incoming links while app is running
    _appLinks.uriLinkStream.listen(
      (uri) {
        logger.info('Received deep link: $uri');
        _handleDeepLink(uri);
      },
      onError: (e) {
        logger.severe('Deep link error: $e');
      },
    );

    logger.info('Deep link handler initialized');
  }

  void _handleDeepLink(Uri uri) {
    try {
      // Expected format: photoframe://connect?wsUrl=ws://192.168.4.1:81&token=abc123xyz
      // or old format: photoframe://connect?ip=192.168.1.100&ssid=MyWiFi&port=8080&v=1&d=1&w=800&h=480
      if (uri.scheme != 'photoframe') {
        logger.warning('Invalid scheme: ${uri.scheme}');
        return;
      }

      if (uri.host != 'connect') {
        logger.warning('Invalid host: ${uri.host}');
        return;
      }

      final params = uri.queryParameters;

      // Check if it's the new WebSocket format
      if (params.containsKey('wsUrl') && params.containsKey('token')) {
        _handleWebSocketDeepLink(uri);
        return;
      }

      // Otherwise, handle old BLE format
      final ip = params['ip'];
      final ssid = params['ssid'];

      if (ip == null || ssid == null) {
        logger.warning('Missing required parameters: ip or ssid (old format) or wsUrl/token (new format)');
        return;
      }

      final port = int.tryParse(params['port'] ?? '8080') ?? 8080;
      final version = int.tryParse(params['v'] ?? '');
      final displayType = int.tryParse(params['d'] ?? '');
      final width = int.tryParse(params['w'] ?? '');
      final height = int.tryParse(params['h'] ?? '');

      final connectionInfo = DeviceConnectionInfo(
        ip: ip,
        ssid: ssid,
        port: port,
        version: version,
        displayType: displayType,
        width: width,
        height: height,
      );

      logger.info('Parsed connection info (BLE format): $connectionInfo');
      _connectionStreamController.add(connectionInfo);
    } catch (e) {
      logger.severe('Failed to parse deep link: $e');
    }
  }

  /// Handle new WebSocket deep link format
  Future<void> _handleWebSocketDeepLink(Uri uri) async {
    try {
      final qrData = QRCodeData.fromUri(uri);
      logger.info('Parsed QR code data (WebSocket format): $qrData');

      // Save QR data for later use
      await _qrCodeDataService.saveQRData(qrData);

      // Emit via stream
      _qrCodeDataStreamController.add(qrData);
    } catch (e) {
      logger.severe('Failed to parse WebSocket deep link: $e');
    }
  }

  /// Get current QR code data from storage
  QRCodeData? getCurrentQRData() {
    return _qrCodeDataService.getCurrentQRData();
  }

  /// Manually parse a deep link URL (useful for testing)
  void parseUrl(String url) {
    try {
      final uri = Uri.parse(url);
      _handleDeepLink(uri);
    } catch (e) {
      logger.severe('Failed to parse URL: $e');
    }
  }

  void dispose() {
    _connectionStreamController.close();
    _qrCodeDataStreamController.close();
  }
}
