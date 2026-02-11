/// WebSocket message models for device communication
library;

import 'package:photoframe_common/models/library_models.dart';

class BoardConfig {
  final String type;
  final String board;
  final String flashSize;
  final int flashSizeBytes;
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
    required this.flashSizeBytes,
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
      flashSize: json['flash_size'] as String,
      flashSizeBytes: (json['flash_size_bytes'] as num).toInt(),
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
  String toString() =>
      'BoardConfig(board=$board, display=${displayWidth}x$displayHeight, version=$serverVersion)';
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
    return WsChunkAck(
      type: json['type'] as String? ?? 'chunk_ack',
      received: (json['received'] as num?)?.toInt() ?? 0,
    );
  }

  @override
  String toString() => 'WsChunkAck(received=$received)';
}

class WsReadyInfo {
  final String type;
  final int sessionId;

  const WsReadyInfo({this.type = 'ready', this.sessionId = 0});

  factory WsReadyInfo.fromJson(Map<String, dynamic> json) {
    return WsReadyInfo(
      type: json['type'] as String? ?? 'ready',
      sessionId: (json['session_id'] as num?)?.toInt() ?? 0,
    );
  }

  @override
  String toString() => 'WsReadyInfo(sessionId=$sessionId)';
}

class WsSuccessInfo {
  final String type;
  final String message;

  const WsSuccessInfo({this.type = 'success', this.message = ''});

  factory WsSuccessInfo.fromJson(Map<String, dynamic> json) {
    return WsSuccessInfo(
      type: json['type'] as String? ?? 'success',
      message: json['message'] as String? ?? '',
    );
  }

  @override
  String toString() => 'WsSuccessInfo(message=$message)';
}

class WsMessageInfo {
  final String type;
  final String message;

  const WsMessageInfo({this.type = '', this.message = ''});

  factory WsMessageInfo.fromJson(Map<String, dynamic> json) {
    return WsMessageInfo(
      type: json['type'] as String? ?? '',
      message: json['message'] as String? ?? '',
    );
  }

  @override
  String toString() => 'WsMessageInfo(type=$type, message=$message)';
}
