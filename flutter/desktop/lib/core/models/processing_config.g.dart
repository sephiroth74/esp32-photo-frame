// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'processing_config.dart';

// **************************************************************************
// JsonSerializableGenerator
// **************************************************************************

ProcessingConfig _$ProcessingConfigFromJson(Map<String, dynamic> json) => ProcessingConfig(
  inputPath: json['inputPath'] as String,
  outputPath: json['outputPath'] as String,
  displayType: $enumDecodeNullable(_$DisplayTypeEnumMap, json['displayType']) ?? DisplayType.blackAndWhite,
  orientation: $enumDecodeNullable(_$OrientationEnumMap, json['orientation']) ?? Orientation.landscape,
  autoColorCorrect: json['autoColorCorrect'] as bool? ?? false,
  ditherMethod: $enumDecodeNullable(_$DitheringMethodEnumMap, json['ditherMethod']) ?? DitheringMethod.floydSteinberg,
  ditherStrength: (json['ditherStrength'] as num?)?.toInt() ?? 100,
  contrast: (json['contrast'] as num?)?.toInt() ?? 100,
  brightness: (json['brightness'] as num?)?.toInt() ?? 100,
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

Map<String, dynamic> _$ProcessingConfigToJson(ProcessingConfig instance) => <String, dynamic>{
  'inputPath': instance.inputPath,
  'outputPath': instance.outputPath,
  'displayType': _$DisplayTypeEnumMap[instance.displayType]!,
  'orientation': _$OrientationEnumMap[instance.orientation]!,
  'autoColorCorrect': instance.autoColorCorrect,
  'ditherMethod': _$DitheringMethodEnumMap[instance.ditherMethod]!,
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

const _$DisplayTypeEnumMap = {DisplayType.blackAndWhite: 'black-and-white', DisplayType.sixColors: 'six-colors'};

const _$OrientationEnumMap = {
  Orientation.landscape: '0',
  Orientation.portrait: '1',
  Orientation.landscapeReverse: '2',
  Orientation.portraitReverse: '3',
};

const _$DitheringMethodEnumMap = {
  DitheringMethod.floydSteinberg: 'floyd-steinberg',
  DitheringMethod.atkinson: 'atkinson',
  DitheringMethod.stucki: 'stucki',
  DitheringMethod.jarvisJudiceNinke: 'jarvis-judice-ninke',
  DitheringMethod.ordered: 'ordered',
};
