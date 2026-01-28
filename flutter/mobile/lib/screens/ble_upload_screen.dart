import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

import '../models/processing_models.dart';
import '../services/ble_service.dart';
import '../services/bin_parser.dart';
import '../state/image_processing_state.dart';
import '../utils/app_logger.dart';

class BleUploadScreen extends StatefulWidget {
  final File image;
  final ProcessingJob job;

  const BleUploadScreen({super.key, required this.image, required this.job});

  @override
  State<BleUploadScreen> createState() => _BleUploadScreenState();
}

class _BleUploadScreenState extends State<BleUploadScreen> {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  final List<DiscoveredDevice> _devices = [];
  StreamSubscription<DiscoveredDevice>? _scanSub;
  DiscoveredDevice? _selected;
  bool _scanning = false;

  static const int _manufacturerId = 0x1337;
  static const List<int> _manufacturerMagic = [0x50, 0x46, 0x52, 0x31]; // "PFR1"
  static const String _devicePrefix = 'ESP32-PhotoFrame';
  static const String _deviceFallbackPrefix = 'PhotoFrame-';

  @override
  void initState() {
    super.initState();
    // Start scanning automatically when entering the screen
    WidgetsBinding.instance.addPostFrameCallback((_) {
      _startScan();
    });
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    super.dispose();
  }

  bool _isPhotoFrame(DiscoveredDevice device) {
    // Check manufacturer data for 0x1337 with "PFR1" magic
    final mfgData = device.manufacturerData;
    if (mfgData.isNotEmpty && mfgData.length >= 6) {
      // First 2 bytes are manufacturer ID (little endian)
      final manufacturerId = mfgData[0] | (mfgData[1] << 8);
      if (manufacturerId == _manufacturerId) {
        // Next 4 bytes should be "PFR1" magic
        if (mfgData[2] == _manufacturerMagic[0] &&
            mfgData[3] == _manufacturerMagic[1] &&
            mfgData[4] == _manufacturerMagic[2] &&
            mfgData[5] == _manufacturerMagic[3]) {
          return true;
        }
      }
    }

    // Fallback: check device name
    final name = device.name;
    if (name.startsWith(_devicePrefix) || name.startsWith(_deviceFallbackPrefix)) {
      return true;
    }

    return false;
  }

  Future<bool> _requestBluetoothPermissions() async {
    if (Platform.isAndroid) {
      // For Android 12+ (API 31+), need Bluetooth permissions
      // For Android <= 11 (API 30), need Location permission
      Map<Permission, PermissionStatus> statuses = await [
        Permission.bluetoothScan,
        Permission.bluetoothConnect,
        Permission.locationWhenInUse,
      ].request();

      // Check if any required permission is denied
      bool allGranted = statuses.values.every((status) => status.isGranted);

      if (!allGranted) {
        // Check if permanently denied
        bool anyPermanentlyDenied = statuses.values.any((status) => status.isPermanentlyDenied);

        if (anyPermanentlyDenied && mounted) {
          await showDialog(
            context: context,
            builder: (context) => AlertDialog(
              title: const Text('Permissions Required'),
              content: const Text(
                'Bluetooth and Location permissions are required to scan for devices. '
                'Please enable them in Settings.',
              ),
              actions: [
                TextButton(onPressed: () => Navigator.pop(context), child: const Text('Cancel')),
                TextButton(
                  onPressed: () {
                    openAppSettings();
                    Navigator.pop(context);
                  },
                  child: const Text('Open Settings'),
                ),
              ],
            ),
          );
        } else if (mounted) {
          ScaffoldMessenger.of(
            context,
          ).showSnackBar(const SnackBar(content: Text('Bluetooth and Location permissions are required to scan for devices')));
        }
        return false;
      }

      return true;
    }

    // iOS doesn't require runtime permission request for Bluetooth
    return true;
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Upload via Bluetooth')),
      body: Consumer<ImageProcessingState>(
        builder: (context, state, _) {
          return FutureBuilder<Uint8List?>(
            future: state.getIntermediateFileAsBytes(),
            builder: (context, snapshot) {
              final previewBytes = snapshot.data;

              return Column(
                children: [
                  // Preview image at the top (final image with annotation)
                  if (previewBytes != null)
                    Container(
                      padding: const EdgeInsets.all(16),
                      child: ConstrainedBox(
                        constraints: BoxConstraints(maxHeight: MediaQuery.of(context).size.height * 0.25),
                        child: AspectRatio(
                          aspectRatio: widget.job.targetResolution.width / widget.job.targetResolution.height,
                          child: ClipRRect(
                            borderRadius: BorderRadius.circular(12),
                            child: Image.memory(previewBytes, fit: BoxFit.contain),
                          ),
                        ),
                      ),
                    ),

                  // Header info card (requires bin already generato)
                  Padding(padding: const EdgeInsets.symmetric(horizontal: 16), child: _buildHeaderCard(state)),

                  // Scan and Upload buttons side by side
                  Padding(
                    padding: const EdgeInsets.all(16),
                    child: Row(
                      children: [
                        Expanded(
                          child: FilledButton.icon(
                            onPressed: _scanning ? null : _startScan,
                            icon: Icon(_scanning ? Icons.radar : Icons.bluetooth_searching),
                            label: Text(_scanning ? 'Scanning...' : 'Scan'),
                            style: FilledButton.styleFrom(padding: const EdgeInsets.symmetric(vertical: 12)),
                          ),
                        ),
                        const SizedBox(width: 12),
                        Expanded(
                          child: FilledButton.icon(
                            onPressed: _selected == null ? null : () => _connectAndUpload(_selected!),
                            icon: const Icon(Icons.cloud_upload),
                            label: const Text('Upload'),
                            style: FilledButton.styleFrom(padding: const EdgeInsets.symmetric(vertical: 12)),
                          ),
                        ),
                      ],
                    ),
                  ),

                  // Device list
                  Expanded(child: _buildDeviceList()),
                ],
              );
            },
          );
        },
      ),
    );
  }

  Widget _buildHeaderCard(ImageProcessingState state) {
    final data = state.binaryData;
    final header = data != null ? _parsePfr1Header(data) : null;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(Icons.description_outlined),
                const SizedBox(width: 8),
                Text('Header file', style: Theme.of(context).textTheme.titleMedium),
              ],
            ),
            const SizedBox(height: 8),
            if (header == null)
              const Text('Header non disponibile: genera prima il file (.bin).')
            else ...[
              _headerRow('Magic', 'PFR1 (0x50465231)'),
              _headerRow('Versione', header.version.toString()),
              _headerRow('Dimensioni', '${header.width} x ${header.height}'),
              _headerRow('Rotazione', _rotationLabel(header.rotation)),
              _headerRow('Color mode', _colorModeLabel(header.colorMode)),
              _headerRow('Payload', '${header.payloadLen} bytes'),
              _headerRow('Header len', '${header.headerLen} bytes'),
              _headerRow('File totale', '${data!.length} bytes'),
            ],
          ],
        ),
      ),
    );
  }

  Widget _headerRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(color: Colors.grey)),
          Text(value, style: const TextStyle(fontFeatures: [FontFeature.tabularFigures()])),
        ],
      ),
    );
  }

  String _rotationLabel(int value) {
    switch (value % 4) {
      case 0:
        return '0° (landscape)';
      case 1:
        return '90° (portrait)';
      case 2:
        return '180°';
      case 3:
        return '270°';
    }
    return value.toString();
  }

  String _colorModeLabel(int value) {
    switch (value) {
      case 0:
        return 'Black & White';
      case 1:
        return 'Six Colors';
      default:
        return 'Unknown ($value)';
    }
  }

  BinHeader? _parsePfr1Header(Uint8List data) {
    try {
      final parsed = BinParser.parse(data);
      return parsed.header;
    } catch (e) {
      logger.warning('Failed to parse PFR1 header: $e');
      return null;
    }
  }

  Widget _buildDeviceList() {
    if (_devices.isEmpty && !_scanning) {
      return const Center(
        child: Padding(
          padding: EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(Icons.bluetooth_disabled, size: 64, color: Colors.grey),
              SizedBox(height: 16),
              Text('No devices found', style: TextStyle(fontSize: 18, fontWeight: FontWeight.w500)),
              SizedBox(height: 8),
              Text(
                'Tap "Scan" to search for Photoframe devices',
                textAlign: TextAlign.center,
                style: TextStyle(color: Colors.grey),
              ),
            ],
          ),
        ),
      );
    }

    if (_scanning && _devices.isEmpty) {
      return const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [CircularProgressIndicator(), SizedBox(height: 16), Text('Searching for devices...')],
        ),
      );
    }

    return ListView.builder(
      padding: const EdgeInsets.symmetric(horizontal: 16),
      itemCount: _devices.length,
      itemBuilder: (context, index) {
        final device = _devices[index];
        final isSelected = _selected?.id == device.id;

        return Card(
          margin: const EdgeInsets.only(bottom: 8),
          child: ListTile(
            leading: Icon(Icons.devices, color: isSelected ? Theme.of(context).colorScheme.primary : null),
            title: Text(
              device.name.isNotEmpty ? device.name : 'Unknown Device',
              style: TextStyle(fontWeight: isSelected ? FontWeight.bold : FontWeight.normal),
            ),
            subtitle: Text('${device.id}\nRSSI: ${device.rssi} dBm'),
            isThreeLine: true,
            trailing: isSelected
                ? Icon(Icons.check_circle, color: Theme.of(context).colorScheme.primary)
                : ElevatedButton(onPressed: () => _selectDevice(device), child: const Text('Select')),
          ),
        );
      },
    );
  }

  Future<void> _startScan() async {
    // Request permissions before scanning
    final hasPermission = await _requestBluetoothPermissions();
    if (!hasPermission) {
      return;
    }

    setState(() {
      _scanning = true;
      _devices.clear();
    });

    _scanSub?.cancel();
    _scanSub = _ble
        .scanForDevices(withServices: [], scanMode: ScanMode.balanced)
        .listen(
          (device) {
            // Filter only PhotoFrame devices using manufacturer ID or name
            if (_isPhotoFrame(device)) {
              final already = _devices.any((d) => d.id == device.id);
              if (!already) {
                setState(() {
                  _devices.add(device);
                });
              }
            }
          },
          onError: (e) {
            setState(() {
              _scanning = false;
            });

            if (context.mounted) {
              ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Scan error: $e')));
            }
          },
        );

    // Scan for 6 seconds
    await Future.delayed(const Duration(seconds: 6));
    await _scanSub?.cancel();
    if (mounted) {
      setState(() => _scanning = false);
    }
  }

  Future<void> _selectDevice(DiscoveredDevice device) async {
    setState(() {
      _selected = device;
    });
  }

  Future<void> _connectAndUpload(DiscoveredDevice device) async {
    final state = Provider.of<ImageProcessingState>(context, listen: false);
    final bleService = BleService(_ble);

    try {
      // Ensure binary data is ready (with PFR1 header)
      Uint8List? binary = state.binaryData;
      binary ??= await state.generateBinaryData();
      if (binary == null) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Impossibile generare il file .bin')));
        }
        return;
      }
      final Uint8List uploadData = binary;

      // Show progress dialog
      if (!mounted) return;
      showDialog(
        context: context,
        barrierDismissible: false,
        builder: (context) => _UploadProgressDialog(bleService: bleService, device: device, imageData: uploadData, job: widget.job),
      );
    } catch (e) {
      logger.severe('Upload failed: $e');
      if (mounted) {
        Navigator.of(context).pop(); // Close dialog if open
        ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text('Upload failed: $e')));
      }
    }
  }
}

class _UploadProgressDialog extends StatefulWidget {
  final BleService bleService;
  final DiscoveredDevice device;
  final Uint8List imageData;
  final ProcessingJob job;

  const _UploadProgressDialog({required this.bleService, required this.device, required this.imageData, required this.job});

  @override
  State<_UploadProgressDialog> createState() => _UploadProgressDialogState();
}

class _UploadProgressDialogState extends State<_UploadProgressDialog> {
  double _progress = 0.0;
  String _status = 'Connecting...';
  bool _success = false;
  bool _cancelled = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _startUpload();
  }

  Future<void> _cancel() async {
    if (_cancelled) return;

    setState(() {
      _cancelled = true;
      _status = 'Cancelling...';
    });

    // Disconnect from device
    try {
      await widget.bleService.disconnect();
    } catch (e) {
      logger.warning('Error during disconnect: $e');
    }

    if (mounted) {
      Navigator.of(context).pop();
    }
  }

  @override
  void dispose() {
    // Ensure proper cleanup
    widget.bleService.disconnect();
    widget.bleService.dispose();
    super.dispose();
  }

  Future<void> _startUpload() async {
    try {
      // Connect
      if (mounted) setState(() => _status = 'Connecting to device...');
      await widget.bleService.connect(widget.device.id);

      if (_cancelled) return;

      // Read device info
      if (mounted) setState(() => _status = 'Reading device configuration...');
      final deviceInfo = await widget.bleService.readDeviceInfo();

      if (_cancelled) return;

      // Note: imageData now contains PFR1 header (21 bytes) + payload + CRC32 (4 bytes)
      // No need to validate pixel count since header contains dimensions
      logger.info('Uploading .bin file: ${widget.imageData.length} bytes total');

      // Determine orientation (portrait = 1, landscape = 0)
      final orientation = widget.job.isPortrait ? 1 : 0;

      // Upload
      if (mounted) setState(() => _status = 'Uploading image...');
      await widget.bleService.uploadImage(
        imageData: widget.imageData,
        width: widget.job.targetResolution.width.toInt(),
        height: widget.job.targetResolution.height.toInt(),
        orientation: orientation,
        deviceInfo: deviceInfo,
        onProgress: (progress) {
          if (mounted) {
            setState(() => _progress = progress);
          }
        },
      );

      // Success
      if (mounted) {
        setState(() {
          _success = true;
          _status = 'Upload complete!';
          _progress = 1.0;
        });
      }
    } catch (e) {
      logger.severe('Upload error: $e');
      if (mounted) {
        setState(() {
          _error = e.toString();
          _status = 'Upload failed';
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_success) {
      return AlertDialog(
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.check_circle, color: Colors.green, size: 80),
            const SizedBox(height: 24),
            Text('Immagine inviata con successo', style: Theme.of(context).textTheme.titleLarge, textAlign: TextAlign.center),
          ],
        ),
        actions: [
          FilledButton(
            onPressed: () {
              Navigator.of(context).pop(); // Close dialog
              Navigator.of(context).popUntil((route) => route.isFirst); // Go to home
            },
            child: const Text('Fine'),
          ),
        ],
      );
    }

    if (_error != null) {
      return AlertDialog(
        title: Row(
          children: [
            Icon(Icons.error, color: Theme.of(context).colorScheme.error),
            const SizedBox(width: 8),
            const Text('Upload Failed'),
          ],
        ),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [Text(_error!.substring(0, _error!.length.clamp(0, 500)))],
        ),
        actions: [TextButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Close'))],
      );
    }

    return AlertDialog(
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          CircularProgressIndicator(value: _progress > 0 ? _progress : null),
          const SizedBox(height: 24),
          Text(_status),
          if (_progress > 0) ...[const SizedBox(height: 8), Text('${(_progress * 100).toStringAsFixed(0)}%')],
        ],
      ),
      actions: [TextButton(onPressed: _cancelled ? null : _cancel, child: const Text('Cancel'))],
    );
  }
}
