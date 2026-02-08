import 'dart:async';
import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:app_settings/app_settings.dart';
import 'package:mobile_scanner/mobile_scanner.dart';
import 'package:photoframe/models/binary_model.dart';
import 'package:wakelock_plus/wakelock_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/processing_models.dart';
import '../services/websocket_service.dart' as ws;
import '../services/wifi_service.dart';
import '../services/bin_parser.dart';
import '../utils/app_logger.dart';

class WebSocketUploadScreen extends StatefulWidget {
  final File pfr1File;
  final ProcessingJob job;
  final String? deviceIp;
  final String? deviceSsid;
  final int? devicePort;

  const WebSocketUploadScreen({super.key, required this.pfr1File, required this.job, this.deviceIp, this.deviceSsid, this.devicePort});

  @override
  State<WebSocketUploadScreen> createState() => _WebSocketUploadScreenState();
}

class _WebSocketUploadScreenState extends State<WebSocketUploadScreen> {
  final ws.WebSocketService _wsService = ws.WebSocketService();
  final WifiService _wifiService = WifiService();
  final TextEditingController _ipController = TextEditingController();
  final TextEditingController _portController = TextEditingController(text: '8080');
  final TextEditingController _ssidController = TextEditingController();

  late final Future<Pfr1ViewData?> _pfr1Future;
  Pfr1ViewData? _pfr1Data;
  int _selectedOrientation = 0;

  String? _currentSsid;
  bool _wifiMatches = false;
  bool _connecting = false;
  bool _uploading = false;
  final ValueNotifier<double> _uploadProgress = ValueNotifier(0.0);
  String? _errorMessage;
  String? _successMessage;

  // Deep link state - indicates if device connection details came from deep link
  bool _hasDeepLinkData = false;

  // Connection phase state
  bool _hasConnectionData = false; // Whether we have connection details (manual or from deep link)
  bool _showQrScanner = false;

  MobileScannerController? _scannerController;
  Timer? _wifiPollTimer;
  int _lastAutoConnectMs = 0;
  bool _autoConnectInProgress = false;
  static const Duration _autoConnectCooldown = Duration(seconds: 15);

  // Keys for SharedPreferences
  static const String _prefKeyDeviceIp = 'device_ip';
  static const String _prefKeyDeviceSsid = 'device_ssid';
  static const String _prefKeyDevicePort = 'device_port';

  @override
  void initState() {
    super.initState();
    _pfr1Future = _loadPfr1Data();
    // Start with 0° orientation (landscape), unless job has specific rotation
    _selectedOrientation = widget.job.rotation; // Job rotation is already in quarterTurns (0, 1, 2, 3)

    // Check if we have deep link data
    _hasDeepLinkData = widget.deviceIp != null && widget.deviceSsid != null;
    _hasConnectionData = _hasDeepLinkData;

    // Pre-fill from deep link if available
    if (_hasDeepLinkData) {
      _ipController.text = widget.deviceIp!;
      _ssidController.text = widget.deviceSsid!;
      if (widget.devicePort != null) {
        _portController.text = widget.devicePort.toString();
      }
    } else {
      // Try to load saved connection details
      _loadSavedConnectionDetails();
    }

    if (_hasConnectionData) {
      _checkWifiStatus();
      _attemptAutoConnect();
    }

    _startWifiPolling();

    // Listen to connection state changes
    _wsService.stateStream.listen((state) {
      if (mounted) {
        setState(() {
          _connecting = state == ws.WsConnectionState.connecting;
          if (state == ws.WsConnectionState.error) {
            _errorMessage = _wsService.lastError ?? 'Connection failed';
          } else if (state == ws.WsConnectionState.connected) {
            _errorMessage = null;
          }
        });
      }
    });
  }

  @override
  void dispose() {
    _wifiPollTimer?.cancel();
    _scannerController?.dispose();
    _wsService.dispose();
    _wifiService.clearWifiBinding();
    _ipController.dispose();
    _portController.dispose();
    _ssidController.dispose();
    _uploadProgress.dispose();
    super.dispose();
  }

  void _startWifiPolling() {
    _wifiPollTimer?.cancel();
    _wifiPollTimer = Timer.periodic(const Duration(seconds: 2), (_) {
      _checkWifiStatus();
    });
  }

  Future<void> _loadSavedConnectionDetails() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final savedIp = prefs.getString(_prefKeyDeviceIp);
      final savedSsid = prefs.getString(_prefKeyDeviceSsid);
      final savedPort = prefs.getString(_prefKeyDevicePort);

      if (savedIp != null || savedSsid != null) {
        if (mounted) {
          setState(() {
            if (savedIp != null) _ipController.text = savedIp;
            if (savedSsid != null) _ssidController.text = savedSsid;
            if (savedPort != null) _portController.text = savedPort;
          });
        }
      }
    } catch (e) {
      logger.warning('Failed to load saved connection details: $e');
    }
  }

  Future<void> _saveConnectionDetails() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(_prefKeyDeviceIp, _ipController.text.trim());
      await prefs.setString(_prefKeyDeviceSsid, _ssidController.text.trim());
      await prefs.setString(_prefKeyDevicePort, _portController.text.trim());
    } catch (e) {
      logger.warning('Failed to save connection details: $e');
    }
  }

  Future<void> _checkWifiStatus() async {
    final ssid = await _wifiService.getCurrentSsid();
    if (mounted) {
      final expectedSsid = _ssidController.text.trim().isNotEmpty ? _ssidController.text.trim() : widget.deviceSsid;
      final newWifiMatches = expectedSsid != null && ssid != null && ssid.toLowerCase() == expectedSsid.toLowerCase();

      // Only setState if values actually changed
      final ssidChanged = ssid != _currentSsid;
      final wifiMatchChanged = newWifiMatches != _wifiMatches;

      if (ssidChanged || wifiMatchChanged) {
        setState(() {
          _currentSsid = ssid;
          _wifiMatches = newWifiMatches;
        });

        if ((ssidChanged || wifiMatchChanged) && newWifiMatches) {
          _maybeAutoConnect();
        }
      }
    }
  }

  void _maybeAutoConnect() {
    if (!_hasConnectionData) return;
    if (_autoConnectInProgress) return;
    if (_connecting || _wsService.state == ws.WsConnectionState.connected) return;

    final now = DateTime.now().millisecondsSinceEpoch;
    if (now - _lastAutoConnectMs < _autoConnectCooldown.inMilliseconds) {
      return;
    }

    _lastAutoConnectMs = now;
    _attemptAutoConnect();
  }

  Future<void> _attemptAutoConnect() async {
    final ip = _ipController.text.trim();
    final portStr = _portController.text.trim();

    if (ip.isEmpty) return;

    // Guard: don't attempt if already connecting or connected
    if (_connecting || _wsService.state == ws.WsConnectionState.connected) {
      logger.fine('Skipping auto-connect: already connecting or connected');
      return;
    }

    if (_autoConnectInProgress) {
      logger.fine('Skipping auto-connect: already in progress');
      return;
    }

    final port = int.tryParse(portStr) ?? 8080;

    try {
      _autoConnectInProgress = true;
      await _wifiService.bindToWifiNetwork();
      await _wsService.connect(ip, port: port);
    } catch (e) {
      logger.warning('Auto-connect failed: $e');
      // Don't show error - user can manually retry
    } finally {
      _autoConnectInProgress = false;
    }
  }

  Future<void> _handleQrCode(String qrData) async {
    try {
      final uri = Uri.parse(qrData);

      // Expected format: photoframe://connect?ip=...&ssid=...&port=...
      if (uri.scheme == 'photoframe' && uri.host == 'connect') {
        final params = uri.queryParameters;
        final ip = params['ip'];
        final ssid = params['ssid'];

        if (ip != null && ssid != null) {
          final port = int.tryParse(params['port'] ?? '8080') ?? 8080;

          setState(() {
            _ipController.text = ip;
            _ssidController.text = ssid;
            _portController.text = port.toString();
            _hasConnectionData = true;
            _showQrScanner = false;
          });

          // Save connection details for future use
          _saveConnectionDetails();

          // Check WiFi and connect
          await _checkWifiStatus();

          if (mounted && (!_wifiMatches || _currentSsid == null)) {
            await _showWifiSettingsPrompt(ssid);
          }

          await _attemptAutoConnect();

          if (mounted) {
            ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('QR code scanned successfully'), backgroundColor: Colors.green));
          }
          return;
        }
      }

      // Not a valid deep link QR code
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('Invalid QR code - not a PhotoFrame device link'), backgroundColor: Colors.red));
      }
    } catch (e) {
      logger.warning('Failed to parse QR code: $e');
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Failed to parse QR code: $e'), backgroundColor: Colors.red));
      }
    }
  }

  Future<void> _showWifiSettingsPrompt(String expectedSsid) async {
    if (!mounted) return;

    await showDialog<void>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Connect to WiFi'),
        content: Text('Please connect to "$expectedSsid" to reach the device.'),
        actions: [
          TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Not now')),
          FilledButton.icon(
            onPressed: () {
              Navigator.of(context).pop();
              AppSettings.openAppSettings(type: AppSettingsType.wifi);
            },
            icon: const Icon(Icons.settings),
            label: const Text('Open WiFi settings'),
          ),
        ],
      ),
    );
  }

  Future<Pfr1ViewData?> _loadPfr1Data() async {
    try {
      final bytes = await widget.pfr1File.readAsBytes();
      final header = BinParser.parseHeader(bytes);
      if (header == null) return null;

      final decoded = await BinParser.decodeToImage(bytes);
      if (decoded == null) return null;

      return Pfr1ViewData(header, decoded);
    } catch (e) {
      logger.severe('Failed to load PFR1 file: $e');
      return null;
    }
  }

  void _setManualConnectionData() {
    final ip = _ipController.text.trim();
    final portStr = _portController.text.trim();
    final ssid = _ssidController.text.trim();

    if (ip.isEmpty) {
      _showError('Please enter device IP address');
      return;
    }

    final port = int.tryParse(portStr);
    if (port == null || port < 1 || port > 65535) {
      _showError('Invalid port number');
      return;
    }

    if (ssid.isEmpty) {
      _showError('Please enter device WiFi SSID');
      return;
    }

    // Save connection details for future use
    _saveConnectionDetails();

    setState(() {
      _hasConnectionData = true;
      _showQrScanner = false;
    });

    _checkWifiStatus();
    _attemptAutoConnect();
  }

  Future<void> _connect() async {
    final ip = _ipController.text.trim();
    final portStr = _portController.text.trim();

    if (ip.isEmpty) {
      _showError('Please enter device IP address');
      return;
    }

    final port = int.tryParse(portStr);
    if (port == null || port < 1 || port > 65535) {
      _showError('Invalid port number');
      return;
    }

    setState(() {
      _errorMessage = null;
      _successMessage = null;
    });

    try {
      await _wifiService.bindToWifiNetwork();
      await _wsService.connect(ip, port: port);

      if (mounted) {
        // Verify device info matches expected dimensions
        final deviceInfo = _wsService.deviceInfo;
        if (deviceInfo != null) {
          logger.info('Connected to device: ${deviceInfo.width}x${deviceInfo.height}, ${deviceInfo.displayName}');
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text('Connected to ${deviceInfo.width}x${deviceInfo.height} ${deviceInfo.displayName} display'),
              backgroundColor: Colors.green,
            ),
          );
        }
      }
    } catch (e) {
      _showError('Connection failed: $e');
    }
  }

  Future<void> _disconnect() async {
    _wsService.disconnect();
  }

  Future<void> _upload() async {
    if (_wsService.state != ws.WsConnectionState.connected) {
      _showError('Not connected to device');
      return;
    }

    if (_pfr1Data == null) {
      _showError('Image data not loaded');
      return;
    }

    setState(() {
      _uploading = true;
      _uploadProgress.value = 0.0;
      _errorMessage = null;
      _successMessage = null;
    });

    // Show the upload dialog
    if (mounted && _pfr1Data != null) {
      _showUploadDialog(_pfr1Data!);
    }

    // Enable wakelock
    await WakelockPlus.enable();

    try {
      // Read image data from PFR1 file
      final bytes = await widget.pfr1File.readAsBytes();

      // Generate filename: use job name + .pfr1 extension
      final filename = '$widget.job.name.pfr1';

      logger.info('Uploading image: ${bytes.length} bytes, filename=$filename, orientation=$_selectedOrientation°');

      await _wsService.uploadImage(
        imageData: bytes,
        filename: filename,
        orientation: _selectedOrientation,
        onProgress: (progress) {
          _uploadProgress.value = progress;
        },
      );

      if (mounted) {
        Navigator.pop(context); // Close the upload dialog
        setState(() {
          _successMessage = 'Upload successful!';
        });
        ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Image uploaded successfully!'), backgroundColor: Colors.green));
      }
    } catch (e) {
      logger.severe('Upload failed: $e');
      if (mounted) {
        Navigator.pop(context); // Close the upload dialog
      }
      _showError('Upload failed: $e');
    } finally {
      await WakelockPlus.disable();
      if (mounted) {
        setState(() {
          _uploading = false;
        });
      }
    }
  }

  void _showUploadDialog(Pfr1ViewData pfr1Data) {
    final fileSize = widget.pfr1File.lengthSync();
    final fileSizeKb = (fileSize / 1024).toStringAsFixed(1);

    showDialog<void>(
      context: context,
      barrierDismissible: false,
      builder: (context) => AlertDialog(
        title: const Text('Uploading Image'),
        content: SizedBox(
          width: double.maxFinite,
          child: SingleChildScrollView(
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                // Image preview
                Center(
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(8),
                    child: RotatedBox(
                      quarterTurns: (4 - (_selectedOrientation % 4)) % 4,
                      child: RawImage(image: pfr1Data.image, fit: BoxFit.contain),
                    ),
                  ),
                ),
                const SizedBox(height: 16),

                // File info and progress - updates in real-time via ValueListenableBuilder
                ValueListenableBuilder<double>(
                  valueListenable: _uploadProgress,
                  builder: (context, progress, _) => Column(
                    mainAxisSize: MainAxisSize.min,
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('File: $widget.job.name.pfr1', style: const TextStyle(fontSize: 12)),
                      Text('Size: ${((progress * fileSize) / 1024).toStringAsFixed(1)} KB / $fileSizeKb KB', style: const TextStyle(fontSize: 12)),
                      const SizedBox(height: 16),

                      // Progress bar
                      ClipRRect(
                        borderRadius: BorderRadius.circular(8),
                        child: LinearProgressIndicator(value: progress, minHeight: 12),
                      ),
                      const SizedBox(height: 12),

                      // Percentage
                      Center(
                        child: Text('${(progress * 100).toStringAsFixed(0)}%', style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  void _showError(String message) {
    if (mounted) {
      setState(() {
        _errorMessage = message;
      });
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message), backgroundColor: Colors.red));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Upload via WebSocket')),
      body: _hasConnectionData ? _buildUploadPhase() : _buildConnectionPhase(),
    );
  }

  /// Build the connection/setup phase
  Widget _buildConnectionPhase() {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(16.0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Text('Connect to Device', style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold)),
          const SizedBox(height: 24),
          // Option 1: Scan QR Code
          if (!_showQrScanner) ...[
            ElevatedButton.icon(
              onPressed: () {
                setState(() {
                  _showQrScanner = true;
                });
              },
              icon: const Icon(Icons.qr_code),
              label: const Text('Scan QR Code'),
              style: ElevatedButton.styleFrom(
                padding: const EdgeInsets.symmetric(vertical: 12),
                backgroundColor: Colors.blue,
                foregroundColor: Colors.white,
              ),
            ),
            const SizedBox(height: 24),
            const Divider(),
            const SizedBox(height: 24),
          ],
          // Option 2: Manual Entry
          if (_showQrScanner) ...[
            _buildQrScanner(),
          ] else ...[
            const Text('Or enter device details manually', style: TextStyle(fontSize: 14, color: Colors.grey)),
            const SizedBox(height: 16),
            TextField(
              controller: _ipController,
              decoration: const InputDecoration(
                labelText: 'Device IP Address',
                hintText: '192.168.4.1',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.computer),
              ),
              keyboardType: TextInputType.numberWithOptions(decimal: true),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _ssidController,
              decoration: const InputDecoration(
                labelText: 'WiFi SSID',
                hintText: 'PhotoFrame-XXXX',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.wifi),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _portController,
              decoration: const InputDecoration(
                labelText: 'Port',
                hintText: '81',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.settings_ethernet),
              ),
              keyboardType: TextInputType.number,
            ),
            const SizedBox(height: 24),
            FilledButton.icon(
              onPressed: _setManualConnectionData,
              icon: const Icon(Icons.done),
              label: const Text('Continue'),
              style: FilledButton.styleFrom(padding: const EdgeInsets.symmetric(vertical: 12)),
            ),
          ],
        ],
      ),
    );
  }

  /// Build the QR scanner widget
  Widget _buildQrScanner() {
    return Column(
      children: [
        Container(
          height: 300,
          decoration: BoxDecoration(
            borderRadius: BorderRadius.circular(8),
            border: Border.all(color: Colors.blue),
          ),
          child: MobileScanner(
            onDetect: (capture) {
              final List<Barcode> barcodes = capture.barcodes;
              for (final barcode in barcodes) {
                if (barcode.rawValue != null) {
                  _handleQrCode(barcode.rawValue!);
                  break;
                }
              }
            },
          ),
        ),
        const SizedBox(height: 16),
        const Text('Point your camera at the QR code', style: TextStyle(fontSize: 14, color: Colors.grey)),
        const SizedBox(height: 16),
        TextButton.icon(
          onPressed: () {
            setState(() {
              _showQrScanner = false;
            });
          },
          icon: const Icon(Icons.close),
          label: const Text('Cancel'),
        ),
      ],
    );
  }

  /// Build the upload phase
  Widget _buildUploadPhase() {
    return FutureBuilder<Pfr1ViewData?>(
      future: _pfr1Future,
      builder: (context, snapshot) {
        final data = snapshot.data ?? _pfr1Data;
        final header = data?.header;
        final previewImage = data?.image;

        if (data != null && _pfr1Data == null) {
          _pfr1Data = data;
          if (header != null) {
            _selectedOrientation = header.rotation;
          }
        }

        return SingleChildScrollView(
          physics: _uploading ? const NeverScrollableScrollPhysics() : const AlwaysScrollableScrollPhysics(),
          padding: const EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              // WiFi Status Card
              _buildWifiStatusCard(),
              const SizedBox(height: 16),

              // Connection Card
              _buildConnectionCard(),
              const SizedBox(height: 16),

              // Preview Card - Hidden during upload to prevent rendering issues
              if (previewImage != null && header != null && !_uploading) ...[_buildPreviewCard(previewImage, header), const SizedBox(height: 16)],

              // Orientation Selection - Disabled during upload
              if (header != null && !_uploading) ...[_buildOrientationCard(header), const SizedBox(height: 16)],

              // Upload Button
              _buildUploadButton(),

              // Error/Success Messages
              if (_errorMessage != null) ...[const SizedBox(height: 16), _buildErrorCard()],
              if (_successMessage != null) ...[const SizedBox(height: 16), _buildSuccessCard()],
            ],
          ),
        );
      },
    );
  }

  Widget _buildWifiStatusCard() {
    final showSettingsButton = !_wifiMatches || _currentSsid == null;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(_wifiMatches ? Icons.wifi : Icons.wifi_off, color: _wifiMatches ? Colors.green : Colors.orange),
                const SizedBox(width: 8),
                const Text('WiFi Status', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              ],
            ),
            const SizedBox(height: 8),
            Text('Expected: ${_ssidController.text}'),
            Text('Current: ${_currentSsid ?? "Not connected"}'),
            if (!_wifiMatches && _currentSsid != null) ...[
              const SizedBox(height: 8),
              Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(color: Colors.orange.withOpacity(0.1), borderRadius: BorderRadius.circular(4)),
                child: const Text(
                  '⚠️ You are connected to a different WiFi network. '
                  'Please connect to the correct network to communicate with the device.',
                  style: TextStyle(fontSize: 12),
                ),
              ),
            ],
            if (_currentSsid == null) ...[
              const SizedBox(height: 8),
              Container(
                padding: const EdgeInsets.all(8),
                decoration: BoxDecoration(color: Colors.red.withOpacity(0.1), borderRadius: BorderRadius.circular(4)),
                child: const Text('❌ Not connected to WiFi. Please connect to the device\'s WiFi network.', style: TextStyle(fontSize: 12)),
              ),
            ],
            if (showSettingsButton) ...[
              const SizedBox(height: 12),
              OutlinedButton.icon(
                onPressed: () => AppSettings.openAppSettings(type: AppSettingsType.wifi),
                icon: const Icon(Icons.settings),
                label: const Text('Open WiFi settings'),
              ),
            ],
          ],
        ),
      ),
    );
  }

  double _previewAspectRatio() {
    final width = widget.job.targetResolution.width;
    final height = widget.job.targetResolution.height;
    if (_selectedOrientation % 2 == 1) {
      return height / width;
    }
    return width / height;
  }

  Widget _buildConnectionCard() {
    final isConnected = _wsService.state == ws.WsConnectionState.connected;
    final deviceInfo = _wsService.deviceInfo;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Icon(isConnected ? Icons.link : Icons.link_off, color: isConnected ? Colors.green : Colors.grey),
                const SizedBox(width: 8),
                const Text('Connection', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
              ],
            ),
            const SizedBox(height: 16),
            if (isConnected && deviceInfo != null) ...[
              Container(
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(color: Colors.green.withOpacity(0.1), borderRadius: BorderRadius.circular(8)),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      '✓ Connected to device',
                      style: TextStyle(color: Colors.green[700], fontWeight: FontWeight.bold),
                    ),
                    const SizedBox(height: 4),
                    Text('Display: ${deviceInfo.width}x${deviceInfo.height}'),
                    Text('Type: ${deviceInfo.displayName}'),
                    Text('Rotation: ${deviceInfo.rotation}°'),
                  ],
                ),
              ),
              const SizedBox(height: 8),
            ],
            Row(
              children: [
                Expanded(
                  child: ElevatedButton.icon(
                    onPressed: _connecting ? null : (isConnected ? _disconnect : _connect),
                    icon: Icon(_connecting ? Icons.hourglass_empty : (isConnected ? Icons.link_off : Icons.link)),
                    label: Text(_connecting ? 'Connecting...' : (isConnected ? 'Disconnect' : 'Connect')),
                    style: ElevatedButton.styleFrom(backgroundColor: isConnected ? Colors.red : Colors.blue, foregroundColor: Colors.white),
                  ),
                ),
                const SizedBox(width: 8),
                OutlinedButton.icon(
                  onPressed: () {
                    setState(() {
                      _hasConnectionData = false;
                      _showQrScanner = false;
                    });
                  },
                  icon: const Icon(Icons.edit),
                  label: const Text('Edit'),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPreviewCard(ui.Image previewImage, BinHeader header) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Preview', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            Container(
              padding: const EdgeInsets.all(16),
              child: ConstrainedBox(
                constraints: BoxConstraints(maxHeight: MediaQuery.of(context).size.height * 0.25),
                child: AspectRatio(
                  aspectRatio: _previewAspectRatio(),
                  child: RepaintBoundary(
                    child: ClipRRect(
                      borderRadius: BorderRadius.circular(12),
                      child: RotatedBox(
                        quarterTurns: (4 - (_selectedOrientation % 4)) % 4,
                        child: Image(image: _UiImageProvider(previewImage), fit: BoxFit.contain),
                      ),
                    ),
                  ),
                ),
              ),
            ),
            const SizedBox(height: 8),
            Text('Size: ${header.width}x${header.height}'),
            Text('File size: ${(widget.pfr1File.lengthSync() / 1024).toStringAsFixed(1)} KB'),
            Text('Orientation: $_selectedOrientation°'),
          ],
        ),
      ),
    );
  }

  Widget _buildOrientationCard(BinHeader header) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text('Display Orientation', style: TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
            const SizedBox(height: 8),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                _buildOrientationButton(0, 'Landscape 0°', Icons.stay_current_landscape),
                _buildOrientationButton(1, 'Portrait 90°', Icons.stay_current_portrait),
                _buildOrientationButton(2, 'Landscape 180°', Icons.stay_current_landscape),
                _buildOrientationButton(3, 'Portrait 270°', Icons.stay_current_portrait),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildOrientationButton(int orientation, String label, IconData icon) {
    final isSelected = _selectedOrientation == orientation;
    return FilterChip(
      label: Row(mainAxisSize: MainAxisSize.min, children: [Icon(icon, size: 18), const SizedBox(width: 4), Text(label)]),
      selected: isSelected,
      onSelected: (_) {
        setState(() {
          _selectedOrientation = orientation;
        });
      },
      backgroundColor: Colors.grey[200],
      selectedColor: Colors.blue,
      labelStyle: TextStyle(color: isSelected ? Colors.white : Colors.black, fontWeight: isSelected ? FontWeight.bold : FontWeight.normal),
    );
  }

  Widget _buildUploadButton() {
    final canUpload = _wsService.state == ws.WsConnectionState.connected && !_uploading;

    return ElevatedButton.icon(
      onPressed: canUpload ? _upload : null,
      icon: const Icon(Icons.upload),
      label: const Text('Upload Image'),
      style: ElevatedButton.styleFrom(
        backgroundColor: Colors.green,
        foregroundColor: Colors.white,
        padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 16),
        textStyle: const TextStyle(fontSize: 18),
      ),
    );
  }

  Widget _buildErrorCard() {
    return Card(
      color: Colors.red.withOpacity(0.1),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Row(
          children: [
            const Icon(Icons.error, color: Colors.red),
            const SizedBox(width: 8),
            Expanded(
              child: Text(_errorMessage!, style: const TextStyle(color: Colors.red)),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildSuccessCard() {
    return Card(
      color: Colors.green.withOpacity(0.1),
      child: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Row(
          children: [
            const Icon(Icons.check_circle, color: Colors.green),
            const SizedBox(width: 8),
            Expanded(
              child: Text(_successMessage!, style: const TextStyle(color: Colors.green)),
            ),
          ],
        ),
      ),
    );
  }
}

class _UiImageProvider extends ImageProvider<_UiImageProvider> {
  final ui.Image image;

  const _UiImageProvider(this.image);

  @override
  Future<_UiImageProvider> obtainKey(ImageConfiguration configuration) => Future.value(this);

  @override
  ImageStreamCompleter loadImage(_UiImageProvider key, ImageDecoderCallback decode) {
    return OneFrameImageStreamCompleter(Future.value(ImageInfo(image: image)));
  }
}
