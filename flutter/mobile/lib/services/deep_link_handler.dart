import 'dart:async';

import 'package:app_links/app_links.dart';
import '../utils/app_logger.dart';
import '../models/qr_code_data.dart';
import 'qr_code_data_service.dart';

/// Deep Link Handler for PhotoFrame app
/// Supports format: photoframe://connect?ip=...&ssid=...&port=...&v=...&d=...&w=...&h=...
class DeepLinkHandler {
  final AppLinks _appLinks = AppLinks();
  final _qrCodeDataStreamController = StreamController<QRCodeData>.broadcast();
  final QRCodeDataService _qrCodeDataService = QRCodeDataService();
  QRCodeData? _initialQrData;
  bool _initialLinkConsumed = false;

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
        _handleDeepLink(initialUri, isInitial: true);
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

  void _handleDeepLink(Uri uri, {bool isInitial = false}) async {
    try {
      // Expected format: photoframe://connect?ip=192.168.4.1&ssid=PhotoFrame-CE3F&port=81&v=1&d=1&w=800&h=480
      if (uri.scheme != 'photoframe') {
        logger.warning('Invalid scheme: ${uri.scheme}');
        return;
      }

      if (uri.host != 'connect') {
        logger.warning('Invalid host: ${uri.host}');
        return;
      }

      final params = uri.queryParameters;

      // Check for required parameters (ip, ssid, port)
      final ip = params['ip'];
      final ssid = params['ssid'];
      final port = params['port'];

      if (ip == null || ssid == null || port == null) {
        logger.warning('Missing required parameters: ip, ssid, or port');
        return;
      }

      // Parse QR code data and save it
      try {
        final qrData = QRCodeData.fromUri(uri);
        logger.info('Parsed QR code data from deep link: $qrData');

        // Save QR data for later use
        await _qrCodeDataService.saveQRData(qrData);

        if (isInitial) {
          _initialQrData = qrData;
          _initialLinkConsumed = false;
        }

        // Emit via stream
        _qrCodeDataStreamController.add(qrData);
      } catch (e) {
        logger.severe('Failed to parse QR code data from deep link: $e');
      }
    } catch (e) {
      logger.severe('Failed to parse deep link: $e');
    }
  }

  /// Get current QR code data from storage
  QRCodeData? getCurrentQRData() {
    return _qrCodeDataService.getCurrentQRData();
  }

  /// Get QR data only if the app was opened via a deep link in this launch
  QRCodeData? consumeInitialQrData() {
    if (_initialLinkConsumed) {
      return null;
    }
    _initialLinkConsumed = true;
    return _initialQrData;
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
    _qrCodeDataStreamController.close();
  }
}
