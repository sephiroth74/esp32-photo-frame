// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'processing_config.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

ProcessingConfig _$ProcessingConfigFromJson(
  Map<String, dynamic> json,
) => ProcessingConfig(
  inputPath: json['inputPath'] as String,
  outputPath: json['outputPath'] as String,
  displayType:
      $enumDecodeNullable(_$DisplayTypeEnumMap, json['displayType']) ??
      DisplayType.blackWhite,
  orientation:
      $enumDecodeNullable(_$TargetOrientationEnumMap, json['orientation']) ??
      TargetOrientation.landscape,
  autoColorCorrect: json['autoColorCorrect'] as bool? ?? false,
  ditherMethod:
      $enumDecodeNullable(_$DitherMethodEnumMap, json['ditherMethod']) ??
      DitherMethod.floydSteinberg,
  ditherStrength: (json['ditherStrength'] as num?)?.toInt() ?? 100,
  contrast: (json['contrast'] as num?)?.toInt() ?? 0,
  brightness: (json['brightness'] as num?)?.toInt() ?? 0,
  saturation: (json['saturation'] as num?)?.toInt() ?? 100,
  autoOptimize: json['autoOptimize'] as bool? ?? false,
  outputBmp: json['outputBmp'] as bool? ?? false,
  outputBin: json['outputBin'] as bool? ?? true,
  outputJpg: json['outputJpg'] as bool? ?? false,
  outputPng: json['outputPng'] as bool? ?? false,
  detectPeople: json['detectPeople'] as bool? ?? false,
  confidenceThreshold: (json['confidenceThreshold'] as num?)?.toDouble() ?? 0.5,
  annotate: json['annotate'] as bool? ?? false,
  font: json['font'] as String? ?? 'Arial',
  fontSize: (json['fontSize'] as num?)?.toInt() ?? 22,
  annotationBackground: json['annotationBackground'] as String? ?? '#40000000',
  noPairing: json['noPairing'] as bool? ?? false,
  dividerWidth: (json['dividerWidth'] as num?)?.toInt() ?? 3,
  dividerColor: json['dividerColor'] as String? ?? '#FFFFFF',
  report: json['report'] as bool? ?? false,
  jobs: (json['jobs'] as num?)?.toInt() ?? 0,
  extensions: json['extensions'] as String? ?? 'jpg,jpeg,png,heic,webp,tiff',
  processorBinaryPath: json['processorBinaryPath'] as String?,
);

Map<String, dynamic> _$ProcessingConfigToJson(ProcessingConfig instance) =>
    <String, dynamic>{
      'inputPath': instance.inputPath,
      'outputPath': instance.outputPath,
      'displayType': _$DisplayTypeEnumMap[instance.displayType]!,
      'orientation': _$TargetOrientationEnumMap[instance.orientation]!,
      'autoColorCorrect': instance.autoColorCorrect,
      'ditherMethod': _$DitherMethodEnumMap[instance.ditherMethod]!,
      'ditherStrength': instance.ditherStrength,
      'contrast': instance.contrast,
      'brightness': instance.brightness,
      'saturation': instance.saturation,
      'autoOptimize': instance.autoOptimize,
      'outputBmp': instance.outputBmp,
      'outputBin': instance.outputBin,
      'outputJpg': instance.outputJpg,
      'outputPng': instance.outputPng,
      'detectPeople': instance.detectPeople,
      'confidenceThreshold': instance.confidenceThreshold,
      'annotate': instance.annotate,
      'font': instance.font,
      'fontSize': instance.fontSize,
      'annotationBackground': instance.annotationBackground,
      'noPairing': instance.noPairing,
      'dividerWidth': instance.dividerWidth,
      'dividerColor': instance.dividerColor,
      'report': instance.report,
      'jobs': instance.jobs,
      'extensions': instance.extensions,
      'processorBinaryPath': instance.processorBinaryPath,
    };

const _$DisplayTypeEnumMap = {
  DisplayType.blackWhite: 'bw',
  DisplayType.sixColor: '6c',
};

const _$TargetOrientationEnumMap = {
  TargetOrientation.landscape: '0',
  TargetOrientation.portrait: '1',
  TargetOrientation.landscapeReverse: '2',
  TargetOrientation.portraitReverse: '3',
};

const _$DitherMethodEnumMap = {
  DitherMethod.floydSteinberg: 'floyd-steinberg',
  DitherMethod.atkinson: 'atkinson',
  DitherMethod.stucki: 'stucki',
  DitherMethod.jarvisJudiceNinke: 'jarvis-judice-ninke',
  DitherMethod.ordered: 'ordered',
};
