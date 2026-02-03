import 'package:json_annotation/json_annotation.dart';

part 'processor_report.g.dart';

@JsonSerializable()
class ProcessorReport {
  final ProcessorConfig config;
  final List<ProcessedImage> processed_images;
  final List<PairedImage> paired_images;
  final ProcessorSummary summary;

  ProcessorReport({required this.config, required this.processed_images, required this.paired_images, required this.summary});

  factory ProcessorReport.fromJson(Map<String, dynamic> json) => _$ProcessorReportFromJson(json);
  Map<String, dynamic> toJson() => _$ProcessorReportToJson(this);
}

@JsonSerializable()
class ProcessorConfig {
  final List<String>? extensions;
  final List<String>? input_paths;
  final bool? no_pairing;
  final String? output_dir;

  ProcessorConfig({this.extensions, this.input_paths, this.no_pairing, this.output_dir});

  factory ProcessorConfig.fromJson(Map<String, dynamic> json) => _$ProcessorConfigFromJson(json);
  Map<String, dynamic> toJson() => _$ProcessorConfigToJson(this);
}

@JsonSerializable()
class ProcessedImage {
  final String source;
  final String? output;
  final String? size;
  final List<dynamic>? faces;
  final int? total_ms;
  final int? detect_ms;

  ProcessedImage({required this.source, this.output, this.size, this.faces, this.total_ms, this.detect_ms});

  factory ProcessedImage.fromJson(Map<String, dynamic> json) => _$ProcessedImageFromJson(json);
  Map<String, dynamic> toJson() => _$ProcessedImageToJson(this);
}

@JsonSerializable()
class PairedImage {
  final String? first;
  final String? second;
  final String? output;

  PairedImage({this.first, this.second, this.output});

  factory PairedImage.fromJson(Map<String, dynamic> json) => _$PairedImageFromJson(json);
  Map<String, dynamic> toJson() => _$PairedImageToJson(this);
}

@JsonSerializable()
class ProcessorSummary {
  final int? total_files_discovered;
  final int? invalid_files;
  final int? images_processed;
  final int? failed_images;
  final int? image_pairs_created;
  final int? unpaired_images;
  final int? total_output_images;

  ProcessorSummary({
    this.total_files_discovered,
    this.invalid_files,
    this.images_processed,
    this.failed_images,
    this.image_pairs_created,
    this.unpaired_images,
    this.total_output_images,
  });

  factory ProcessorSummary.fromJson(Map<String, dynamic> json) => _$ProcessorSummaryFromJson(json);
  Map<String, dynamic> toJson() => _$ProcessorSummaryToJson(this);
}
