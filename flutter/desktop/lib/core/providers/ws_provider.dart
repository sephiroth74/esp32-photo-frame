import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/foundation.dart';
import 'package:path/path.dart' as p;
import 'package:photoframe_common/models/bin_model.dart';
import 'package:photoframe_common/models/ws_messages.dart'; // Import for WsDisplayReadyMessage
import 'package:photoframe_common/photoframe_common.dart';
import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

class WsUploadState with ChangeNotifier {
  Completer<WsDisplayReadyMessage>? _displayReadyCompleter;
  static const String _defaultIp = '192.168.4.1';
  static const String _defaultPort = '81';
  static const int _chunkSize = 4096;

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

  BoardConfig? boardConfig;

  WebSocketChannel? _channel;
  StreamSubscription? _channelSub;
  Completer<BoardConfig>? _configCompleter;
  Completer<WsReadyInfo>? _uploadReadyCompleter;
  Completer<WsChunkAck>? _uploadChunkAckCompleter;
  Completer<WsFinalResponse>? _uploadCompleteCompleter;
  Completer<WsMessageInfo>? _shutdownCompleter;

  bool get canConnect =>
      ipAddress.isNotEmpty && port.isNotEmpty && !connecting && !connected;
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
      final uri = Uri.parse('ws://$ipAddress:$port');
      _channel = IOWebSocketChannel.connect(uri);
      _channelSub = _channel!.stream.listen(
        _handleMessage,
        onError: (err) {
          error = err.toString();
          status = 'Connection error';
          connected = false;
          notifyListeners();
        },
        onDone: () {
          connected = false;
          status = 'Disconnected';
          notifyListeners();
        },
      );

      status = 'Connected. Requesting config...';
      notifyListeners();
      debugPrint('✓ WebSocket channel opened at $ipAddress:$port');

      await _requestConfig();
    } catch (e) {
      error = e.toString();
      status = 'Connection failed';
      connected = false;
      debugPrint('✗ Connection error: $e');
    } finally {
      connecting = false;
      notifyListeners();
    }
  }

  Future<void> disconnect() async {
    if (!connected) return;

    connected = false;
    error = null;
    status = 'Disconnecting...';
    notifyListeners();

    try {
      // Cancella il listener del stream prima di chiudere il canale
      await _channelSub?.cancel();
      _channelSub = null;

      // Chiudi il canale
      await _channel?.sink.close();
      _channel = null;
      boardConfig = null;
      status = 'Disconnected';
      debugPrint('✓ Disconnected from WebSocket');
    } catch (e) {
      status = 'Disconnect error: $e';
      error = e.toString();
      debugPrint('✗ Disconnect error: $e');
    } finally {
      notifyListeners();
    }
  }

  Future<void> _requestConfig() async {
    if (_channel == null) return;

    _configCompleter = Completer<BoardConfig>();
    _channel!.sink.add('GET_CONFIG');

    try {
      final config = await _configCompleter!.future.timeout(
        const Duration(seconds: 5),
      );
      boardConfig = config;
      connected = true;
      status = 'Connected - Configuration received';
      debugPrint('✓ Successfully connected with BoardConfig');
      notifyListeners();
    } on TimeoutException {
      status = 'Config request timed out';
      connected = false;
      error = 'Timeout waiting for board configuration';
      debugPrint('✗ Config request timeout');
      notifyListeners();
      // Disconnetti al timeout
      await disconnect();
    } catch (e) {
      status = 'Config request failed';
      connected = false;
      error = 'Failed to get board config: $e';
      debugPrint('✗ Config request error: $e');
      notifyListeners();
      // Disconnetti su errore
      await disconnect();
    }
  }

  void _handleMessage(dynamic message) {
    if (message is List<int>) {
      _handleTextMessage(utf8.decode(message));
      return;
    }

    if (message is String) {
      _handleTextMessage(message);
      return;
    }
  }

  void _handleTextMessage(String message) {
    try {
      final decoded = jsonDecode(message);
      if (decoded is Map<String, dynamic>) {
        // Handle error messages
        if (decoded['type'] == WsMessageType.error.value) {
          final err = WsErrorInfo.fromJson(decoded);
          error = err.message.isNotEmpty ? err.message : 'Unknown error';
          status = 'Server error';

          if (_configCompleter != null && !_configCompleter!.isCompleted) {
            _configCompleter!.completeError(Exception(error));
          }
          if (_uploadReadyCompleter != null &&
              !_uploadReadyCompleter!.isCompleted) {
            _uploadReadyCompleter!.completeError(Exception(error));
          }
          if (_uploadChunkAckCompleter != null &&
              !_uploadChunkAckCompleter!.isCompleted) {
            _uploadChunkAckCompleter!.completeError(Exception(error));
          }
          if (_uploadCompleteCompleter != null &&
              !_uploadCompleteCompleter!.isCompleted) {
            _uploadCompleteCompleter!.completeError(Exception(error));
          }
          if (_shutdownCompleter != null && !_shutdownCompleter!.isCompleted) {
            _shutdownCompleter!.completeError(Exception(error));
          }

          notifyListeners();
          return;
        }

        // Handle upload ready response
        if (decoded['type'] == WsMessageType.ready.value) {
          final ready = WsReadyInfo.fromJson(decoded);
          if (_uploadReadyCompleter != null &&
              !_uploadReadyCompleter!.isCompleted) {
            _uploadReadyCompleter!.complete(ready);
          }
          return;
        }

        // Handle upload chunk ACK
        if (decoded['type'] == WsMessageType.chunkAck.value) {
          final ack = WsChunkAck.fromJson(decoded);
          if (_uploadChunkAckCompleter != null &&
              !_uploadChunkAckCompleter!.isCompleted) {
            _uploadChunkAckCompleter!.complete(ack);
          }
          return;
        }

        // Handle upload success
        if (decoded['type'] == WsMessageType.finalResponse.value) {
          final success = WsFinalResponse.fromJson(decoded);
          if (_uploadCompleteCompleter != null &&
              !_uploadCompleteCompleter!.isCompleted) {
            _uploadCompleteCompleter!.complete(success);
          }
          // Dopo finalResponse, attendi display_ready
          _displayReadyCompleter = Completer<WsDisplayReadyMessage>();
          return;
        }

        // Handle display_ready
        if (decoded['type'] == WsMessageType.displayReady.value) {
          final displayReady = WsDisplayReadyMessage.fromJson(decoded);
          if (_displayReadyCompleter != null &&
              !_displayReadyCompleter!.isCompleted) {
            _displayReadyCompleter!.complete(displayReady);
          }
          return;
        }

        if (decoded['type'] == WsMessageType.ack.value) {
          final ack = WsMessageInfo.fromJson(decoded);
          if (_shutdownCompleter != null && !_shutdownCompleter!.isCompleted) {
            _shutdownCompleter!.complete(ack);
          }
          return;
        }

        // Handle board configuration
        if (decoded['type'] == WsMessageType.boardInfo.value &&
            BoardConfig.looksLikeConfig(decoded)) {
          final config = BoardConfig.fromJson(decoded);
          if (_configCompleter != null && !_configCompleter!.isCompleted) {
            _configCompleter!.complete(config);
          } else {
            boardConfig = config;
            notifyListeners();
          }
        }
      }
    } catch (_) {
      // Ignore non-JSON messages.
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

      final rgba = parsed.decodeToRgba();
      previewImage = await _rgbaToImage(
        rgba,
        parsed.header.width,
        parsed.header.height,
      );
      rotation = parsed.header.orientation.value;

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
    ui.decodeImageFromPixels(
      rgba,
      width,
      height,
      ui.PixelFormat.rgba8888,
      (img) => completer.complete(img),
    );
    return completer.future;
  }

  /// Valida che le dimensioni dell'immagine corrispondano al display del board
  bool validateImageDimensions() {
    if (boardConfig == null) {
      error = 'Board configuration not available';
      return false;
    }

    if (binHeader == null) {
      error = 'Image not loaded';
      return false;
    }

    final imageWidth = binHeader!.width;
    final imageHeight = binHeader!.height;
    final displayWidth = boardConfig!.displayWidth;
    final displayHeight = boardConfig!.displayHeight;

    if ((imageWidth == displayWidth && imageHeight == displayHeight) ||
        (imageWidth == displayHeight && imageHeight == displayWidth)) {
      return true; // Dimensions match exactly
    } else {
      error =
          'Image dimensions ($imageWidth×$imageHeight) do not match '
          'display dimensions ($displayWidth×$displayHeight) even after considering rotation';
      return false;
    }
  }

  Future<void> upload() async {
    if (!canUpload) return;

    // Validate image dimensions before uploading
    if (!validateImageDimensions()) {
      status = 'Validation failed';
      notifyListeners();
      return;
    }

    uploading = true;
    progress = 0;
    error = null;
    status = 'Uploading...';
    notifyListeners();

    final stopwatch = Stopwatch()..start();

    try {
      if (_channel == null) {
        throw Exception('Not connected to WebSocket');
      }

      // Read the binary file
      final fileBytes = await File(binPath!).readAsBytes();
      final fileName = p.basename(binPath!);

      debugPrint('✓ File loaded: $fileName (${fileBytes.length} bytes)');

      // Prepare upload parameters
      final timestamp = DateTime.now().millisecondsSinceEpoch ~/ 1000;
      final token = timestamp.toRadixString(16).padLeft(8, '0');

      // Send upload init message
      final initMessage = {
        'type': 'init',
        'filename': fileName,
        'token': token,
        'timestamp': timestamp,
        'orientation': rotation,
      };

      status = 'Sending upload init...';
      notifyListeners();
      _channel!.sink.add(jsonEncode(initMessage));
      debugPrint('✓ Sent upload init: $fileName, orientation=$rotation');

      // Wait for server ready response
      _uploadReadyCompleter = Completer<WsReadyInfo>();
      final ready = await _uploadReadyCompleter!.future.timeout(
        const Duration(seconds: 5),
        onTimeout: () =>
            throw TimeoutException('Server did not respond with ready'),
      );
      debugPrint('✓ Server ready with session_id=${ready.sessionId}');

      // Send binary chunks
      const chunkSize = _chunkSize;
      int totalChunksSent = 0;

      for (int i = 0; i < fileBytes.length; i += chunkSize) {
        final endIndex = (i + chunkSize < fileBytes.length)
            ? i + chunkSize
            : fileBytes.length;
        final chunk = fileBytes.sublist(i, endIndex);

        // Send chunk as binary data
        _channel!.sink.add(chunk);
        totalChunksSent++;

        progress = (endIndex / fileBytes.length).clamp(0.0, 1.0);
        status = 'Uploading... ${(progress * 100).toStringAsFixed(1)}%';
        notifyListeners();

        // Wait for chunk ACK
        _uploadChunkAckCompleter = Completer<WsChunkAck>();
        final ack = await _uploadChunkAckCompleter!.future.timeout(
          const Duration(seconds: 5),
          onTimeout: () =>
              throw TimeoutException('No ACK for chunk $totalChunksSent'),
        );

        final ackProgress = (ack.received / fileBytes.length).clamp(0.0, 1.0);
        if (ackProgress > progress) {
          progress = ackProgress;
          status = 'Uploading... ${(progress * 100).toStringAsFixed(1)}%';
          notifyListeners();
        }

        debugPrint(
          '✓ Chunk $totalChunksSent sent: ${ack.received}/${fileBytes.length} bytes',
        );
      }

      // Send upload end message
      status = 'Finalizing upload...';
      notifyListeners();
      final endMessage = {'type': 'end'};
      _channel!.sink.add(jsonEncode(endMessage));
      debugPrint('✓ Sent upload end message');

      // Wait for final response
      _uploadCompleteCompleter = Completer<WsFinalResponse>();
      await _uploadCompleteCompleter!.future.timeout(
        const Duration(seconds: 5),
        onTimeout: () =>
            throw TimeoutException('Server did not send final response'),
      );

      // Mostra progress indeterminato in attesa di display_ready
      status = 'Updating display...';
      progress = 1.0;
      notifyListeners();

      // Attendi display_ready o timeout
      try {
        final displayReady = await _displayReadyCompleter!.future.timeout(
          const Duration(seconds: 30),
          onTimeout: () =>
              throw TimeoutException('Display not ready after 30s'),
        );
        status = 'Display updated: ${displayReady.message}';
        debugPrint('✓ Display ready: ${displayReady.message}');
      } catch (e) {
        status = 'Display update timeout';
        debugPrint('✗ Display ready timeout: $e');
      }

      uploadDuration = stopwatch.elapsed.inSeconds;
      debugPrint('✓ Upload completed in ${uploadDuration}s');
    } catch (e) {
      error = e.toString();
      status = 'Upload failed';
      debugPrint('✗ Upload error: $e');
    } finally {
      uploading = false;
      _uploadReadyCompleter = null;
      _uploadChunkAckCompleter = null;
      _uploadCompleteCompleter = null;
      _displayReadyCompleter = null;
      stopwatch.stop();
      notifyListeners();
    }
  }

  Future<void> shutdown() async {
    if (!connected) return;

    if (_channel == null) {
      error = 'Not connected to WebSocket';
      status = 'Shutdown failed';
      notifyListeners();
      return;
    }

    if (uploading) {
      error = 'Upload in progress';
      status = 'Shutdown failed';
      notifyListeners();
      return;
    }

    error = null;
    status = 'Sending shutdown...';
    notifyListeners();

    try {
      final shutdownMessage = {'type': 'shutdown'};
      _channel!.sink.add(jsonEncode(shutdownMessage));
      debugPrint('✓ Shutdown request sent');

      _shutdownCompleter = Completer<WsMessageInfo>();
      final ack = await _shutdownCompleter!.future.timeout(
        const Duration(seconds: 5),
        onTimeout: () =>
            throw TimeoutException('Server did not acknowledge shutdown'),
      );

      final ackMessage = ack.message.isNotEmpty
          ? ack.message
          : 'Device entering deep sleep';
      status = 'Shutdown accepted: $ackMessage';
      debugPrint('✓ Shutdown accepted: $ackMessage');

      await disconnect();
    } catch (e) {
      error = e.toString();
      status = 'Shutdown failed';
      debugPrint('✗ Shutdown error: $e');
      notifyListeners();
    } finally {
      _shutdownCompleter = null;
    }
  }

  @override
  void dispose() {
    disconnect();
    super.dispose();
  }
}
