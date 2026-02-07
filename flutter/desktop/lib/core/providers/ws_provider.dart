import 'dart:async';
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:path/path.dart' as p;
import '../services/bin_parser.dart';

class WsUploadState with ChangeNotifier {
  static const String _defaultIp = '192.168.4.1';
  static const String _defaultPort = '8080';

  bool connecting = false;
  bool connected = false;
  bool uploading = false;
  double progress = 0;
  String status = '';
  String? error;
  int uploadDuration = 0; // Duration in seconds

  String ipAddress = _defaultIp;
  String port = _defaultPort;

  String? binPath;
  ui.Image? previewImage;
  BinHeader? binHeader;
  int rotation = 0;

  bool get canConnect => ipAddress.isNotEmpty && port.isNotEmpty && !connecting && !connected;
  bool get canUpload => connected && binPath != null && !uploading;

  void setIpAddress(String value) {
    ipAddress = value;
    notifyListeners();
  }

  void setPort(String value) {
    port = value;
    notifyListeners();
  }

  void setRotation(int value) {
    rotation = value;
    notifyListeners();
  }

  Future<void> connect() async {
    if (!canConnect) return;

    connecting = true;
    error = null;
    status = 'Connecting to $ipAddress:$port...';
    notifyListeners();

    try {
      // TODO: Implementare la connessione WebSocket
      await Future.delayed(const Duration(seconds: 1)); // Placeholder

      connected = true;
      status = 'Connected';
      debugPrint('✓ Connected to WebSocket at $ipAddress:$port');
    } catch (e) {
      error = e.toString();
      status = 'Connection failed';
      debugPrint('✗ Connection error: $e');
    } finally {
      connecting = false;
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    if (!connected) return;

    try {
      // TODO: Implementare la disconnessione WebSocket
      connected = false;
      status = 'Disconnected';
      notifyListeners();
      debugPrint('✓ Disconnected from WebSocket');
    } catch (e) {
      debugPrint('✗ Disconnect error: $e');
    }
  }

  Future<void> selectBinFile(String? path) async {
    if (path == null) return;

    try {
      binPath = path;
      status = 'Loading file...';
      error = null;
      notifyListeners();

      final bytes = await File(path).readAsBytes();
      final parsed = BinParser.parse(bytes);
      binHeader = parsed.header;

      final rgba = BinParser.decodeToRgba(parsed);
      previewImage = await _rgbaToImage(rgba, parsed.header.width, parsed.header.height);
      rotation = parsed.header.rotation;

      status = 'File loaded: ${p.basename(path)}';
      notifyListeners();
    } catch (e) {
      error = 'Failed to load file: $e';
      binPath = null;
      binHeader = null;
      previewImage = null;
      status = '';
      notifyListeners();
    }
  }

  Future<ui.Image> _rgbaToImage(Uint8List rgba, int width, int height) {
    final completer = Completer<ui.Image>();
    ui.decodeImageFromPixels(rgba, width, height, ui.PixelFormat.rgba8888, (img) => completer.complete(img));
    return completer.future;
  }

  Future<void> upload() async {
    if (!canUpload) return;

    uploading = true;
    progress = 0;
    error = null;
    status = 'Uploading...';
    notifyListeners();

    try {
      // TODO: Implementare l'upload via WebSocket
      await Future.delayed(const Duration(seconds: 2)); // Placeholder

      progress = 1.0;
      status = 'Upload completed';
      debugPrint('✓ Upload completed');
    } catch (e) {
      error = e.toString();
      status = 'Upload failed';
      debugPrint('✗ Upload error: $e');
    } finally {
      uploading = false;
      notifyListeners();
    }
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}
