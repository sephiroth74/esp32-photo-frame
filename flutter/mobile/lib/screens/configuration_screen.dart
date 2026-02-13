import 'dart:async';

import 'package:flutter/material.dart';
import 'package:photoframe/models/qr_code_data.dart';
import 'package:photoframe/screens/image_select_screen.dart';
import 'package:photoframe/services/wifi_service.dart';
import 'package:photoframe/services/ws_connection_service.dart';
import 'package:photoframe/utils/app_logger.dart';
import 'package:photoframe/utils/theme_colors.dart';
import 'package:photoframe/l10n/app_localizations.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:app_settings/app_settings.dart';

/// Configuration screen for WiFi and WebSocket connection
class ConfigurationScreen extends StatefulWidget {
  final QRCodeData? qrData;

  const ConfigurationScreen({super.key, this.qrData});

  @override
  State<ConfigurationScreen> createState() => _ConfigurationScreenState();
}

class _ConfigurationScreenState extends State<ConfigurationScreen> with WidgetsBindingObserver {
  final _formKey = GlobalKey<FormState>();
  final _ipController = TextEditingController(text: '192.168.4.1');
  final _portController = TextEditingController(text: '81');

  String? _currentSSID;
  bool _isLoadingWiFi = true;
  bool _wifiValid = false;
  String? _expectedSSID;
  Timer? _wifiPoller;
  bool _isConnecting = false;

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);
    _loadSavedConfig();
    _checkWiFiConnection();
    if (!WsConnectionService().isConnected) {
      _startWiFiPolling();
    }

    // If QR data provided, extract info
    if (widget.qrData != null) {
      _parseQRData();
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _wifiPoller?.cancel();
    _ipController.dispose();
    _portController.dispose();
    super.dispose();
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (state == AppLifecycleState.resumed) {
      _checkWiFiConnection(showLoading: false, showPermissionSnackBar: false);
      if (WsConnectionService().isConnected) {
        _stopWiFiPolling();
      } else {
        _startWiFiPolling();
      }
    }
  }

  /// Parse QR data to extract IP, port and SSID
  void _parseQRData() {
    try {
      // Use IP and port directly from QR data
      _ipController.text = widget.qrData!.ip;
      _portController.text = widget.qrData!.port.toString();

      // Get SSID from QRCodeData
      _expectedSSID = widget.qrData!.ssid;

      _checkWiFiConnection(showLoading: false, showPermissionSnackBar: false);

      logger.info('Parsed QR data - IP: ${widget.qrData!.ip}, Port: ${widget.qrData!.port}, SSID: $_expectedSSID');
    } catch (e) {
      logger.severe('Failed to parse QR data: $e');
    }
  }

  /// Load saved configuration from preferences
  Future<void> _loadSavedConfig() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final savedIp = prefs.getString('connection_ip');
      final savedPort = prefs.getString('connection_port');

      if (savedIp != null && savedIp.isNotEmpty) {
        _ipController.text = savedIp;
      }

      if (savedPort != null && savedPort.isNotEmpty) {
        _portController.text = savedPort;
      }

      logger.info('Loaded saved config - IP: $savedIp, Port: $savedPort');
    } catch (e) {
      logger.warning('Failed to load saved config: $e');
    }
  }

  /// Check current WiFi connection
  Future<void> _checkWiFiConnection({bool showLoading = true, bool showPermissionSnackBar = true}) async {
    if (showLoading) {
      setState(() {
        _isLoadingWiFi = true;
      });
    }

    try {
      // Request location permission first (required on Android to get WiFi SSID)
      final wifiService = WiFiService();
      final permissionGranted = await wifiService.requestLocationPermission();

      if (!permissionGranted) {
        logger.warning('Location permission denied');
        if (mounted) {
          final l10n = AppLocalizations.of(context)!;
          setState(() {
            _currentSSID = l10n.permissionRequiredLabel;
            _isLoadingWiFi = false;
            _wifiValid = false;
          });
        }

        if (showPermissionSnackBar && mounted) {
          final l10n = AppLocalizations.of(context)!;
          final colors = ThemeColors(context);
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text(l10n.locationPermissionRequiredMessage), backgroundColor: colors.warning, duration: const Duration(seconds: 3)),
          );
        }
        return;
      }

      final ssid = await WiFiService.getCurrentSSID();
      if (mounted) {
        setState(() {
          _currentSSID = ssid;
          _isLoadingWiFi = false;
        });
      }

      // Validate WiFi
      if (ssid != null) {
        if (_expectedSSID != null) {
          // If we have exact SSID from QR code, match exactly
          _wifiValid = ssid == _expectedSSID;
        } else {
          // Otherwise, check if it starts with "PhotoFrame-"
          _wifiValid = ssid.startsWith('PhotoFrame-');
        }
      } else {
        _wifiValid = false;
      }
    } catch (e) {
      logger.severe('Failed to get WiFi info: $e');
      if (mounted) {
        final l10n = AppLocalizations.of(context)!;
        setState(() {
          _currentSSID = l10n.wifiInfoErrorLabel;
          _isLoadingWiFi = false;
          _wifiValid = false;
        });
      }
    }
  }

  void _startWiFiPolling() {
    if (WsConnectionService().isConnected) {
      return;
    }
    _wifiPoller?.cancel();
    _wifiPoller = Timer.periodic(const Duration(seconds: 3), (_) {
      if (!mounted || _isLoadingWiFi) {
        return;
      }
      _checkWiFiConnection(showLoading: false, showPermissionSnackBar: false);
    });
  }

  void _stopWiFiPolling() {
    _wifiPoller?.cancel();
    _wifiPoller = null;
  }

  /// Open system WiFi settings
  void _openWiFiSettings() {
    AppSettings.openAppSettings(type: AppSettingsType.wifi);
  }

  /// Save configuration and connect to WebSocket
  Future<void> _saveAndContinue() async {
    if (!_formKey.currentState!.validate()) {
      return;
    }

    final colors = ThemeColors(context);
    final l10n = AppLocalizations.of(context)!;
    if (!_wifiValid) {
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(l10n.wifiConnectCorrectNetworkMessage), backgroundColor: colors.warning));
      return;
    }

    setState(() {
      _isConnecting = true;
    });

    try {
      final ip = _ipController.text;
      final portStr = _portController.text;
      final port = int.parse(portStr);

      logger.info('Attempting WebSocket connection to $ip:$port');

      // Get singleton instance and connect
      final wsService = WsConnectionService();
      final boardConfig = await wsService.connect(host: ip, port: port);

      logger.info('WebSocket connection successful: $boardConfig');

      _stopWiFiPolling();

      // Save to preferences (including SSID)
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('connection_ip', ip);
      await prefs.setString('connection_port', portStr);
      if (_currentSSID != null) {
        await prefs.setString('connection_ssid', _currentSSID!);
      }

      logger.info('Configuration saved - IP: $ip, Port: $portStr, SSID: $_currentSSID');

      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text(l10n.connectedReadyMessage), backgroundColor: colors.success, duration: const Duration(seconds: 2)));

        // Navigate to image upload screen with board config
        await Future.delayed(const Duration(milliseconds: 500));
        if (mounted) {
          Navigator.of(context).push(
            MaterialPageRoute(
              builder: (_) => ImageSelectScreen(boardConfig: boardConfig),
              settings: const RouteSettings(name: '/image_select'),
            ),
          );
        }
      }
    } catch (e) {
      logger.severe('Failed to save configuration or connect: $e');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(l10n.connectionFailedMessage(e.toString())), backgroundColor: colors.error, duration: const Duration(seconds: 3)),
        );
      }
      if (!WsConnectionService().isConnected) {
        _startWiFiPolling();
      }
    } finally {
      if (mounted) {
        setState(() {
          _isConnecting = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = ThemeColors(context);
    final l10n = AppLocalizations.of(context)!;
    return Scaffold(
      appBar: AppBar(title: Text(l10n.ePaperConnectionTitle), centerTitle: true),
      body: AbsorbPointer(
        absorbing: _isConnecting,
        child: Opacity(
          opacity: _isConnecting ? 0.6 : 1.0,
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24.0),
            child: Form(
              key: _formKey,
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.stretch,
                children: [
                  // WiFi Status Card
                  Card(
                    elevation: 2,
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            children: [
                              Icon(_wifiValid ? Icons.wifi : Icons.wifi_off, color: _wifiValid ? colors.success : colors.warning),
                              const SizedBox(width: 8),
                              Text(l10n.wifiConnectionTitle, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                            ],
                          ),
                          const SizedBox(height: 12),
                          if (_isLoadingWiFi)
                            Row(
                              children: [
                                SizedBox(width: 16, height: 16, child: CircularProgressIndicator(strokeWidth: 2)),
                                SizedBox(width: 12),
                                Text(l10n.checkingWifiMessage),
                              ],
                            )
                          else ...[
                            Text(l10n.currentNetworkLabel(_currentSSID ?? l10n.notConnectedLabel), style: const TextStyle(fontSize: 14)),
                            if (_expectedSSID != null) ...[
                              const SizedBox(height: 4),
                              Text(
                                l10n.requiredNetworkLabel(_expectedSSID!),
                                style: TextStyle(fontSize: 14, color: _wifiValid ? Colors.green : Colors.orange, fontWeight: FontWeight.bold),
                              ),
                            ] else ...[
                              const SizedBox(height: 4),
                              Text(
                                l10n.networkPrefixRequirement('PhotoFrame-'),
                                style: TextStyle(fontSize: 14, color: _wifiValid ? Colors.green : Colors.orange, fontWeight: FontWeight.bold),
                              ),
                            ],
                            const SizedBox(height: 4),
                            Text(l10n.wifiSettingsInstruction, style: const TextStyle(fontSize: 14, color: Colors.grey)),
                          ],
                          const SizedBox(height: 16),
                          Row(
                            children: [
                              OutlinedButton.icon(
                                onPressed: _openWiFiSettings,
                                icon: const Icon(Icons.settings),
                                label: Text(l10n.wifiSettingsAction),
                              ),
                              const SizedBox(width: 12),
                              OutlinedButton.icon(onPressed: _checkWiFiConnection, icon: const Icon(Icons.refresh), label: Text(l10n.refreshAction)),
                            ],
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  // Connection Configuration Card
                  Card(
                    elevation: 2,
                    child: Padding(
                      padding: const EdgeInsets.all(16.0),
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Row(
                            children: [
                              Icon(Icons.settings_ethernet, color: colors.primary),
                              const SizedBox(width: 8),
                              Text(l10n.connectionSettingsTitle, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
                            ],
                          ),
                          const SizedBox(height: 16),
                          TextFormField(
                            controller: _ipController,
                            decoration: InputDecoration(
                              labelText: l10n.ipAddressLabel,
                              hintText: '192.168.4.1',
                              prefixIcon: Icon(Icons.computer),
                              border: OutlineInputBorder(),
                            ),
                            keyboardType: TextInputType.number,
                            validator: (value) {
                              if (value == null || value.isEmpty) {
                                return l10n.ipAddressRequiredMessage;
                              }
                              // Basic IP validation
                              final parts = value.split('.');
                              if (parts.length != 4) {
                                return l10n.ipAddressFormatInvalidMessage;
                              }
                              for (final part in parts) {
                                final num = int.tryParse(part);
                                if (num == null || num < 0 || num > 255) {
                                  return l10n.ipAddressInvalidMessage;
                                }
                              }
                              return null;
                            },
                          ),
                          const SizedBox(height: 16),
                          TextFormField(
                            controller: _portController,
                            decoration: InputDecoration(
                              labelText: l10n.portLabel,
                              hintText: '81',
                              prefixIcon: Icon(Icons.power),
                              border: OutlineInputBorder(),
                            ),
                            keyboardType: TextInputType.number,
                            validator: (value) {
                              if (value == null || value.isEmpty) {
                                return l10n.portRequiredMessage;
                              }
                              final port = int.tryParse(value);
                              if (port == null || port < 1 || port > 65535) {
                                return l10n.portInvalidMessage;
                              }
                              return null;
                            },
                          ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 32),
                  // Continue Button
                  FilledButton(
                    onPressed: (!_isConnecting) ? _saveAndContinue : null,
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        if (_isConnecting) ...[
                          SizedBox(
                            height: 24,
                            width: 24,
                            child: CircularProgressIndicator(strokeWidth: 2, valueColor: AlwaysStoppedAnimation<Color>(colors.primary)),
                          ),
                          const SizedBox(width: 12),
                        ],
                        Text(l10n.saveConnectAction, style: TextStyle(fontSize: 16)),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ),
        ),
      ),
    );
  }
}
