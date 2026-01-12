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
      DisplayType.sixColor,
  orientation:
      $enumDecodeNullable(_$TargetOrientationEnumMap, json['orientation']) ??
      TargetOrientation.landscape,
  autoColorCorrect: json['autoColorCorrect'] as bool? ?? true,
  ditherMethod:
      $enumDecodeNullable(_$DitherMethodEnumMap, json['ditherMethod']) ??
      DitherMethod.jarvisJudiceNinke,
  ditherStrength: (json['ditherStrength'] as num?)?.toDouble() ?? 1.0,
  contrast: (json['contrast'] as num?)?.toInt() ?? 0,
  brightness: (json['brightness'] as num?)?.toInt() ?? 0,
  saturationBoost: (json['saturationBoost'] as num?)?.toDouble() ?? 1.1,
  autoOptimize: json['autoOptimize'] as bool? ?? false,
  outputBmp: json['outputBmp'] as bool? ?? false,
  outputBin: json['outputBin'] as bool? ?? true,
  outputJpg: json['outputJpg'] as bool? ?? true,
  outputPng: json['outputPng'] as bool? ?? false,
  detectPeople: json['detectPeople'] as bool? ?? false,
  confidenceThreshold: (json['confidenceThreshold'] as num?)?.toDouble() ?? 0.6,
  annotate: json['annotate'] as bool? ?? false,
  font: json['font'] as String? ?? 'Arial',
  pointsize: (json['pointsize'] as num?)?.toInt() ?? 22,
  annotateBackground: json['annotateBackground'] as String? ?? '#00000040',
  dividerWidth: (json['dividerWidth'] as num?)?.toInt() ?? 3,
  dividerColor: json['dividerColor'] as String? ?? '#FFFFFF',
  force: json['force'] as bool? ?? false,
  dryRun: json['dryRun'] as bool? ?? false,
  debug: json['debug'] as bool? ?? false,
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
      'saturationBoost': instance.saturationBoost,
      'autoOptimize': instance.autoOptimize,
      'outputBmp': instance.outputBmp,
      'outputBin': instance.outputBin,
      'outputJpg': instance.outputJpg,
      'outputPng': instance.outputPng,
      'detectPeople': instance.detectPeople,
      'confidenceThreshold': instance.confidenceThreshold,
      'annotate': instance.annotate,
      'font': instance.font,
      'pointsize': instance.pointsize,
      'annotateBackground': instance.annotateBackground,
      'dividerWidth': instance.dividerWidth,
      'dividerColor': instance.dividerColor,
      'force': instance.force,
      'dryRun': instance.dryRun,
      'debug': instance.debug,
      'report': instance.report,
      'jobs': instance.jobs,
      'extensions': instance.extensions,
      'processorBinaryPath': instance.processorBinaryPath,
    };

const _$DisplayTypeEnumMap = {
  DisplayType.blackWhite: 'bw',
  DisplayType.sixColor: '6c',
  DisplayType.sevenColor: '7c',
};

const _$TargetOrientationEnumMap = {
  TargetOrientation.landscape: 'landscape',
  TargetOrientation.portrait: 'portrait',
};

const _$DitherMethodEnumMap = {
  DitherMethod.floydSteinberg: 'floyd-steinberg',
  DitherMethod.atkinson: 'atkinson',
  DitherMethod.stucki: 'stucki',
  DitherMethod.jarvisJudiceNinke: 'jarvis-judice-ninke',
  DitherMethod.ordered: 'ordered',
};
