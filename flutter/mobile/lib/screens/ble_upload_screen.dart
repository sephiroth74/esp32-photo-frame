import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';
import '../models/processing_models.dart';
import '../services/ble_service.dart';
import '../services/bin_parser.dart';
import '../utils/app_logger.dart';

class BleUploadScreen extends StatefulWidget {
  final File pfr1File;
  final ProcessingJob job;

  const BleUploadScreen({super.key, required this.pfr1File, required this.job});

  @override
  State<BleUploadScreen> createState() => _BleUploadScreenState();
}

class _BleUploadScreenState extends State<BleUploadScreen> {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  final List<DiscoveredDevice> _devices = [];
  StreamSubscription<DiscoveredDevice>? _scanSub;
  DiscoveredDevice? _selected;
  bool _scanning = false;
  late final Future<_Pfr1ViewData?> _pfr1Future;
  late int _selectedOrientation;
  _Pfr1ViewData? _pfr1Data;
  bool _orientationInitialized = false;

  static const int _manufacturerId = 0x1337;
  static const List<int> _manufacturerMagic = [0x50, 0x46, 0x52, 0x31]; // "PFR1"
  static const String _devicePrefix = 'ESP32-PhotoFrame';
  static const String _deviceFallbackPrefix = 'PhotoFrame-';

  @override
  void initState() {
    super.initState();
    _pfr1Future = _loadPfr1Data();
    _selectedOrientation = widget.job.isPortrait ? 1 : 0;
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
      body: FutureBuilder<_Pfr1ViewData?>(
        future: _pfr1Future,
        builder: (context, snapshot) {
          final data = snapshot.data ?? _pfr1Data;
          final header = data?.header;
          final previewImage = data?.image;

          return Column(
            children: [
              // Preview image at the top (decoded from .pfr1)
              if (previewImage != null)
                Container(
                  padding: const EdgeInsets.all(16),
                  child: ConstrainedBox(
                    constraints: BoxConstraints(maxHeight: MediaQuery.of(context).size.height * 0.25),
                    child: AspectRatio(
                      aspectRatio: _previewAspectRatio(),
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

              // Orientation selector
              Padding(padding: const EdgeInsets.symmetric(horizontal: 16), child: _buildOrientationSelector()),

              // Header info card (requires valid .pfr1)
              Padding(padding: const EdgeInsets.symmetric(horizontal: 16), child: _buildHeaderCard(data?.bytes)),

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
                        onPressed: (_selected == null || header == null) ? null : () => _connectAndUpload(_selected!),
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
      ),
    );
  }

  Widget _buildHeaderCard(Uint8List? data) {
    final header = data != null ? _parsePfr1Header(data) : null;

    return Card(
      child: ExpansionTile(
        initiallyExpanded: false,
        title: Row(
          children: [
            const Icon(Icons.description_outlined),
            const SizedBox(width: 8),
            Text('Header', style: Theme.of(context).textTheme.titleMedium),
          ],
        ),
        childrenPadding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
        children: [
          if (header == null)
            const Text('Header not available: provide a valid .pfr1 file.')
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
    );
  }

  Widget _buildOrientationSelector() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
        child: Row(
          children: [
            const Icon(Icons.screen_rotation),
            const SizedBox(width: 8),
            Text('Orientation', style: Theme.of(context).textTheme.titleMedium),
            const Spacer(),
            DropdownButton<int>(
              value: _selectedOrientation,
              onChanged: (value) {
                if (value == null) return;
                setState(() {
                  _selectedOrientation = value;
                });
              },
              items: const [
                DropdownMenuItem(value: 0, child: Text('0° (landscape)')),
                DropdownMenuItem(value: 1, child: Text('90° (portrait)')),
                DropdownMenuItem(value: 2, child: Text('180°')),
                DropdownMenuItem(value: 3, child: Text('270°')),
              ],
            ),
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

  Widget _headerRow(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 2),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(color: Colors.grey)),
          Text(value, style: const TextStyle(fontFeatures: [ui.FontFeature.tabularFigures()])),
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

  Future<_Pfr1ViewData?> _loadPfr1Data() async {
    try {
      final bytes = await widget.pfr1File.readAsBytes();
      final parsed = BinParser.parse(bytes);
      final rgba = BinParser.decodeToRgba(parsed);
      final image = await _rgbaToImage(rgba, parsed.header.width, parsed.header.height);
      final viewData = _Pfr1ViewData(header: parsed.header, bytes: bytes, image: image);
      _pfr1Data = viewData;
      if (mounted && !_orientationInitialized) {
        setState(() {
          _selectedOrientation = parsed.header.rotation % 4;
          _orientationInitialized = true;
        });
      }
      return viewData;
    } catch (e) {
      logger.warning('Failed to load .pfr1 file: $e');
      return null;
    }
  }

  Future<ui.Image?> _rgbaToImage(Uint8List rgba, int width, int height) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(rgba, width, height, ui.PixelFormat.rgba8888, (img) => completer.complete(img));
    return completer.future;
  }

  Widget _buildDeviceList() {
    if (_devices.isEmpty && !_scanning) {
      return ListView(
        padding: const EdgeInsets.all(24),
        children: const [
          SizedBox(height: 8),
          Icon(Icons.bluetooth_disabled, size: 64, color: Colors.grey),
          SizedBox(height: 16),
          Text(
            'No devices found',
            textAlign: TextAlign.center,
            style: TextStyle(fontSize: 18, fontWeight: FontWeight.w500),
          ),
          SizedBox(height: 8),
          Text(
            'Tap "Scan" to search for Photoframe devices',
            textAlign: TextAlign.center,
            style: TextStyle(color: Colors.grey),
          ),
        ],
      );
    }

    if (_scanning && _devices.isEmpty) {
      return ListView(
        padding: const EdgeInsets.all(24),
        children: const [
          SizedBox(height: 8),
          Center(child: CircularProgressIndicator()),
          SizedBox(height: 16),
          Center(child: Text('Searching for devices...')),
        ],
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
    final bleService = BleService(_ble);

    try {
      // Read binary data from .pfr1 file
      final binary = await widget.pfr1File.readAsBytes();
      if (binary.isEmpty) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Invalid .pfr1 file')));
        }
        return;
      }
      final Uint8List uploadData = binary;
      final header = _pfr1Data?.header ?? _parsePfr1Header(binary);
      if (header == null) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Unable to read PFR1 header')));
        }
        return;
      }

      // Show progress dialog
      if (!mounted) return;
      showDialog(
        context: context,
        barrierDismissible: false,
        builder: (context) => _UploadProgressDialog(
          bleService: bleService,
          device: device,
          imageData: uploadData,
          job: widget.job,
          pfr1Header: header,
          orientation: _selectedOrientation,
        ),
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

class _Pfr1ViewData {
  final BinHeader header;
  final Uint8List bytes;
  final ui.Image? image;

  const _Pfr1ViewData({required this.header, required this.bytes, required this.image});
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

class _UploadProgressDialog extends StatefulWidget {
  final BleService bleService;
  final DiscoveredDevice device;
  final Uint8List imageData;
  final ProcessingJob job;
  final BinHeader pfr1Header;
  final int orientation;

  const _UploadProgressDialog({
    required this.bleService,
    required this.device,
    required this.imageData,
    required this.job,
    required this.pfr1Header,
    required this.orientation,
  });

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

      // Upload
      if (mounted) setState(() => _status = 'Uploading...');
      await widget.bleService.uploadImage(
        imageData: widget.imageData,
        width: widget.pfr1Header.width,
        height: widget.pfr1Header.height,
        orientation: widget.orientation,
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
            Text('Image sent!', style: Theme.of(context).textTheme.titleLarge, textAlign: TextAlign.center),
          ],
        ),
        actions: [
          FilledButton(
            onPressed: () {
              Navigator.of(context).pop(); // Close dialog
              Navigator.of(context).popUntil((route) => route.isFirst); // Go to home
            },
            child: const Text('OK'),
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
        actions: [FilledButton(onPressed: () => Navigator.of(context).pop(), child: const Text('Close'))],
      );
    }

    final statusText = _progress > 0 ? '$_status (${(_progress * 100).toStringAsFixed(0)}%)' : _status;

    return AlertDialog(
      content: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          CircularProgressIndicator(value: null, constraints: BoxConstraints.tightFor(width: 36, height: 36)),
          const SizedBox(width: 16),
          Text(statusText, style: Theme.of(context).textTheme.bodyLarge),
        ],
      ),
      actions: [FilledButton(onPressed: _cancelled ? null : _cancel, child: const Text('Cancel'))],
    );
  }
}
