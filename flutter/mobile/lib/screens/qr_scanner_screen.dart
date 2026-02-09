import 'package:flutter/material.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:photoframe/models/qr_code_data.dart';
import 'package:photoframe/screens/configuration_screen.dart';
import 'package:photoframe/utils/app_logger.dart';

/// Fullscreen QR Code scanner
class QRScannerScreen extends StatefulWidget {
  const QRScannerScreen({super.key});

  @override
  State<QRScannerScreen> createState() => _QRScannerScreenState();
}

class _QRScannerScreenState extends State<QRScannerScreen> {
  MobileScannerController cameraController = MobileScannerController();
  bool _isProcessing = false;

  @override
  void dispose() {
    cameraController.dispose();
    super.dispose();
  }

  void _onDetect(BarcodeCapture capture) async {
    if (_isProcessing) return;

    final List<Barcode> barcodes = capture.barcodes;
    if (barcodes.isEmpty) return;

    final barcode = barcodes.first;
    final String? code = barcode.rawValue;

    if (code == null || code.isEmpty) return;

    setState(() {
      _isProcessing = true;
    });

    try {
      logger.info('QR Code detected: $code');

      // Parse the URL from QR code
      // Expected format: photoframe://connect?wsUrl=ws://192.168.4.1:81/upload&token=abc123
      final uri = Uri.parse(code);

      if (uri.scheme != 'photoframe' || uri.host != 'connect') {
        _showError('Invalid QR code format');
        setState(() {
          _isProcessing = false;
        });
        return;
      }

      // Create QR data from URI
      final qrData = QRCodeData.fromUri(uri);

      logger.info('QR data parsed successfully: $qrData');

      // Navigate to configuration screen
      if (mounted) {
        Navigator.of(context).pushReplacement(MaterialPageRoute(builder: (context) => ConfigurationScreen(qrData: qrData)));
      }
    } catch (e) {
      logger.severe('Failed to process QR code: $e');
      _showError('Failed to process QR code: $e');
      setState(() {
        _isProcessing = false;
      });
    }
  }

  void _showError(String message) {
    ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message), backgroundColor: Colors.red, duration: const Duration(seconds: 3)));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(title: const Text('Scan QR Code'), backgroundColor: Colors.black, foregroundColor: Colors.white),
      body: Stack(
        children: [
          MobileScanner(controller: cameraController, onDetect: _onDetect),
          // Overlay with instructions
          Positioned(
            bottom: 0,
            left: 0,
            right: 0,
            child: Container(
              color: Colors.black.withValues(alpha: 0.7),
              padding: const EdgeInsets.all(24),
              child: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  const Icon(Icons.qr_code_scanner, color: Colors.white, size: 48),
                  const SizedBox(height: 16),
                  const Text(
                    'Point your camera at the QR code',
                    style: TextStyle(color: Colors.white, fontSize: 18, fontWeight: FontWeight.bold),
                    textAlign: TextAlign.center,
                  ),
                  const SizedBox(height: 8),
                  const Text(
                    'The QR code will be scanned automatically',
                    style: TextStyle(color: Colors.white70, fontSize: 14),
                    textAlign: TextAlign.center,
                  ),
                  if (_isProcessing) ...[
                    const SizedBox(height: 16),
                    const CircularProgressIndicator(valueColor: AlwaysStoppedAnimation<Color>(Colors.white)),
                  ],
                ],
              ),
            ),
          ),
          // Viewfinder overlay
          Center(
            child: Container(
              width: 250,
              height: 250,
              decoration: BoxDecoration(
                border: Border.all(color: Colors.white, width: 2),
                borderRadius: BorderRadius.circular(12),
              ),
            ),
          ),
        ],
      ),
    );
  }
}
