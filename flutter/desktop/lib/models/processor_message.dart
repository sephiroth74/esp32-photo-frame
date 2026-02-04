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
  complete,
  @JsonValue('summary')
  summary;

  static ProcessorMessagePhase? fromString(String value) {
    return ProcessorMessagePhase.values.firstWhere((item) => item.name == value);
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
    required this.message}) : super(type: ProcessorMessageType.progress);

  factory ProgressMessage.fromJson(Map<String, dynamic> json) => _$ProgressMessageFromJson(json);
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
  @JsonKey(name: 'input_path')
  final String inputPath;

  @JsonKey(name: 'output_paths')
  final List<String> outputPaths;

  @JsonKey(name: 'processing_time_ms')
  final int processingTimeMs;

  FileCompletedMessage({
    required super.phase,
    required this.inputPath, required this.outputPaths, required this.processingTimeMs}) : super(type: ProcessorMessageType.filecompleted, phase: phase);

  factory FileCompletedMessage.fromJson(Map<String, dynamic> json) => _$FileCompletedMessageFromJson(json);
  Map<String, dynamic> toJson() => _$FileCompletedMessageToJson(this);
}

@JsonSerializable()
class FileFailedMessage extends ProcessorMessage {
  @JsonKey(name: 'input_path')
  final String inputPath;
  
  @JsonKey(name: 'error')
  final String error;

  FileFailedMessage({required super.phase, required this.inputPath, required this.error}) : super(type: ProcessorMessageType.filefailed);

  factory FileFailedMessage.fromJson(Map<String, dynamic> json) => _$FileFailedMessageFromJson(json);
}
