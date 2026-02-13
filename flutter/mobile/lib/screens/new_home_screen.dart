import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:photoframe/main.dart';
import 'package:photoframe/services/photoframe_dithering_ffi.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';
import 'package:photoframe_common/photoframe_common.dart';
import 'package:photoframe/screens/qr_scanner_screen.dart';
import 'package:photoframe/screens/configuration_screen.dart';
import 'package:photoframe/screens/image_select_screen.dart';
import 'package:photoframe/l10n/app_localizations.dart';

/// Home Screen - Choose connection method
class NewHomeScreen extends StatefulWidget {
  const NewHomeScreen({super.key});

  @override
  State<NewHomeScreen> createState() => _NewHomeScreenState();
}

class _NewHomeScreenState extends State<NewHomeScreen> {
  static final logger = getLogger('NewHomeScreen');

  @override
  void initState() {
    super.initState();
    _checkDeepLink();
  }

  void _checkNativeLib() {
    try {
      final result = PhotoframeDithering.dummyFunction();
      logger.info('Rust dummy function result: $result'); // Dovrebbe stampare 42
    } catch (e) {
      logger.severe('Failed to call Rust dummy function: $e');
    }
  }

  /// Listen for deep links received while app is running
  void _checkDeepLink() {
    final initialQrData = deepLinkHandler.consumeInitialQrData();
    if (initialQrData != null) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (!mounted) {
          return;
        }
        Navigator.of(context).pushReplacement(
          MaterialPageRoute(
            builder: (context) => ConfigurationScreen(qrData: initialQrData),
            settings: const RouteSettings(name: '/configuration'),
          ),
        );
      });
    }

    // Listen for new deep links (only navigates when link received while app is running)
    deepLinkHandler.qrCodeDataStream.listen((data) {
      if (mounted) {
        Navigator.of(context).pushReplacement(
          MaterialPageRoute(
            builder: (context) => ConfigurationScreen(qrData: data),
            settings: const RouteSettings(name: '/configuration'),
          ),
        );
      }
    });
  }

  void _scanQRCode() {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (context) => const QRScannerScreen(),
        settings: const RouteSettings(name: '/qr_scanner'),
      ),
    );
  }

  void _enterManually() {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (context) => const ConfigurationScreen(),
        settings: const RouteSettings(name: '/configuration'),
      ),
    );
  }

  void _testCropScreen() {
    _checkNativeLib();

    // Simulated board config for testing without hardware
    const boardConfigJson = '''{
  "type": "board_info",
  "board": "ESP32-S3",
  "flash_size": 16777216,
  "display_type": "six-colors",
  "display_width": 800,
  "display_height": 480,
  "display_rotation": 1,
  "server_version": "1.0.0",
  "file_version": 1,
  "binary_file_size": 384025,
  "battery_level": 94,
  "battery_voltage_mv": 4135
}''';

    try {
      final boardConfig = BoardConfig.fromJson(jsonDecode(boardConfigJson) as Map<String, dynamic>);

      Navigator.of(context).push(MaterialPageRoute(builder: (context) => ImageSelectScreen(boardConfig: boardConfig)));
    } catch (e) {
      final l10n = AppLocalizations.of(context)!;
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l10n.errorMessageWithDetails(e.toString()))));
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    final l10n = AppLocalizations.of(context)!;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.homeAppBarTitle), centerTitle: true),
      body: Center(
        child: SingleChildScrollView(
          child: Padding(
            padding: const EdgeInsets.all(24.0),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Image.asset('assets/images/app_icon.png', width: 80, height: 80),
                const SizedBox(height: 24),
                Text(l10n.homeTitle, style: const TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
                const SizedBox(height: 6),
                Text(
                  l10n.chooseConnectionMethod,
                  style: const TextStyle(fontSize: 16, color: Colors.grey),
                  textAlign: TextAlign.center,
                ),
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
                        color: colors.surface,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: colors.borderMedium, width: 2),
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.qr_code_scanner, color: colors.primary, size: 48),
                          const SizedBox(height: 12),
                          Text(
                            l10n.scanQrTitle,
                            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: colors.primary),
                          ),
                          const SizedBox(height: 8),
                          Text(
                            l10n.scanQrSubtitle,
                            textAlign: TextAlign.center,
                            style: TextStyle(fontSize: 14, color: colors.secondary),
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
                        color: colors.surface,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: colors.borderMedium, width: 2),
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.edit, color: colors.primary, size: 48),
                          const SizedBox(height: 12),
                          Text(
                            l10n.enterManuallyTitle,
                            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: colors.primary),
                          ),
                          const SizedBox(height: 8),
                          Text(
                            l10n.enterManuallySubtitle,
                            textAlign: TextAlign.center,
                            style: TextStyle(fontSize: 14, color: colors.secondary),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
                const SizedBox(height: 20),
                // TEST: Direct to crop screen (temporary)
                SizedBox(
                  width: double.infinity,
                  child: InkWell(
                    onTap: _testCropScreen,
                    borderRadius: BorderRadius.circular(8),
                    child: Container(
                      padding: const EdgeInsets.all(20),
                      decoration: BoxDecoration(
                        color: Colors.grey.shade200,
                        borderRadius: BorderRadius.circular(8),
                        border: Border.all(color: Colors.grey.shade400, width: 2),
                      ),
                      child: Column(
                        children: [
                          Icon(Icons.bug_report, color: Colors.grey.shade600, size: 48),
                          const SizedBox(height: 12),
                          Text(
                            l10n.testCropTitle,
                            style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: Colors.grey.shade700),
                          ),
                          const SizedBox(height: 8),
                          Text(
                            l10n.testCropSubtitle,
                            textAlign: TextAlign.center,
                            style: TextStyle(fontSize: 12, color: Colors.grey.shade600, fontStyle: FontStyle.italic),
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
