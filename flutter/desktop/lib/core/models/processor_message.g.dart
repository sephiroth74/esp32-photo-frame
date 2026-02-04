// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'processor_message.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

ProgressMessage _$ProgressMessageFromJson(Map<String, dynamic> json) =>
    ProgressMessage(
      phase: json['phase'] as String,
      current: (json['current'] as num).toInt(),
      total: (json['total'] as num).toInt(),
      message: json['message'] as String,
    );

Map<String, dynamic> _$ProgressMessageToJson(ProgressMessage instance) =>
    <String, dynamic>{
      'phase': instance.phase,
      'current': instance.current,
      'total': instance.total,
      'message': instance.message,
    };

FileCompletedMessage _$FileCompletedMessageFromJson(
  Map<String, dynamic> json,
) => FileCompletedMessage(
  inputPath: json['input_path'] as String,
  outputPaths: (json['output_paths'] as List<dynamic>)
      .map((e) => e as String)
      .toList(),
  processingTimeMs: (json['processing_time_ms'] as num).toInt(),
);

Map<String, dynamic> _$FileCompletedMessageToJson(
  FileCompletedMessage instance,
) => <String, dynamic>{
  'input_path': instance.inputPath,
  'output_paths': instance.outputPaths,
  'processing_time_ms': instance.processingTimeMs,
};
