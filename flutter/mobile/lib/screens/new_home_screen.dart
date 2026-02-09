import 'package:flutter/material.dart';
import 'package:photoframe/main.dart';
import 'package:photoframe/screens/qr_scanner_screen.dart';
import 'package:photoframe/screens/configuration_screen.dart';

/// Home Screen - Choose connection method
class NewHomeScreen extends StatefulWidget {
  const NewHomeScreen({super.key});

  @override
  State<NewHomeScreen> createState() => _NewHomeScreenState();
}

class _NewHomeScreenState extends State<NewHomeScreen> {
  @override
  void initState() {
    super.initState();
    _checkDeepLink();
  }

  /// Listen for deep links received while app is running
  void _checkDeepLink() {
    final initialQrData = deepLinkHandler.consumeInitialQrData();
    if (initialQrData != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) {
          return;
        }
        Navigator.of(context).pushReplacement(MaterialPageRoute(builder: (context) => ConfigurationScreen(qrData: initialQrData)));
      });
    }

    // Listen for new deep links (only navigates when link received while app is running)
    deepLinkHandler.qrCodeDataStream.listen((data) {
      if (mounted) {
        Navigator.of(context).pushReplacement(MaterialPageRoute(builder: (context) => ConfigurationScreen(qrData: data)));
      }
    });
  }

  void _scanQRCode() {
    Navigator.of(context).push(MaterialPageRoute(builder: (context) => const QRScannerScreen()));
  }

  void _enterManually() {
    Navigator.of(context).push(MaterialPageRoute(builder: (context) => const ConfigurationScreen()));
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
                const Text('Choose connection method', style: TextStyle(fontSize: 16, color: Colors.grey)),
                const SizedBox(height: 48),
                // Scan QR Code option
                SizedBox(
                  width: double.infinity,
                  child: InkWell(
                    onTap: _scanQRCode,
                    borderRadius: BorderRadius.circular(8),
                    child: Container(
                      padding: const EdgeInsets.all(20),
                      decoration: BoxDecoration(
                        color: Colors.indigo.shade50,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: Colors.indigo, width: 2),
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.qr_code_scanner, color: Colors.indigo, size: 48),
                          const SizedBox(height: 12),
                          Text(
                            'Scan QR Code',
                            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.indigo.shade900),
                          ),
                          const SizedBox(height: 8),
                          Text(
                            'Scan the QR code from your ESP32 display',
                            textAlign: TextAlign.center,
                            style: TextStyle(fontSize: 14, color: Colors.indigo.shade700),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 20),
                // Manual entry option
                SizedBox(
                  width: double.infinity,
                  child: InkWell(
                    onTap: _enterManually,
                    borderRadius: BorderRadius.circular(8),
                    child: Container(
                      padding: const EdgeInsets.all(20),
                      decoration: BoxDecoration(
                        color: Colors.blue.shade50,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: Colors.blue, width: 2),
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.edit, color: Colors.blue, size: 48),
                          const SizedBox(height: 12),
                          Text(
                            'Enter Manually',
                            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.blue.shade900),
                          ),
                          const SizedBox(height: 8),
                          Text(
                            'Enter IP address and port manually',
                            textAlign: TextAlign.center,
                            style: TextStyle(fontSize: 14, color: Colors.blue.shade700),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
