import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:photoframe_common/photoframe_common.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../utils/app_logger.dart';

// Export the enum so it can be used with prefix
export 'websocket_service.dart' show WsConnectionState;

enum WsConnectionState { disconnected, connecting, connected, error }

class WebSocketService {
  WebSocketChannel? _channel;
  WsConnectionState _state = WsConnectionState.disconnected;
  final _stateController = StreamController<WsConnectionState>.broadcast();
  final _messageController = StreamController<Map<String, dynamic>>.broadcast();
  StreamSubscription? _channelSubscription;
  BoardConfig? _deviceInfo;
  String? _lastError;
  bool _connectionInProgress = false; // Guard against multiple simultaneous connection attempts

  WsConnectionState get state => _state;
  Stream<WsConnectionState> get stateStream => _stateController.stream;
  Stream<Map<String, dynamic>> get messageStream => _messageController.stream;
  BoardConfig? get deviceInfo => _deviceInfo;
  String? get lastError => _lastError;

  Future<void> connect(String host, {int port = 8080, Duration timeout = const Duration(seconds: 10)}) async {
    if (_state == WsConnectionState.connected) {
      logger.warning('Already connected');
      return;
    }

    if (_connectionInProgress) {
      logger.warning('Connection already in progress');
      return;
    }

    _connectionInProgress = true;

    try {
      // Close any existing connection cleanly
      if (_channel != null || _channelSubscription != null) {
        logger.info('Closing existing connection before establishing new one');
        _channelSubscription?.cancel();
        _channelSubscription = null;
        _channel = null;
      }

      _updateState(WsConnectionState.connecting);
      _lastError = null;

      final uri = Uri.parse('ws://$host:$port');
      logger.info('Connecting to WebSocket: $uri');

      _channel = WebSocketChannel.connect(uri);

      // Wait for connection to be established
      logger.info('Waiting for WebSocket handshake...');
      try {
        await _channel!.ready.timeout(timeout);
      } on TimeoutException {
        throw TimeoutException('WebSocket handshake timed out', timeout);
      }

      logger.info('WebSocket connected successfully');

      // Listen to incoming messages (do this AFTER connection is established)
      _channelSubscription = _channel!.stream.listen(
        _handleMessage,
        onError: (error) {
          logger.severe('WebSocket error: $error');
          _lastError = error.toString();
          _updateState(WsConnectionState.error);
          disconnect();
        },
        onDone: () {
          logger.info('WebSocket connection closed');
          _updateState(WsConnectionState.disconnected);
          disconnect();
        },
      );

      _updateState(WsConnectionState.connected);

      // Send handshake
      await _sendHandshake();

      // Wait for device info
      try {
        await _waitForDeviceInfo(timeout);
      } on TimeoutException {
        throw TimeoutException('Timed out waiting for device_info', timeout);
      }
    } catch (e) {
      logger.severe('Failed to connect: $e');
      _lastError = e.toString();
      _updateState(WsConnectionState.error);
      disconnect();
      rethrow;
    } finally {
      _connectionInProgress = false;
    }
  }

  Future<void> _sendHandshake() async {
    final handshake = {'type': WsMessageType.handshake.value, 'clientVersion': '1.0.0', 'platform': 'flutter'};
    _sendJson(handshake);
    logger.info('Sent handshake');
  }

  Future<void> _waitForDeviceInfo(Duration timeout) async {
    final completer = Completer<void>();
    late StreamSubscription subscription;

    subscription = _messageController.stream.listen((message) {
      final messageType = WsMessageType.fromString(message['type'] as String?);
      if (messageType == WsMessageType.boardInfo) {
        try {
          _deviceInfo = BoardConfig.fromJson(message);
          logger.info('Received device info: ${_deviceInfo!.displayWidth}x${_deviceInfo!.displayHeight}, type=${_deviceInfo!.displayType}');
          if (!completer.isCompleted) {
            completer.complete();
          }
        } catch (e) {
          logger.severe('Failed to parse device info: $e');
          if (!completer.isCompleted) {
            completer.completeError('Invalid device info: $e');
          }
        }
        subscription.cancel();
      }
    });

    try {
      await completer.future.timeout(timeout);
    } catch (e) {
      subscription.cancel();
      rethrow;
    }
  }

  void _handleMessage(dynamic message) {
    try {
      if (message is String) {
        // JSON message
        final json = jsonDecode(message) as Map<String, dynamic>;
        logger.fine('Received message: ${json['type']}');
        _messageController.add(json);
      } else if (message is List<int>) {
        // Binary message (shouldn't receive these, but handle gracefully)
        logger.fine('Received binary message: ${message.length} bytes');
      } else {
        logger.warning('Received unknown message type: ${message.runtimeType}');
      }
    } catch (e) {
      logger.severe('Failed to handle message: $e');
    }
  }

  void _sendJson(Map<String, dynamic> data) {
    if (_channel == null) {
      throw Exception('Not connected');
    }
    final json = jsonEncode(data);
    _channel!.sink.add(json);
  }

  void _sendBinary(Uint8List data) {
    if (_channel == null) {
      throw Exception('Not connected');
    }
    _channel!.sink.add(data);
  }

  Future<void> uploadImage({
    required Uint8List imageData,
    required String filename,
    required int orientation, // 0, 1, 2, 3 for 0°, 90°, 180°, 270°
    required Function(double progress) onProgress,
  }) async {
    if (_state != WsConnectionState.connected) {
      throw Exception('Not connected');
    }

    if (_deviceInfo == null) {
      throw Exception('Device info not available');
    }

    logger.info('Starting image upload: ${imageData.length} bytes, filename=$filename, orientation=$orientation');

    // Generate token and timestamp
    final timestamp = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    final token = timestamp.toRadixString(16);

    // Send upload init message
    final initMessage = {'type': WsMessageType.init.value, 'filename': filename, 'token': token, 'timestamp': timestamp, 'orientation': orientation};
    _sendJson(initMessage);
    logger.info('Sent upload init');

    // Wait for "ready" response
    final readyCompleter = Completer<void>();
    late StreamSubscription subscription;

    subscription = _messageController.stream.listen((message) {
      final messageType = WsMessageType.fromString(message['type'] as String?);
      if (messageType == WsMessageType.ready) {
        logger.info('Server ready to receive image data');
        if (!readyCompleter.isCompleted) {
          readyCompleter.complete();
        }
        subscription.cancel();
      } else if (messageType == WsMessageType.error) {
        logger.severe('Server error during init: ${message['message']}');
        if (!readyCompleter.isCompleted) {
          readyCompleter.completeError(message['message'] ?? 'Server error');
        }
        subscription.cancel();
      }
    });

    try {
      await readyCompleter.future.timeout(const Duration(seconds: 5));
    } catch (e) {
      subscription.cancel();
      throw Exception('Server did not respond with ready: $e');
    }

    // Send image data in chunks
    const chunkSize = 4096; // 4KB chunks
    int sentBytes = 0;

    for (int offset = 0; offset < imageData.length; offset += chunkSize) {
      final end = (offset + chunkSize < imageData.length) ? offset + chunkSize : imageData.length;
      final chunk = imageData.sublist(offset, end);

      _sendBinary(chunk);

      // Wait for chunk ACK
      final ackCompleter = Completer<void>();
      late StreamSubscription ackSubscription;

      ackSubscription = _messageController.stream.listen((message) {
        final messageType = WsMessageType.fromString(message['type'] as String?);
        if (messageType == WsMessageType.chunkAck) {
          final received = message['received'] as int?;
          if (received != null) {
            onProgress(received / imageData.length);
            logger.fine('Received chunk ack: $received bytes');
          }
          if (!ackCompleter.isCompleted) {
            ackCompleter.complete();
          }
          ackSubscription.cancel();
        } else if (messageType == WsMessageType.error) {
          logger.severe('Server error during chunk transfer: ${message['message']}');
          if (!ackCompleter.isCompleted) {
            ackCompleter.completeError(message['message'] ?? 'Server error');
          }
          ackSubscription.cancel();
        }
      });

      try {
        await ackCompleter.future.timeout(const Duration(seconds: 10));
      } catch (e) {
        ackSubscription.cancel();
        throw Exception('Failed to receive chunk ACK: $e');
      }

      sentBytes += chunk.length;
      await Future.delayed(const Duration(milliseconds: 10));
    }

    logger.info('Sent all image data: $sentBytes bytes');

    // Send completion message
    final endMessage = {'type': WsMessageType.end.value};
    _sendJson(endMessage);
    logger.info('Sent upload completion message');

    // Wait for final response
    final finalCompleter = Completer<void>();
    late StreamSubscription finalSubscription;

    finalSubscription = _messageController.stream.listen((message) {
      final messageType = WsMessageType.fromString(message['type'] as String?);
      if (messageType == WsMessageType.finalResponse) {
        final success = message['success'] as bool? ?? false;
        if (success) {
          logger.info('Image upload successful');
          if (!finalCompleter.isCompleted) {
            finalCompleter.complete();
          }
        } else {
          final errorMsg = message['message'] ?? 'Upload failed';
          logger.severe('Upload failed: $errorMsg');
          if (!finalCompleter.isCompleted) {
            finalCompleter.completeError(errorMsg);
          }
        }
        finalSubscription.cancel();
      } else if (messageType == WsMessageType.error) {
        logger.severe('Upload failed: ${message['message']}');
        if (!finalCompleter.isCompleted) {
          finalCompleter.completeError(message['message'] ?? 'Upload failed');
        }
        finalSubscription.cancel();
      }
    });

    try {
      await finalCompleter.future.timeout(const Duration(seconds: 10));
    } catch (e) {
      finalSubscription.cancel();
      throw Exception('Upload completion failed: $e');
    }
  }

  void _updateState(WsConnectionState newState) {
    if (_state != newState) {
      _state = newState;
      _stateController.add(newState);
    }
  }

  void disconnect() {
    logger.info('Disconnecting WebSocket');
    _channelSubscription?.cancel();
    _channelSubscription = null;
    _channel?.sink.close();
    _channel = null;
    _deviceInfo = null;
    _updateState(WsConnectionState.disconnected);
  }

  void dispose() {
    disconnect();
    _stateController.close();
    _messageController.close();
  }
}
