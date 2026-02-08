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
};

FileCompletedMessage _$FileCompletedMessageFromJson(
  Map<String, dynamic> json,
) => FileCompletedMessage(
  phase: $enumDecode(_$ProcessorMessagePhaseEnumMap, json['phase']),
);

Map<String, dynamic> _$FileCompletedMessageToJson(
  FileCompletedMessage instance,
) => <String, dynamic>{
  'phase': _$ProcessorMessagePhaseEnumMap[instance.phase]!,
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

ProcessorSummary _$ProcessorSummaryFromJson(Map<String, dynamic> json) =>
    ProcessorSummary(
      failed: (json['failed'] as num).toInt(),
      paired: (json['paired'] as num).toInt(),
      processed: (json['processed'] as num).toInt(),
      totalFiles: (json['total_files'] as num).toInt(),
      totalOutputImages: (json['total_output_images'] as num).toInt(),
    );

Map<String, dynamic> _$ProcessorSummaryToJson(ProcessorSummary instance) =>
    <String, dynamic>{
      'failed': instance.failed,
      'paired': instance.paired,
      'processed': instance.processed,
      'total_files': instance.totalFiles,
      'total_output_images': instance.totalOutputImages,
    };

ProcessorCompleteMessage _$ProcessorCompleteMessageFromJson(
  Map<String, dynamic> json,
) => ProcessorCompleteMessage(
  totalFiles: (json['total_files'] as num).toInt(),
  processed: (json['processed'] as num).toInt(),
  failed: (json['failed'] as num).toInt(),
  durationInSeconds: (json['duration_secs'] as num).toDouble(),
  summary: ProcessorSummary.fromJson(json['summary'] as Map<String, dynamic>),
);

Map<String, dynamic> _$ProcessorCompleteMessageToJson(
  ProcessorCompleteMessage instance,
) => <String, dynamic>{
  'total_files': instance.totalFiles,
  'processed': instance.processed,
  'failed': instance.failed,
  'duration_secs': instance.durationInSeconds,
  'summary': instance.summary,
};
