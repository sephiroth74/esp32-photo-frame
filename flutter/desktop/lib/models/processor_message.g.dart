// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'processor_message.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

ProgressMessage _$ProgressMessageFromJson(Map<String, dynamic> json) =>
    ProgressMessage(
      phase: $enumDecode(_$ProcessorMessagePhaseEnumMap, json['phase']),
      current: (json['current'] as num).toInt(),
      total: (json['total'] as num).toInt(),
      message: json['message'] as String,
    );

Map<String, dynamic> _$ProgressMessageToJson(ProgressMessage instance) =>
    <String, dynamic>{
      'phase': _$ProcessorMessagePhaseEnumMap[instance.phase]!,
      'current': instance.current,
      'total': instance.total,
      'message': instance.message,
    };

const _$ProcessorMessagePhaseEnumMap = {
  ProcessorMessagePhase.startup: 'startup',
  ProcessorMessagePhase.validation: 'validation',
  ProcessorMessagePhase.discovery: 'discovery',
  ProcessorMessagePhase.inspection: 'inspection',
  ProcessorMessagePhase.processing: 'processing',
  ProcessorMessagePhase.saving: 'saving',
  ProcessorMessagePhase.complete: 'complete',
  ProcessorMessagePhase.summary: 'summary',
};

FileCompletedMessage _$FileCompletedMessageFromJson(
  Map<String, dynamic> json,
) => FileCompletedMessage(
  phase: $enumDecode(_$ProcessorMessagePhaseEnumMap, json['phase']),
  inputPath: json['input_path'] as String,
  outputPaths: (json['output_paths'] as List<dynamic>)
      .map((e) => e as String)
      .toList(),
  processingTimeMs: (json['processing_time_ms'] as num).toInt(),
);

Map<String, dynamic> _$FileCompletedMessageToJson(
  FileCompletedMessage instance,
) => <String, dynamic>{
  'phase': _$ProcessorMessagePhaseEnumMap[instance.phase]!,
  'input_path': instance.inputPath,
  'output_paths': instance.outputPaths,
  'processing_time_ms': instance.processingTimeMs,
};

FileFailedMessage _$FileFailedMessageFromJson(Map<String, dynamic> json) =>
    FileFailedMessage(
      phase: $enumDecode(_$ProcessorMessagePhaseEnumMap, json['phase']),
      inputPath: json['input_path'] as String,
      error: json['error'] as String,
    );

Map<String, dynamic> _$FileFailedMessageToJson(FileFailedMessage instance) =>
    <String, dynamic>{
      'phase': _$ProcessorMessagePhaseEnumMap[instance.phase]!,
      'input_path': instance.inputPath,
      'error': instance.error,
    };
