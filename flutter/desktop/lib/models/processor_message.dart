import 'package:json_annotation/json_annotation.dart';

part 'processor_message.g.dart';

/// Base class for messages from the processor
abstract class ProcessorMessage {
  final String type;

  ProcessorMessage({required this.type});

  factory ProcessorMessage.fromJson(Map<String, dynamic> json) {
    final type = json['type'] as String?;

    switch (type) {
      case 'progress':
        return ProgressMessage.fromJson(json);
      case 'filecompleted':
        return FileCompletedMessage.fromJson(json);
      case 'filefailed':
        return FileFailedMessage.fromJson(json);
      default:
        throw UnimplementedError('Unknown message type: $type');
    }
  }
}

@JsonSerializable()
class ProgressMessage extends ProcessorMessage {
  final String phase;
  final int current;
  final int total;
  final String message;

  ProgressMessage({required this.phase, required this.current, required this.total, required this.message}) : super(type: 'progress');

  factory ProgressMessage.fromJson(Map<String, dynamic> json) => _$ProgressMessageFromJson(json);
  Map<String, dynamic> toJson() => _$ProgressMessageToJson(this);

  /// Get human-readable phase name
  String get phaseName {
    switch (phase) {
      case 'startup':
        return 'Starting';
      case 'discovery':
        return 'Discovering files';
      case 'inspection':
        return 'Validating files';
      case 'processing':
        return 'Processing';
      case 'saving':
        return 'Saving output';
      case 'complete':
        return 'Complete';
      default:
        return phase;
    }
  }

  /// Check if this is the final progress message
  bool get isComplete => phase == 'complete';
}

@JsonSerializable()
class FileCompletedMessage extends ProcessorMessage {
  @JsonKey(name: 'input_path')
  final String inputPath;

  @JsonKey(name: 'output_paths')
  final List<String> outputPaths;

  @JsonKey(name: 'processing_time_ms')
  final int processingTimeMs;

  FileCompletedMessage({required this.inputPath, required this.outputPaths, required this.processingTimeMs}) : super(type: 'filecompleted');

  factory FileCompletedMessage.fromJson(Map<String, dynamic> json) => _$FileCompletedMessageFromJson(json);
  Map<String, dynamic> toJson() => _$FileCompletedMessageToJson(this);
}

@JsonSerializable()
class FileFailedMessage extends ProcessorMessage {
  @JsonKey(name: 'input_path')
  final String inputPath;
  
  @JsonKey(name: 'error')
  final String error;

  FileFailedMessage({required this.inputPath, required this.error}) : super(type: 'filefailed');

  factory FileFailedMessage.fromJson(Map<String, dynamic> json) => _$FileFailedMessageFromJson(json);
}
