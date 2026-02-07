import 'package:json_annotation/json_annotation.dart';

part 'ws_messages.g.dart';

@JsonSerializable()
class BoardConfig {
  final String board;
  @JsonKey(name: 'flash_size')
  final String flashSize;
  @JsonKey(name: 'flash_size_bytes')
  final int flashSizeBytes;
  @JsonKey(name: 'display_type')
  final String displayType;
  @JsonKey(name: 'display_width')
  final int displayWidth;
  @JsonKey(name: 'display_height')
  final int displayHeight;
  @JsonKey(name: 'display_rotation')
  final int displayRotation;
  @JsonKey(name: 'server_version')
  final String serverVersion;
  @JsonKey(name: 'file_version')
  final int fileVersion;
  @JsonKey(name: 'binary_file_size')
  final int binaryFileSize;
  @JsonKey(name: 'battery_level')
  final int? batteryLevel;
  @JsonKey(name: 'battery_voltage_mv')
  final int? batteryVoltageMv;

  const BoardConfig({
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

  static bool looksLikeConfig(Map<String, dynamic> json) {
    return json.containsKey('board') &&
        json.containsKey('flash_size') &&
        json.containsKey('display_width') &&
        json.containsKey('display_height') &&
        json.containsKey('server_version');
  }

  factory BoardConfig.fromJson(Map<String, dynamic> json) => _$BoardConfigFromJson(json);
  Map<String, dynamic> toJson() => _$BoardConfigToJson(this);
}

@JsonSerializable()
class WsErrorInfo {
  @JsonKey(defaultValue: 'error')
  final String type;
  @JsonKey(defaultValue: 0)
  final int code;
  @JsonKey(defaultValue: '')
  final String message;

  const WsErrorInfo({required this.type, required this.code, required this.message});

  factory WsErrorInfo.fromJson(Map<String, dynamic> json) => _$WsErrorInfoFromJson(json);
  Map<String, dynamic> toJson() => _$WsErrorInfoToJson(this);
}

@JsonSerializable()
class WsChunkAck {
  @JsonKey(defaultValue: 'chunk_ack')
  final String type;
  @JsonKey(defaultValue: 0)
  final int received;

  const WsChunkAck({required this.type, required this.received});

  factory WsChunkAck.fromJson(Map<String, dynamic> json) => _$WsChunkAckFromJson(json);
  Map<String, dynamic> toJson() => _$WsChunkAckToJson(this);
}

@JsonSerializable()
class WsReadyInfo {
  @JsonKey(defaultValue: 'ready')
  final String type;
  @JsonKey(name: 'session_id', defaultValue: 0)
  final int sessionId;

  const WsReadyInfo({required this.type, required this.sessionId});

  factory WsReadyInfo.fromJson(Map<String, dynamic> json) => _$WsReadyInfoFromJson(json);
  Map<String, dynamic> toJson() => _$WsReadyInfoToJson(this);
}

@JsonSerializable()
class WsSuccessInfo {
  @JsonKey(defaultValue: 'success')
  final String type;
  @JsonKey(defaultValue: '')
  final String message;

  const WsSuccessInfo({required this.type, required this.message});

  factory WsSuccessInfo.fromJson(Map<String, dynamic> json) => _$WsSuccessInfoFromJson(json);
  Map<String, dynamic> toJson() => _$WsSuccessInfoToJson(this);
}

@JsonSerializable()
class WsMessageInfo {
  @JsonKey(defaultValue: '')
  final String type;
  @JsonKey(defaultValue: '')
  final String message;

  const WsMessageInfo({required this.type, required this.message});

  factory WsMessageInfo.fromJson(Map<String, dynamic> json) => _$WsMessageInfoFromJson(json);
  Map<String, dynamic> toJson() => _$WsMessageInfoToJson(this);
}
