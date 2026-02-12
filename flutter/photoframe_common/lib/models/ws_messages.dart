/// WebSocket message models for device communication
library;

import 'package:photoframe_common/models/library_models.dart';

/// WebSocket message types
enum WsMessageType {
  // Client to server
  handshake('handshake'),
  init('init'),
  end('end'),

  // Server to client
  boardInfo('board_info'),
  ready('ready'),
  chunkAck('chunk_ack'),
  ack('ack'),
  finalResponse('final_response'),
  error('error');

  final String value;
  const WsMessageType(this.value);

  /// Parse message type from string
  static WsMessageType? fromString(String? value) {
    if (value == null) return null;
    for (final type in WsMessageType.values) {
      if (type.value == value) return type;
    }
    return null;
  }

  @override
  String toString() => value;
}

class BoardConfig {
  final String type;
  final String board;
  final int flashSize;
  final DisplayType displayType;
  final int displayWidth;
  final int displayHeight;
  final Orientation displayRotation;
  final String serverVersion;
  final int fileVersion;
  final int binaryFileSize;
  final int? batteryLevel;
  final int? batteryVoltageMv;

  const BoardConfig({
    required this.type,
    required this.board,
    required this.flashSize,
    required this.displayType,
    required this.displayWidth,
    required this.displayHeight,
    required this.displayRotation,
    required this.serverVersion,
    required this.fileVersion,
    required this.binaryFileSize,
    this.batteryLevel,
    this.batteryVoltageMv,
  });

  /// Check if a JSON map looks like a BoardConfig
  static bool looksLikeConfig(Map<String, dynamic> json) {
    return json.containsKey('type') &&
        json.containsKey('board') &&
        json.containsKey('flash_size') &&
        json.containsKey('display_width') &&
        json.containsKey('display_height') &&
        json.containsKey('server_version');
  }

  /// Parse from JSON response
  factory BoardConfig.fromJson(Map<String, dynamic> json) {
    return BoardConfig(
      type: json['type'] as String,
      board: json['board'] as String,
      flashSize: (json['flash_size'] as num).toInt(),
      displayType: displayTypeFromString(json['display_type']),
      displayWidth: (json['display_width'] as num).toInt(),
      displayHeight: (json['display_height'] as num).toInt(),
      displayRotation: orientationFromInt((json['display_rotation'] as num).toInt()),
      serverVersion: json['server_version'] as String,
      fileVersion: (json['file_version'] as num).toInt(),
      binaryFileSize: (json['binary_file_size'] as num).toInt(),
      batteryLevel: (json['battery_level'] as num?)?.toInt(),
      batteryVoltageMv: (json['battery_voltage_mv'] as num?)?.toInt(),
    );
  }

  @override
  String toString() => 'BoardConfig(board=$board, display=${displayWidth}x$displayHeight, version=$serverVersion)';
}

class WsErrorInfo {
  final String type;
  final int code;
  final String message;

  const WsErrorInfo({this.type = 'error', this.code = 0, this.message = ''});

  factory WsErrorInfo.fromJson(Map<String, dynamic> json) {
    return WsErrorInfo(
      type: json['type'] as String? ?? 'error',
      code: (json['code'] as num?)?.toInt() ?? 0,
      message: json['message'] as String? ?? '',
    );
  }

  @override
  String toString() => 'WsErrorInfo(code=$code, message=$message)';
}

class WsChunkAck {
  final String type;
  final int received;

  const WsChunkAck({this.type = 'chunk_ack', this.received = 0});

  factory WsChunkAck.fromJson(Map<String, dynamic> json) {
    return WsChunkAck(type: json['type'] as String? ?? 'chunk_ack', received: (json['received'] as num?)?.toInt() ?? 0);
  }

  @override
  String toString() => 'WsChunkAck(received=$received)';
}

class WsReadyInfo {
  final String type;
  final int sessionId;

  const WsReadyInfo({this.type = 'ready', this.sessionId = 0});

  factory WsReadyInfo.fromJson(Map<String, dynamic> json) {
    return WsReadyInfo(type: json['type'] as String? ?? 'ready', sessionId: (json['session_id'] as num?)?.toInt() ?? 0);
  }

  @override
  String toString() => 'WsReadyInfo(sessionId=$sessionId)';
}

class WsFinalResponse {
  final String type;
  final bool success;
  final String message;
  final String? filepath;

  const WsFinalResponse({this.type = 'final_response', required this.success, this.message = '', this.filepath});

  factory WsFinalResponse.fromJson(Map<String, dynamic> json) {
    return WsFinalResponse(
      type: json['type'] as String? ?? 'final_response',
      success: json['success'] as bool? ?? false,
      message: json['message'] as String? ?? '',
      filepath: json['filepath'] as String?,
    );
  }

  @override
  String toString() => 'WsFinalResponse(success=$success, message=$message, filepath=$filepath)';
}

class WsMessageInfo {
  final String type;
  final String message;

  const WsMessageInfo({this.type = '', this.message = ''});

  factory WsMessageInfo.fromJson(Map<String, dynamic> json) {
    return WsMessageInfo(type: json['type'] as String? ?? '', message: json['message'] as String? ?? '');
  }

  @override
  String toString() => 'WsMessageInfo(type=$type, message=$message)';
}
