import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:path/path.dart' as p;
import '../services/bin_parser.dart';

class BleDeviceInfo {
  final String id;
  final String name;
  final int rssi;
  final int mtu;
  final int width;
  final int height;
  final int displayType; // 0=bw, 1=6c
  final int rotation;
  const BleDeviceInfo({
    required this.id,
    required this.name,
    required this.rssi,
    required this.mtu,
    required this.width,
    required this.height,
    required this.displayType,
    required this.rotation,
  });
}

class BleUploadState with ChangeNotifier {
  static final _serviceUuid = Guid("0000180a-0000-1000-8000-00805f9b34fb");
  static final _configChar = Guid("00002a29-0000-1000-8000-00805f9b34fb");
  static final _imageChar = Guid("00002a2a-0000-1000-8000-00805f9b34fb");
  static final _deviceInfoChar = Guid("00002a2c-0000-1000-8000-00805f9b34fb");
  static const _defaultChunk = 512; // Conservative chunk size for iOS/macOS reliability
  static const _maxChunk = 512; // iOS/macOS limit for withResponse writes
  static const _manufacturerId = 0x1337;
  static const _manufacturerMagic = [0x50, 0x46, 0x52, 0x31]; // "PFR1"
  static const _devicePrefix = 'ESP32-PhotoFrame';
  static const _deviceFallbackPrefix = 'PhotoFrame-';

  bool scanning = false;
  bool connecting = false;
  bool uploading = false;
  double progress = 0;
  String status = '';
  String? error;

  List<ScanResult> devices = [];
  BluetoothDevice? connected;
  BleDeviceInfo? deviceInfo;

  String? binPath;
  ui.Image? previewImage;
  BinHeader? binHeader;
  int rotation = 0;

  StreamSubscription<List<ScanResult>>? _scanSub;

  bool get canUpload => connected != null && deviceInfo != null && binPath != null && !uploading;

  void setRotation(int value) {
    rotation = value;
    notifyListeners();
  }

  Future<bool> scan({Duration timeout = const Duration(seconds: 6)}) async {
    scanning = true;
    error = null;
    notifyListeners();
    devices = [];
    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      devices = results.where((r) => _isPhotoFrame(r)).toList();
      notifyListeners();
    });

    try {
      // Wait for Bluetooth to be ready (max 5 seconds)
      print('Waiting for Bluetooth to be ready...');
      var adapterState = await FlutterBluePlus.adapterState.first;
      if (adapterState != BluetoothAdapterState.on) {
        print('Bluetooth not ready, waiting...');
        adapterState = await FlutterBluePlus.adapterState
            .firstWhere((state) => state == BluetoothAdapterState.on)
            .timeout(
              const Duration(seconds: 5),
              onTimeout: () {
                throw Exception('Bluetooth non disponibile. Assicurati che il Bluetooth sia acceso.');
              },
            );
      }
      print('✓ Bluetooth ready, starting scan...');

      await FlutterBluePlus.startScan(timeout: timeout);
      await Future.delayed(timeout);
    } catch (e) {
      error = e.toString();
      print('❌ Scan error: $e');
      rethrow;
    } finally {
      await FlutterBluePlus.stopScan().catchError((_) {});
      scanning = false;
      notifyListeners();
    }
    return devices.isNotEmpty;
  }

  bool _isPhotoFrame(ScanResult result) {
    final adv = result.advertisementData;

    // Check manufacturer data for 0x1337 with "PFR1" magic
    if (adv.manufacturerData.isNotEmpty) {
      final mfgData = adv.manufacturerData[_manufacturerId];
      if (mfgData != null && mfgData.length >= 4) {
        if (mfgData[0] == _manufacturerMagic[0] &&
            mfgData[1] == _manufacturerMagic[1] &&
            mfgData[2] == _manufacturerMagic[2] &&
            mfgData[3] == _manufacturerMagic[3]) {
          return true;
        }
      }
    }

    // Fallback: check device name
    final name = result.device.platformName;
    if (name.startsWith(_devicePrefix) || name.startsWith(_deviceFallbackPrefix)) {
      return true;
    }

    return false;
  }

  Future<void> selectDevice(ScanResult result) async {
    error = null;
    connecting = true;
    notifyListeners();
    try {
      final d = result.device;
      await connected?.disconnect();
      await d.connect(timeout: const Duration(seconds: 8));
      connected = d;
      status = 'Connected to ${d.platformName.isNotEmpty ? d.platformName : d.remoteId.str}';
      await _discoverAndReadInfo();
    } catch (e) {
      error = 'Connection failed: $e';
    } finally {
      connecting = false;
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    if (connected != null) {
      await connected!.disconnect();
    }
    await _scanSub?.cancel();
    connected = null;
    deviceInfo = null;
    previewImage = null;
    binHeader = null;
    status = '';
    notifyListeners();
  }

  Future<void> pickBin(String path) async {
    binPath = path;
    await _loadPreview();
    notifyListeners();
  }

  Future<void> _loadPreview() async {
    if (binPath == null) return;
    try {
      final bytes = await File(binPath!).readAsBytes();
      ui.Image? img;
      BinHeader? header;
      // Try new PFR1 format first
      final parsed = BinParser.parse(bytes);
      final rgba = BinParser.decodeToRgba(parsed);
      img = await _rgbaToImage(rgba, parsed.header.width, parsed.header.height);
      header = parsed.header;
      previewImage = img;
      binHeader = header;
      status = 'Preview loaded (${p.basename(binPath!)})';
    } catch (e) {
      previewImage = null;
      binHeader = null;
      error = 'Preview failed: $e';
    }
  }

  Future<ui.Image> _rgbaToImage(Uint8List rgba, int width, int height) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(rgba, width, height, ui.PixelFormat.rgba8888, (img) => completer.complete(img));
    return completer.future;
  }

  Future<String?> upload() async {
    if (connected == null || deviceInfo == null || binPath == null) {
      error = 'Device or file not ready';
      notifyListeners();
      return error;
    }
    uploading = true;
    progress = 0;
    error = null;
    notifyListeners();
    try {
      final data = await File(binPath!).readAsBytes();
      final info = deviceInfo!;
      final config = _buildConfig(data, info.width, info.height, rotation);

      final services = await connected!.discoverServices();
      BluetoothCharacteristic? cfg;
      BluetoothCharacteristic? img;
      for (final s in services) {
        if (s.serviceUuid == _serviceUuid) {
          for (final c in s.characteristics) {
            if (c.characteristicUuid == _configChar) cfg = c;
            if (c.characteristicUuid == _imageChar) img = c;
          }
        }
      }
      if (cfg == null || img == null) {
        throw Exception('Config or image characteristic not found');
      }
      final BluetoothCharacteristic configChar = cfg;
      final BluetoothCharacteristic imageChar = img;

      await configChar.write(config, withoutResponse: false);

      // Determine chunk size from device MTU, limited to platform max
      // iOS/macOS limit withResponse writes to 512 bytes
      int chunk;
      if (info.mtu > 0 && info.mtu <= _maxChunk) {
        chunk = info.mtu.clamp(20, _maxChunk);
      } else {
        print('⚠ Invalid MTU size ${info.mtu}, using default $_defaultChunk');
        chunk = _defaultChunk;
      }

      // Additional safety: ensure we never exceed platform limits
      if (chunk > _maxChunk) {
        print('⚠ Limiting chunk from $chunk to $_maxChunk for platform compatibility');
        chunk = _maxChunk;
      }

      // Log upload configuration
      print('📤 Upload Configuration:');
      print('   Size: ${data.length} bytes');
      print('   Dimensions: ${info.width}x${info.height}');
      print('   Display: ${info.displayType == 1 ? '6-color' : 'B/W'}');
      print('   Rotation: $rotation');
      print('   Chunk size: $chunk bytes (from device MTU: ${info.mtu})');

      final totalChunks = (data.length + chunk - 1) ~/ chunk;
      int sent = 0;
      for (int i = 0; i < data.length; i += chunk) {
        final end = (i + chunk < data.length) ? i + chunk : data.length;
        final slice = data.sublist(i, end);
        await imageChar.write(slice, withoutResponse: false);
        sent++;
        progress = sent / totalChunks;
        notifyListeners();
      }
      print('✓ Upload complete');
      status = 'Upload complete';
    } catch (e) {
      error = 'Upload failed: $e';
      print('❌ BLE Upload Error: $e');
    } finally {
      uploading = false;
      notifyListeners();
    }
    return error;
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    super.dispose();
  }

  Future<void> _discoverAndReadInfo() async {
    deviceInfo = null;
    final services = await connected!.discoverServices();
    BluetoothCharacteristic? infoChar;
    for (final s in services) {
      if (s.serviceUuid == _serviceUuid) {
        for (final c in s.characteristics) {
          if (c.characteristicUuid == _deviceInfoChar) {
            infoChar = c;
            break;
          }
        }
      }
    }
    if (infoChar == null) {
      throw Exception('Device info characteristic not found');
    }
    final data = await infoChar.read();
    if (data.length < 9) {
      throw Exception('Device info too short (${data.length})');
    }
    final version = data[0];
    final displayType = data[1];
    final width = data[2] | (data[3] << 8);
    final height = data[4] | (data[5] << 8);
    final rotationValue = data[6];
    final mtu = data[7] | (data[8] << 8);

    final displayName = displayType == 1 ? '6-color' : 'B/W';
    print('📊 Device Info:');
    print('   Version: $version');
    print('   Display: $displayName (${width}x$height)');
    print('   Rotation: $rotationValue');
    print('   MTU Size: $mtu bytes');

    status = 'Device v$version $displayName ${width}x$height mtu=$mtu';
    deviceInfo = BleDeviceInfo(
      id: connected!.remoteId.str,
      name: connected!.platformName.isNotEmpty ? connected!.platformName : connected!.remoteId.str,
      rssi: 0,
      mtu: mtu,
      width: width,
      height: height,
      displayType: displayType,
      rotation: rotationValue,
    );
    rotation = rotationValue;
    try {
      await connected!.requestMtu(mtu);
    } catch (_) {
      // Some platforms may not support changing MTU; ignore failures
    }
  }

  List<int> _buildConfig(Uint8List data, int width, int height, int rotation) {
    final magic = 0xBEEF;
    final version = 0x01;
    final ts = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    final imageSize = data.length;
    final buf = BytesBuilder();
    buf.add(_le16(magic));
    buf.add([version]);
    buf.add([rotation]);
    buf.add(_le16(width));
    buf.add(_le16(height));
    buf.add(_le32(ts));
    buf.add(_le32(imageSize));
    final withoutCrc = buf.toBytes();
    final crc = _crc16(withoutCrc);
    buf.add(_le16(crc));
    return buf.toBytes();
  }

  List<int> _le16(int v) => [v & 0xFF, (v >> 8) & 0xFF];
  List<int> _le32(int v) => [v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, (v >> 24) & 0xFF];

  int _crc16(List<int> data) {
    int crc = 0xFFFF;
    for (final b in data) {
      crc ^= b & 0xFF;
      for (int i = 0; i < 8; i++) {
        if ((crc & 1) != 0) {
          crc = (crc >> 1) ^ 0xA001;
        } else {
          crc >>= 1;
        }
      }
    }
    return crc & 0xFFFF;
  }
}
