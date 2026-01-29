import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';

import '../utils/app_logger.dart';

class BleDeviceInfo {
  final int version;
  final int displayType;
  final int width;
  final int height;
  final int rotation;
  final int mtu;

  const BleDeviceInfo({
    required this.version,
    required this.displayType,
    required this.width,
    required this.height,
    required this.rotation,
    required this.mtu,
  });

  String get displayName => displayType == 1 ? '6-color' : 'B/W';
}

class BleService {
  static final _serviceUuid = Uuid.parse('0000180a-0000-1000-8000-00805f9b34fb');
  static final _configCharUuid = Uuid.parse('00002a29-0000-1000-8000-00805f9b34fb');
  static final _imageCharUuid = Uuid.parse('00002a2a-0000-1000-8000-00805f9b34fb');
  static final _deviceInfoCharUuid = Uuid.parse('00002a2c-0000-1000-8000-00805f9b34fb');

  final FlutterReactiveBle _ble;
  StreamSubscription<ConnectionStateUpdate>? _connectionSub;
  String? _connectedDeviceId;
  int? _negotiatedMtu;

  BleService(this._ble);

  Future<void> connect(String deviceId) async {
    logger.info('Connecting to device: $deviceId');
    final completer = Completer<void>();

    _connectionSub = _ble
        .connectToDevice(id: deviceId, connectionTimeout: const Duration(seconds: 30))
        .listen(
          (state) {
            logger.fine('Connection state: ${state.connectionState}');
            if (state.connectionState == DeviceConnectionState.connected) {
              _connectedDeviceId = deviceId;
              if (!completer.isCompleted) {
                completer.complete();
              }
            } else if (state.connectionState == DeviceConnectionState.disconnected) {
              if (!completer.isCompleted) {
                completer.completeError('Device disconnected');
              }
            }
          },
          onError: (e) {
            logger.severe('Connection error: $e');
            if (!completer.isCompleted) {
              completer.completeError(e);
            }
          },
        );

    await completer.future.timeout(const Duration(seconds: 35), onTimeout: () => throw Exception('Connection timeout'));

    // Wait for service discovery to complete
    logger.info('Waiting for service discovery...');
    await Future.delayed(const Duration(milliseconds: 1500));

    // Request the highest MTU the client supports to reduce chunk count
    // Note: MTU request can cause disconnection on some devices, handle gracefully
    try {
      final mtu = await _ble.requestMtu(deviceId: deviceId, mtu: 247);
      _negotiatedMtu = mtu;
      logger.info('Negotiated MTU: $mtu');
      // Wait a bit after MTU change
      await Future.delayed(const Duration(milliseconds: 500));
    } catch (e) {
      logger.warning('MTU request failed, using default payload sizing. Error: $e');
      _negotiatedMtu = null;
    }

    // Verify still connected after MTU request
    if (_connectedDeviceId == null) {
      throw Exception('Device disconnected after MTU negotiation');
    }
  }

  Future<void> disconnect() async {
    logger.info('Disconnecting from device...');
    await _connectionSub?.cancel();
    _connectionSub = null;
    _connectedDeviceId = null;
    _negotiatedMtu = null;
  }

  void dispose() {
    logger.fine('Disposing BLE service');
    _connectionSub?.cancel();
    _connectionSub = null;
    _connectedDeviceId = null;
    _negotiatedMtu = null;
  }

  Future<BleDeviceInfo> readDeviceInfo() async {
    if (_connectedDeviceId == null) {
      throw Exception('Not connected to device');
    }

    logger.info('📖 Reading device configuration...');

    final characteristic = QualifiedCharacteristic(serviceId: _serviceUuid, characteristicId: _deviceInfoCharUuid, deviceId: _connectedDeviceId!);

    try {
      // Subscribe to notifications first (ESP32 sends config via notify on connect)
      final completer = Completer<List<int>>();
      late final StreamSubscription<List<int>> notifySub;

      notifySub = _ble
          .subscribeToCharacteristic(characteristic)
          .listen(
            (data) {
              if (!completer.isCompleted) {
                logger.fine('Received device config via notification: ${data.length} bytes');
                completer.complete(data);
              }
              notifySub.cancel();
            },
            onError: (e) {
              if (!completer.isCompleted) {
                logger.warning('Notification error, falling back to read: $e');
                completer.completeError(e);
              }
            },
          );

      // Wait for notification or timeout and fallback to read
      List<int> data;
      try {
        data = await completer.future.timeout(
          const Duration(seconds: 2),
          onTimeout: () async {
            logger.info('No notification received, trying direct read...');
            notifySub.cancel();
            return await _ble.readCharacteristic(characteristic);
          },
        );
      } catch (e) {
        logger.warning('Notification failed, trying direct read: $e');
        notifySub.cancel();
        data = await _ble.readCharacteristic(characteristic);
      }

      if (data.length < 9) {
        logger.severe('Invalid device info length: ${data.length}');
        throw Exception('Device info too short (${data.length})');
      }

      final version = data[0];
      final displayType = data[1];
      final width = data[2] | (data[3] << 8);
      final height = data[4] | (data[5] << 8);
      final rotation = data[6];
      final mtu = data[7] | (data[8] << 8);

      logger.info('✅ Device config: version=$version, type=$displayType, ${width}x$height, rotation=$rotation, mtu=$mtu');

      final info = BleDeviceInfo(version: version, displayType: displayType, width: width, height: height, rotation: rotation, mtu: mtu);

      logger.info('✓ Device Configuration:');
      logger.info('  Version: $version');
      logger.info('  Display: ${info.displayName} (${width}x$height)');
      logger.info('  Rotation: $rotation');
      logger.info('  MTU Size: $mtu bytes');

      return info;
    } catch (e) {
      logger.severe('Failed to read device info: $e');
      rethrow;
    }
  }

  Future<void> uploadImage({
    required Uint8List imageData,
    required int width,
    required int height,
    required int orientation,
    required BleDeviceInfo deviceInfo,
    void Function(double progress)? onProgress,
  }) async {
    if (_connectedDeviceId == null) {
      throw Exception('Not connected to device');
    }

    try {
      logger.info('📤 Starting image upload...');
      logger.info('  Size: ${imageData.length} bytes (${width}x$height)');
      logger.info('  Orientation: $orientation');

      // Build config packet
      final config = _buildConfig(imageData, width, height, orientation);

      // Write config
      final configChar = QualifiedCharacteristic(serviceId: _serviceUuid, characteristicId: _configCharUuid, deviceId: _connectedDeviceId!);

      logger.fine('Sending configuration packet (${config.length} bytes)...');
      final configStopwatch = Stopwatch()..start();
      try {
        await _ble.writeCharacteristicWithResponse(configChar, value: config);
        configStopwatch.stop();
        logger.fine('✓ Configuration sent in ${configStopwatch.elapsedMilliseconds}ms');
      } catch (e) {
        logger.severe('Failed to send configuration: $e');
        rethrow;
      }

      // Use chunk size based on negotiated MTU with conservative overhead
      // BLE GATT has 3 bytes ATT header + variable handle overhead (~5-7 bytes total)
      // Use safe margin of 20 bytes to account for all BLE/GATT overhead
      final negotiatedMtu = _negotiatedMtu ?? 247;
      final safePayload = negotiatedMtu - 20;
      // Also respect standard BLE max write size (244 bytes is common safe limit)
      final chunk = safePayload.clamp(20, 244);

      logger.info('📊 Upload Configuration:');
      logger.info('  Chunk size: $chunk bytes');
      logger.info('  Total chunks: ${(imageData.length + chunk - 1) ~/ chunk}');
      logger.info('  Negotiated MTU: ${_negotiatedMtu ?? deviceInfo.mtu}');
      logger.info('  Est. time: ~${(imageData.length ~/ chunk * 2)}ms (2ms per chunk)');

      // Write image data in chunks using writeCharacteristicWithoutResponse for speed
      final imageChar = QualifiedCharacteristic(serviceId: _serviceUuid, characteristicId: _imageCharUuid, deviceId: _connectedDeviceId!);

      final totalChunks = (imageData.length + chunk - 1) ~/ chunk;
      int sent = 0;
      final uploadStopwatch = Stopwatch()..start();

      for (int i = 0; i < imageData.length; i += chunk) {
        final end = (i + chunk < imageData.length) ? i + chunk : imageData.length;
        final slice = imageData.sublist(i, end);
        final isLast = end >= imageData.length;

        // Use withoutResponse for speed, except last chunk which needs confirmation
        try {
          if (isLast) {
            await _ble.writeCharacteristicWithResponse(imageChar, value: slice);
          } else {
            await _ble.writeCharacteristicWithResponse(imageChar, value: slice);
            // await Future.delayed(const Duration(milliseconds: 10));
          }
        } catch (e) {
          logger.severe('Failed to send chunk ${sent + 1}/$totalChunks: $e');
          rethrow;
        }

        sent++;
        final progress = sent / totalChunks;
        onProgress?.call(progress);

        if (sent % 20 == 0 || sent == totalChunks) {
          logger.fine('⬆ Progress: $sent/$totalChunks chunks (${(progress * 100).toStringAsFixed(1)}%)');
          // Extra breather every 20 chunks to help BTC task
          if (sent < totalChunks) {
            await Future.delayed(const Duration(milliseconds: 40));
          }
        }
      }

      uploadStopwatch.stop();
      final elapsedMs = uploadStopwatch.elapsedMilliseconds;
      final speedKbps = (imageData.length / elapsedMs).toStringAsFixed(2);
      logger.info('✓ Upload complete in ${elapsedMs}ms (~$speedKbps KB/s)');
    } catch (e) {
      logger.severe('Upload failed: $e');
      rethrow;
    }
  }

  List<int> _buildConfig(Uint8List data, int width, int height, int rotation) {
    const magic = 0xBEEF;
    const version = 0x01;
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
