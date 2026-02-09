import 'package:flutter/material.dart';
import 'package:photoframe/main.dart';
import 'package:photoframe/models/qr_code_data.dart';

/// Empty Home Screen - Starting point for new implementation
class NewHomeScreen extends StatefulWidget {
  const NewHomeScreen({super.key});

  @override
  State<NewHomeScreen> createState() => _NewHomeScreenState();
}

class _NewHomeScreenState extends State<NewHomeScreen> {
  QRCodeData? _qrData;

  @override
  void initState() {
    super.initState();
    _loadCurrentQRData();
    _listenToQRCodeStream();
  }

  void _loadCurrentQRData() {
    final data = deepLinkHandler.getCurrentQRData();
    if (data != null) {
      setState(() {
        _qrData = data;
      });
    }
  }

  void _listenToQRCodeStream() {
    deepLinkHandler.qrCodeDataStream.listen((data) {
      setState(() {
        _qrData = data;
      });
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('QR Code scanned successfully!'), duration: Duration(seconds: 2)));
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('ESP32 Photo Frame'), centerTitle: true),
      body: Center(
        child: SingleChildScrollView(
          child: Padding(
            padding: const EdgeInsets.all(24.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Image.asset('assets/images/app_icon.png', width: 80, height: 80),
                const SizedBox(height: 24),
                const Text('Photo Frame App', style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
                const SizedBox(height: 16),
                const Text('Ready for implementation', style: TextStyle(fontSize: 16, color: Colors.grey)),
                const SizedBox(height: 48),
                if (_qrData != null) ...[
                  Container(
                    padding: const EdgeInsets.all(16),
                    decoration: BoxDecoration(
                      color: Colors.green.shade100,
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: Colors.green, width: 2),
                    ),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        const Row(
                          children: [
                            Icon(Icons.check_circle, color: Colors.green),
                            SizedBox(width: 8),
                            Text(
                              'QR Code Data Loaded',
                              style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.green),
                            ),
                          ],
                        ),
                        const SizedBox(height: 12),
                        Text('WebSocket URL: ${_qrData!.wsUrl}', style: const TextStyle(fontSize: 14), maxLines: 2, overflow: TextOverflow.ellipsis),
                        const SizedBox(height: 8),
                        Text('Token: ${_qrData!.token.substring(0, 10)}...', style: const TextStyle(fontSize: 14)),
                        const SizedBox(height: 8),
                        Text('Scanned: ${_qrData!.scannedAt.toString()}', style: const TextStyle(fontSize: 12, color: Colors.grey)),
                      ],
                    ),
                  ),
                ] else ...[
                  Container(
                    padding: const EdgeInsets.all(16),
                    decoration: BoxDecoration(
                      color: Colors.blue.shade100,
                      borderRadius: BorderRadius.circular(8),
                      border: Border.all(color: Colors.blue, width: 2),
                    ),
                    child: const Column(
                      children: [
                        Icon(Icons.qr_code_2, color: Colors.blue, size: 40),
                        SizedBox(height: 12),
                        Text(
                          'Waiting for QR Code',
                          style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.blue),
                        ),
                        SizedBox(height: 8),
                        Text(
                          'Scan the QR code from your ESP32 display to continue',
                          textAlign: TextAlign.center,
                          style: TextStyle(fontSize: 14, color: Colors.blue),
                        ),
                      ],
                    ),
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    );
  }
}
