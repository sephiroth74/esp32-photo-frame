import 'package:json_annotation/json_annotation.dart';
import 'package:photoframe_common/photoframe_common.dart';

part 'processing_config.g.dart';

@JsonSerializable()
class ProcessingConfig {
  final String inputPath;
  final String outputPath;
  final DisplayType displayType;
  final Orientation orientation;
  final bool autoColorCorrect;
  final DitheringMethod ditherMethod;
  final int ditherStrength;
  final int contrast;
  final int brightness;
  final int saturation;
  final bool autoOptimize;
  final bool outputBmp;
  final bool outputBin;
  final bool outputJpg;
  final bool outputPng;
  final bool detectPeople;
  final double confidenceThreshold;
  final bool annotate;
  final String font;
  final int fontSize;
  final String annotationBackground;
  final bool noPairing;
  final int dividerWidth;
  final String dividerColor;
  final bool report;
  final int jobs;
  final String extensions;
  final String? processorBinaryPath;

  const ProcessingConfig({
    required this.inputPath,
    required this.outputPath,
    this.displayType = DisplayType.blackAndWhite,
    this.orientation = Orientation.landscape,
    this.autoColorCorrect = false,
    this.ditherMethod = DitheringMethod.floydSteinberg,
    this.ditherStrength = 100,
    this.contrast = 100,
    this.brightness = 100,
    this.saturation = 100,
    this.autoOptimize = false,
    this.outputBmp = false,
    this.outputBin = true,
    this.outputJpg = false,
    this.outputPng = false,
    this.detectPeople = false,
    this.confidenceThreshold = 0.5,
    this.annotate = false,
    this.font = 'Arial',
    this.fontSize = 22,
    this.annotationBackground = '#40000000',
    this.noPairing = false,
    this.dividerWidth = 3,
    this.dividerColor = '#FFFFFF',
    this.report = false,
    this.jobs = 0,
    this.extensions = 'jpg,jpeg,png,heic,webp,tiff',
    this.processorBinaryPath,
  });

  factory ProcessingConfig.fromJson(Map<String, dynamic> json) => _$ProcessingConfigFromJson(json);

  Map<String, dynamic> toJson() => _$ProcessingConfigToJson(this);

  ProcessingConfig copyWith({
    String? inputPath,
    String? outputPath,
    DisplayType? displayType,
    Orientation? orientation,
    bool? autoColorCorrect,
    DitheringMethod? ditherMethod,
    int? ditherStrength,
    int? contrast,
    int? brightness,
    int? saturation,
    bool? autoOptimize,
    bool? outputBmp,
    bool? outputBin,
    bool? outputJpg,
    bool? outputPng,
    bool? detectPeople,
    double? confidenceThreshold,
    bool? annotate,
    String? font,
    int? fontSize,
    String? annotationBackground,
    bool? noPairing,
    int? dividerWidth,
    String? dividerColor,
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
      saturation: saturation ?? this.saturation,
      autoOptimize: autoOptimize ?? this.autoOptimize,
      outputBmp: outputBmp ?? this.outputBmp,
      outputBin: outputBin ?? this.outputBin,
      outputJpg: outputJpg ?? this.outputJpg,
      outputPng: outputPng ?? this.outputPng,
      detectPeople: detectPeople ?? this.detectPeople,
      confidenceThreshold: confidenceThreshold ?? this.confidenceThreshold,
      annotate: annotate ?? this.annotate,
      font: font ?? this.font,
      fontSize: fontSize ?? this.fontSize,
      annotationBackground: annotationBackground ?? this.annotationBackground,
      noPairing: noPairing ?? this.noPairing,
      dividerWidth: dividerWidth ?? this.dividerWidth,
      dividerColor: dividerColor ?? this.dividerColor,
      report: report ?? this.report,
      jobs: jobs ?? this.jobs,
      extensions: extensions ?? this.extensions,
      processorBinaryPath: processorBinaryPath ?? this.processorBinaryPath,
    );
  }
}
