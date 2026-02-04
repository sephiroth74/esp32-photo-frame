// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'processor_report.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

ProcessorReport _$ProcessorReportFromJson(Map<String, dynamic> json) => ProcessorReport(
  config: ProcessorConfig.fromJson(json['config'] as Map<String, dynamic>),
  processed_images: (json['processed_images'] as List<dynamic>).map((e) => ProcessedImage.fromJson(e as Map<String, dynamic>)).toList(),
  paired_images: (json['paired_images'] as List<dynamic>).map((e) => PairedImage.fromJson(e as Map<String, dynamic>)).toList(),
  summary: ProcessorSummary.fromJson(json['summary'] as Map<String, dynamic>),
);

Map<String, dynamic> _$ProcessorReportToJson(ProcessorReport instance) => <String, dynamic>{
  'config': instance.config,
  'processed_images': instance.processed_images,
  'paired_images': instance.paired_images,
  'summary': instance.summary,
};

ProcessorConfig _$ProcessorConfigFromJson(Map<String, dynamic> json) => ProcessorConfig(
  extensions: (json['extensions'] as List<dynamic>?)?.map((e) => e as String).toList(),
  input_paths: (json['input_paths'] as List<dynamic>?)?.map((e) => e as String).toList(),
  no_pairing: json['no_pairing'] as bool?,
  output_dir: json['output_dir'] as String?,
);

Map<String, dynamic> _$ProcessorConfigToJson(ProcessorConfig instance) => <String, dynamic>{
  'extensions': instance.extensions,
  'input_paths': instance.input_paths,
  'no_pairing': instance.no_pairing,
  'output_dir': instance.output_dir,
};

ProcessedImage _$ProcessedImageFromJson(Map<String, dynamic> json) => ProcessedImage(
  source: json['source'] as String,
  output: json['output'] as String?,
  size: json['size'] as String?,
  faces: json['faces'] as List<dynamic>?,
  total_ms: (json['total_ms'] as num?)?.toInt(),
  detect_ms: (json['detect_ms'] as num?)?.toInt(),
);

Map<String, dynamic> _$ProcessedImageToJson(ProcessedImage instance) => <String, dynamic>{
  'source': instance.source,
  'output': instance.output,
  'size': instance.size,
  'faces': instance.faces,
  'total_ms': instance.total_ms,
  'detect_ms': instance.detect_ms,
};

PairedImage _$PairedImageFromJson(Map<String, dynamic> json) =>
    PairedImage(first: json['first'] as String?, second: json['second'] as String?, output: json['output'] as String?);

Map<String, dynamic> _$PairedImageToJson(PairedImage instance) => <String, dynamic>{
  'first': instance.first,
  'second': instance.second,
  'output': instance.output,
};

ProcessorSummary _$ProcessorSummaryFromJson(Map<String, dynamic> json) => ProcessorSummary(
  total_files_discovered: (json['total_files_discovered'] as num?)?.toInt(),
  invalid_files: (json['invalid_files'] as num?)?.toInt(),
  images_processed: (json['images_processed'] as num?)?.toInt(),
  failed_images: (json['failed_images'] as num?)?.toInt(),
  image_pairs_created: (json['image_pairs_created'] as num?)?.toInt(),
  unpaired_images: (json['unpaired_images'] as num?)?.toInt(),
  total_output_images: (json['total_output_images'] as num?)?.toInt(),
);

Map<String, dynamic> _$ProcessorSummaryToJson(ProcessorSummary instance) => <String, dynamic>{
  'total_files_discovered': instance.total_files_discovered,
  'invalid_files': instance.invalid_files,
  'images_processed': instance.images_processed,
  'failed_images': instance.failed_images,
  'image_pairs_created': instance.image_pairs_created,
  'unpaired_images': instance.unpaired_images,
  'total_output_images': instance.total_output_images,
};
