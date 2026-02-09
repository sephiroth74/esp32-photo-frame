import 'dart:async';
import 'dart:convert';

import 'package:web_socket_channel/io.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import '../models/ws_messages.dart';
import '../platform/network_binding_service.dart';
import '../utils/app_logger.dart';

/// Service for WebSocket connection to ESP32 device
/// Handles connection, GET_CONFIG request, and maintains connection state
/// Singleton pattern - available app-wide for image upload and other operations
class WsConnectionService {
  static final WsConnectionService _instance = WsConnectionService._internal();

  WebSocketChannel? _channel;
  StreamSubscription? _channelSubscription;
  Completer<BoardConfig>? _configCompleter;
  BoardConfig? _boardConfig;
  bool _isConfigReceived = false;

  WsConnectionService._internal();

  /// Get singleton instance
  factory WsConnectionService() {
    return _instance;
  }

  /// Check if truly connected (after receiving BoardConfig)
  bool get isConnected => _isConfigReceived && _channel != null;

  /// Get board configuration (only available after successful connection)
  BoardConfig? get boardConfig => _boardConfig;

  Future<BoardConfig> connect({required String host, required int port, Duration timeout = const Duration(seconds: 10)}) async {
    if (isConnected) {
      logger.info('Already connected, returning cached config');
      return _boardConfig!;
    }

    if (_channel != null && !_isConfigReceived) {
      throw Exception('Connection in progress');
    }

    try {
      // Force Android to use WiFi network for WebSocket connection
      // This is required on Android 12+ when WiFi has no internet
      logger.info('Binding to WiFi network before WebSocket connection...');
      await NetworkBindingService.bindToWifi();

      final uri = Uri.parse('ws://$host:$port');
      logger.info('Connecting to WebSocket: $uri with timeout ${timeout.inSeconds}s');
      logger.fine('TCP Connection: host=$host, port=$port');

      // Use IOWebSocketChannel for direct socket connection
      _channel = IOWebSocketChannel.connect(uri);
      logger.fine('✓ IOWebSocketChannel.connect() called');

      // Wait for TCP connection to be ready (with timeout)
      logger.fine('Waiting for TCP handshake...');
      try {
        await _channel!.ready.timeout(timeout);
        logger.info('✓ TCP connection ready');
      } on TimeoutException catch (_) {
        logger.severe('✗ TCP handshake timeout after ${timeout.inSeconds}s');
        await disconnect();
        throw TimeoutException('TCP handshake timed out', timeout);
      }

      // Register stream listener AFTER TCP is ready
      _channelSubscription = _channel!.stream.listen(
        _handleMessage,
        onError: (error) {
          logger.severe('✗ WebSocket stream error: $error');
          _isConfigReceived = false;
          disconnect();
        },
        onDone: () {
          logger.info('WebSocket stream closed');
          _isConfigReceived = false;
          disconnect();
        },
      );

      logger.fine('✓ Stream listener registered');

      // Send handshake - server will respond with BoardInfo
      logger.fine('Sending handshake...');
      await _sendHandshake();

      // Wait for board configuration response from server
      logger.fine('Waiting for board configuration...');
      final config = await _requestConfig(timeout);
      logger.info('✓ Connection successful: $config');
      return config;
    } catch (e) {
      logger.severe('✗ Failed to connect: $e');
      await disconnect();
      rethrow;
    }
  }

  Future<void> _sendHandshake() async {
    try {
      final handshake = {'type': 'handshake', 'clientVersion': '1.0.0', 'platform': 'flutter'};
      _channel!.sink.add(jsonEncode(handshake));
      logger.info('✓ Handshake sent');
    } catch (e) {
      logger.severe('Failed to send handshake: $e');
      rethrow;
    }
  }

  Future<BoardConfig> _requestConfig(Duration timeout) async {
    logger.fine('_requestConfig() called with timeout: ${timeout.inSeconds}s');

    if (_channel == null) {
      logger.severe('_channel is null in _requestConfig!');
      throw Exception('Not connected to WebSocket');
    }

    _configCompleter = Completer<BoardConfig>();

    try {
      // Server sends BoardInfo after receiving handshake
      final config = await _configCompleter!.future.timeout(timeout);
      _boardConfig = config;
      _isConfigReceived = true;

      logger.info('✓ Received board configuration: $config');
      return config;
    } on TimeoutException {
      await disconnect();
      throw TimeoutException('Timeout waiting for board configuration', timeout);
    } catch (e) {
      await disconnect();
      rethrow;
    }
  }

  void _handleMessage(dynamic message) {
    try {
      if (message is String) {
        final json = jsonDecode(message) as Map<String, dynamic>;
        logger.fine('Received message type: ${json['type']}');

        if(json['type'] == 'board_info') {
          final config = BoardConfig.fromJson(json);
          if (_configCompleter != null && !_configCompleter!.isCompleted) {
            _configCompleter!.complete(config);
          }
          return;
        }

        // Check for error messages
        if (json['type'] == 'error') {
          final error = WsErrorInfo.fromJson(json);
          logger.severe('Server error: ${error.message}');
          if (_configCompleter != null && !_configCompleter!.isCompleted) {
            _configCompleter!.completeError(Exception(error.message));
          }
          return;
        }
      } else if (message is List<int>) {
        logger.fine('Received binary message: ${message.length} bytes');
      }
    } catch (e) {
      logger.severe('Failed to parse message: $e');
    }
  }

  Future<void> disconnect() async {
    try {
      _isConfigReceived = false;
      await _channelSubscription?.cancel();
      _channelSubscription = null;

      // Clear network binding when disconnecting
      await NetworkBindingService.clearWifiBinding();

      await _channel?.sink.close();
      _channel = null;

      _configCompleter = null;

      logger.info('WebSocket disconnected');
    } catch (e) {
      logger.warning('Error during disconnect: $e');
    }
  }
}
