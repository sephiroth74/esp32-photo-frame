import 'package:json_annotation/json_annotation.dart';

part 'processor_message.g.dart';

@JsonEnum()
enum ProcessorMessageType {
  progress,
  filecompleted,
  filefailed,
  summary,
  error,
  complete;

  static ProcessorMessageType? fromString(String value) {
    return ProcessorMessageType.values.firstWhere((item) => item.name == value);
  }
}

@JsonEnum(alwaysCreate: true)
enum ProcessorMessagePhase {
  @JsonValue('startup')
  startup,
  @JsonValue('validation')
  validation,
  @JsonValue('discovery')
  discovery,
  @JsonValue('inspection')
  inspection,
  @JsonValue('processing')
  processing,
  @JsonValue('saving')
  saving,
  @JsonValue('complete')
  complete;

  static ProcessorMessagePhase? fromString(String value) {
    return ProcessorMessagePhase.values.firstWhere(
      (item) => item.name == value,
    );
  }
}

/// Base class for messages from the processor
abstract class ProcessorMessage {
  final ProcessorMessageType type;
  final ProcessorMessagePhase phase;

  ProcessorMessage({required this.type, required this.phase});

  factory ProcessorMessage.fromJson(Map<String, dynamic> json) {
    final type = ProcessorMessageType.fromString(json['type']);

    switch (type) {
      case ProcessorMessageType.progress:
        return ProgressMessage.fromJson(json);
      case ProcessorMessageType.filecompleted:
        return FileCompletedMessage.fromJson(json);
      case ProcessorMessageType.filefailed:
        return FileFailedMessage.fromJson(json);
      case ProcessorMessageType.complete:
        return ProcessorCompleteMessage.fromJson(json);
      default:
        throw UnimplementedError('Unknown message type: $type');
    }
  }
}

@JsonSerializable()
class ProgressMessage extends ProcessorMessage {
  final int current;
  final int total;
  final String message;

  ProgressMessage({
    required super.phase,
    required this.current,
    required this.total,
    required this.message,
  }) : super(type: ProcessorMessageType.progress);

  factory ProgressMessage.fromJson(Map<String, dynamic> json) =>
      _$ProgressMessageFromJson(json);
  Map<String, dynamic> toJson() => _$ProgressMessageToJson(this);

  /// Get human-readable phase name
  String get phaseName {
    return phase.name;
  }

  /// Check if this is the final progress message
  bool get isComplete => phase == ProcessorMessagePhase.complete;
}

@JsonSerializable()
class FileCompletedMessage extends ProcessorMessage {
  // @JsonKey(name: 'input_path')
  // final String inputPath;
  // @JsonKey(name: 'output_paths')
  // final List<String> outputPaths;
  // @JsonKey(name: 'processing_time_ms')
  // final int processingTimeMs;
  FileCompletedMessage({required super.phase})
    : super(type: ProcessorMessageType.filecompleted);

  factory FileCompletedMessage.fromJson(Map<String, dynamic> json) =>
      _$FileCompletedMessageFromJson(json);
  Map<String, dynamic> toJson() => _$FileCompletedMessageToJson(this);
}

@JsonSerializable(createToJson: false)
class FileFailedMessage extends ProcessorMessage {
  @JsonKey(name: 'input_path')
  final String inputPath;

  @JsonKey(name: 'error')
  final String error;

  FileFailedMessage({
    required super.phase,
    required this.inputPath,
    required this.error,
  }) : super(type: ProcessorMessageType.filefailed);

  factory FileFailedMessage.fromJson(Map<String, dynamic> json) =>
      _$FileFailedMessageFromJson(json);
}

@JsonSerializable(createToJson: false)
class ProcessorSummary {
  @JsonKey(name: 'failed')
  final int failed;
  @JsonKey(name: 'paired')
  final int paired;
  @JsonKey(name: 'processed')
  final int processed;
  @JsonKey(name: 'total_files')
  final int totalFiles;
  @JsonKey(name: 'total_output_images')
  final int totalOutputImages;

  ProcessorSummary({
    required this.failed,
    required this.paired,
    required this.processed,
    required this.totalFiles,
    required this.totalOutputImages,
  });

  factory ProcessorSummary.fromJson(Map<String, dynamic> json) =>
      _$ProcessorSummaryFromJson(json);

  factory ProcessorSummary.empty() {
    return ProcessorSummary(
      failed: 0,
      paired: 0,
      processed: 0,
      totalFiles: 0,
      totalOutputImages: 0,
    );
  }
}

// "type":"complete",
// "phase":"complete",
// "total_files":16,
// "processed":16,
// "failed":0,
// "duration_secs":2.461637167,
// "report":{"config":{"extensions":"jpg,jpeg,png,heic,webp,tiff","input_paths":["/Users/alessandro/Desktop/arduino/photos/test"],"no_pairing":true,"output_dir":"/Users/alessandro/Desktop/arduino/photos/outputs/test","output_formats":["pfr1","jpg"]},"paired_images":[],"processed_images":[{"detect_ms":1025,"faces":2,"output":"1691418184145_e6d26805.pfr1","size":"480x800","source":"1691418184145.jpg","total_ms":1381},{"detect_ms":439,"faces":1,"output":"20220720_195613_9df835e7.pfr1","size":"480x800","source":"20220720_195613.jpg","total_ms":629},{"detect_ms":279,"faces":1,"output":"20220720_195642_477d8ece.pfr1","size":"480x800","source":"20220720_195642.jpg","total_ms":386},{"detect_ms":889,"faces":1,"output":"20220721_083759_d772beaf.pfr1","size":"480x800","source":"20220721_083759.jpg","total_ms":1097},{"detect_ms":594,"faces":2,"output":"20230402_170803_1dd60086.pfr1","size":"480x800","source":"20230402_170803.jpg","total_ms":759},{"detect_ms":1405,"faces":13,"output":"20230409_175100_54dc816d.pfr1","size":"480x800","source":"20230409_175100.jpg","total_ms":1559},{"detect_ms":1192,"faces":0,"output":"DSC_0599_89c0aa70.pfr1","size":"480x800","source":"DSC_0599.jpg","total_ms":1710},{"detect_ms":1289,"faces":1,"output":"IMG-20250610-WA0018_6aa37055.pfr1","size":"480x800","source":"IMG-20250610-WA0018.jpg","total_ms":1449},{"detect_ms":295,"faces":2,"output":"IMG-20250719-WA0004_1e9c3e76.pfr1","size":"480x800","source":"IMG-20250719-WA0004.jpg","total_ms":379},{"detect_ms":1125,"faces":2,"output":"IMG-20251006-WA0002_97794d4d.pfr1","size":"480x800","source":"IMG-20251006-WA0002.jpg","total_ms":1209},{"detect_ms":286,"faces":2,"output":"IMG-20251006-WA0003_c4092582.pfr1","size":"480x800","source":"IMG-20251006-WA0003.jpg","total_ms":389},{"detect_ms":263,"faces":1,"output":"IMG-20251113-WA0000_38b43cc4.pfr1","size":"480x800","source":"IMG-20251113-WA0000.jpg","total_ms":451},{"detect_ms":763,"faces":5,"output":"PXL_20250530_133132350_949f7883.pfr1","size":"480x800","source":"PXL_20250530_133132350.jpg","total_ms":967},{"detect_ms":1398,"faces":0,"output":"DSC0468-Edit_ccacfd2b.pfr1","size":"480x800","source":"_DSC0468-Edit.jpg","total_ms":1754},{"detect_ms":1177,"faces":4,"output":"cc7d8f28-49dd-4a0d-82f2-a5fe7facb882_beb9c921.pfr1","size":"480x800","source":"cc7d8f28-49dd-4a0d-82f2-a5fe7facb882.JPG","total_ms":1315},{"detect_ms":1425,"faces":0,"output":"default_portrait_50d7a647.pfr1","size":"480x800","source":"default_portrait.jpg","total_ms":1522}],"summary":{"failed_images":0,"image_pairs_created":0,"images_processed":16,"invalid_files":0,"total_files_discovered":16,"total_output_images":16,"unpaired_images":0}},
// "summary":{"failed":0,"paired":0,"processed":16,"total_files":16,"total_output_images":16}}

@JsonSerializable(createToJson: false)
class ProcessorCompleteMessage extends ProcessorMessage {
  @JsonKey(name: 'total_files')
  final int totalFiles;
  @JsonKey(name: 'processed')
  final int processed;
  @JsonKey(name: 'failed')
  final int failed;
  @JsonKey(name: 'duration_secs')
  final double durationInSeconds;
  final ProcessorSummary summary;

  ProcessorCompleteMessage({
    required this.totalFiles,
    required this.processed,
    required this.failed,
    required this.durationInSeconds,
    required this.summary,
  }) : super(
         phase: ProcessorMessagePhase.complete,
         type: ProcessorMessageType.complete,
       );

  factory ProcessorCompleteMessage.fromJson(Map<String, dynamic> json) =>
      _$ProcessorCompleteMessageFromJson(json);
}
