// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'ws_messages.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

BoardConfig _$BoardConfigFromJson(Map<String, dynamic> json) => BoardConfig(
  board: json['board'] as String,
  flashSize: json['flash_size'] as String,
  flashSizeBytes: (json['flash_size_bytes'] as num).toInt(),
  displayType: json['display_type'] as String,
  displayWidth: (json['display_width'] as num).toInt(),
  displayHeight: (json['display_height'] as num).toInt(),
  displayRotation: (json['display_rotation'] as num).toInt(),
  serverVersion: json['server_version'] as String,
  fileVersion: (json['file_version'] as num).toInt(),
  binaryFileSize: (json['binary_file_size'] as num).toInt(),
  batteryLevel: (json['battery_level'] as num?)?.toInt(),
  batteryVoltageMv: (json['battery_voltage_mv'] as num?)?.toInt(),
);

Map<String, dynamic> _$BoardConfigToJson(BoardConfig instance) => <String, dynamic>{
  'board': instance.board,
  'flash_size': instance.flashSize,
  'flash_size_bytes': instance.flashSizeBytes,
  'display_type': instance.displayType,
  'display_width': instance.displayWidth,
  'display_height': instance.displayHeight,
  'display_rotation': instance.displayRotation,
  'server_version': instance.serverVersion,
  'file_version': instance.fileVersion,
  'binary_file_size': instance.binaryFileSize,
  'battery_level': instance.batteryLevel,
  'battery_voltage_mv': instance.batteryVoltageMv,
};

WsErrorInfo _$WsErrorInfoFromJson(Map<String, dynamic> json) =>
    WsErrorInfo(type: json['type'] as String? ?? 'error', code: (json['code'] as num?)?.toInt() ?? 0, message: json['message'] as String? ?? '');

Map<String, dynamic> _$WsErrorInfoToJson(WsErrorInfo instance) => <String, dynamic>{
  'type': instance.type,
  'code': instance.code,
  'message': instance.message,
};

WsChunkAck _$WsChunkAckFromJson(Map<String, dynamic> json) =>
    WsChunkAck(type: json['type'] as String? ?? 'chunk_ack', received: (json['received'] as num?)?.toInt() ?? 0);

Map<String, dynamic> _$WsChunkAckToJson(WsChunkAck instance) => <String, dynamic>{'type': instance.type, 'received': instance.received};

WsReadyInfo _$WsReadyInfoFromJson(Map<String, dynamic> json) =>
    WsReadyInfo(type: json['type'] as String? ?? 'ready', sessionId: (json['session_id'] as num?)?.toInt() ?? 0);

Map<String, dynamic> _$WsReadyInfoToJson(WsReadyInfo instance) => <String, dynamic>{'type': instance.type, 'session_id': instance.sessionId};

WsSuccessInfo _$WsSuccessInfoFromJson(Map<String, dynamic> json) =>
    WsSuccessInfo(type: json['type'] as String? ?? 'success', message: json['message'] as String? ?? '');

Map<String, dynamic> _$WsSuccessInfoToJson(WsSuccessInfo instance) => <String, dynamic>{'type': instance.type, 'message': instance.message};

WsMessageInfo _$WsMessageInfoFromJson(Map<String, dynamic> json) =>
    WsMessageInfo(type: json['type'] as String? ?? '', message: json['message'] as String? ?? '');

Map<String, dynamic> _$WsMessageInfoToJson(WsMessageInfo instance) => <String, dynamic>{'type': instance.type, 'message': instance.message};
