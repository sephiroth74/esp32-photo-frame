import 'package:json_annotation/json_annotation.dart';

part 'processing_config.g.dart';

enum DisplayType {
  @JsonValue('bw')
  blackWhite,
  @JsonValue('6c')
  sixColor,
  @JsonValue('7c')
  sevenColor,
}

enum TargetOrientation {
  @JsonValue('landscape')
  landscape,
  @JsonValue('portrait')
  portrait,
}

enum DitherMethod {
  @JsonValue('floyd-steinberg')
  floydSteinberg,
  @JsonValue('atkinson')
  atkinson,
  @JsonValue('stucki')
  stucki,
  @JsonValue('jarvis-judice-ninke')
  jarvisJudiceNinke,
  @JsonValue('ordered')
  ordered,
}

@JsonSerializable()
class ProcessingConfig {
  final String inputPath;
  final String outputPath;
  final DisplayType displayType;
  final TargetOrientation orientation;
  final bool autoColorCorrect;
  final DitherMethod ditherMethod;
  final double ditherStrength;
  final int contrast;
  final int brightness;
  final double saturationBoost;
  final bool autoOptimize;
  final bool outputBmp;
  final bool outputBin;
  final bool outputJpg;
  final bool outputPng;
  final bool detectPeople;
  final double confidenceThreshold;
  final bool annotate;
  final String font;
  final int pointsize;
  final String annotateBackground;
  final int dividerWidth;
  final String dividerColor;
  final bool force;
  final bool dryRun;
  final bool debug;
  final bool report;
  final int jobs;
  final String extensions;
  final String? processorBinaryPath;

  const ProcessingConfig({
    required this.inputPath,
    required this.outputPath,
    this.displayType = DisplayType.sixColor,
    this.orientation = TargetOrientation.landscape,
    this.autoColorCorrect = true,
    this.ditherMethod = DitherMethod.jarvisJudiceNinke,
    this.ditherStrength = 1.0,
    this.contrast = 0,
    this.brightness = 0,
    this.saturationBoost = 1.1,
    this.autoOptimize = false,
    this.outputBmp = false,
    this.outputBin = true,
    this.outputJpg = true,
    this.outputPng = false,
    this.detectPeople = false,
    this.confidenceThreshold = 0.6,
    this.annotate = false,
    this.font = 'Arial',
    this.pointsize = 22,
    this.annotateBackground = '#00000040',
    this.dividerWidth = 3,
    this.dividerColor = '#FFFFFF',
    this.force = false,
    this.dryRun = false,
    this.debug = false,
    this.report = false,
    this.jobs = 0,
    this.extensions = 'jpg,jpeg,png,heic,webp,tiff',
    this.processorBinaryPath,
  });

  factory ProcessingConfig.fromJson(Map<String, dynamic> json) =>
      _$ProcessingConfigFromJson(json);

  Map<String, dynamic> toJson() => _$ProcessingConfigToJson(this);

  ProcessingConfig copyWith({
    String? inputPath,
    String? outputPath,
    DisplayType? displayType,
    TargetOrientation? orientation,
    bool? autoColorCorrect,
    DitherMethod? ditherMethod,
    double? ditherStrength,
    int? contrast,
    int? brightness,
    double? saturationBoost,
    bool? autoOptimize,
    bool? outputBmp,
    bool? outputBin,
    bool? outputJpg,
    bool? outputPng,
    bool? detectPeople,
    double? confidenceThreshold,
    bool? annotate,
    String? font,
    int? pointsize,
    String? annotateBackground,
    int? dividerWidth,
    String? dividerColor,
    bool? force,
    bool? dryRun,
    bool? debug,
    bool? report,
    int? jobs,
    String? extensions,
    String? processorBinaryPath,
  }) {
    return ProcessingConfig(
      inputPath: inputPath ?? this.inputPath,
      outputPath: outputPath ?? this.outputPath,
      displayType: displayType ?? this.displayType,
      orientation: orientation ?? this.orientation,
      autoColorCorrect: autoColorCorrect ?? this.autoColorCorrect,
      ditherMethod: ditherMethod ?? this.ditherMethod,
      ditherStrength: ditherStrength ?? this.ditherStrength,
      contrast: contrast ?? this.contrast,
      brightness: brightness ?? this.brightness,
      saturationBoost: saturationBoost ?? this.saturationBoost,
      autoOptimize: autoOptimize ?? this.autoOptimize,
      outputBmp: outputBmp ?? this.outputBmp,
      outputBin: outputBin ?? this.outputBin,
      outputJpg: outputJpg ?? this.outputJpg,
      outputPng: outputPng ?? this.outputPng,
      detectPeople: detectPeople ?? this.detectPeople,
      confidenceThreshold: confidenceThreshold ?? this.confidenceThreshold,
      annotate: annotate ?? this.annotate,
      font: font ?? this.font,
      pointsize: pointsize ?? this.pointsize,
      annotateBackground: annotateBackground ?? this.annotateBackground,
      dividerWidth: dividerWidth ?? this.dividerWidth,
      dividerColor: dividerColor ?? this.dividerColor,
      force: force ?? this.force,
      dryRun: dryRun ?? this.dryRun,
      debug: debug ?? this.debug,
      report: report ?? this.report,
      jobs: jobs ?? this.jobs,
      extensions: extensions ?? this.extensions,
      processorBinaryPath: processorBinaryPath ?? this.processorBinaryPath,
    );
  }
}
