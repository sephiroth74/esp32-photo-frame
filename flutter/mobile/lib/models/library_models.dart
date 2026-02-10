import 'package:json_annotation/json_annotation.dart';

part 'library_models.g.dart';

@JsonEnum(alwaysCreate: true)
enum DitheringMethod {
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

@JsonEnum(alwaysCreate: true)
enum DisplayType {
  @JsonValue('black-and-white')
  blackAndWhite,
  @JsonValue('six-colors')
  sixColors,
}

@JsonEnum(alwaysCreate: true)
enum ColorMode {
  @JsonValue('black-and-white')
  blackAndWhite,
  @JsonValue('six-colors')
  sixColors,
}

// ----------- Extensions for FFI enums ------------

ColorMode colorModeFromString(String value) {
  return _$ColorModeEnumMap.entries.firstWhere(
    (entry) => entry.value == value,
    orElse: () => throw ArgumentError('Unknown color mode: $value'),
  ).key;
}

DisplayType displayTypeFromString(String value) {
  return _$DisplayTypeEnumMap.entries.firstWhere(
    (entry) => entry.value == value,
    orElse: () => throw ArgumentError('Unknown display type: $value'),
  ).key;
}

extension ColorModeExtension on ColorMode {
  String toJsonValue() {
    switch (this) {
      case ColorMode.blackAndWhite:
        return 'black-and-white';
      case ColorMode.sixColors:
        return 'six-colors';
    }
  }

  DisplayType toDisplayType() {
    switch (this) {
      case ColorMode.blackAndWhite:
        return DisplayType.blackAndWhite;
      case ColorMode.sixColors:
        return DisplayType.sixColors;
    }
  }
}

extension DisplayTypeExtension on DisplayType {
  String toJsonValue() {
    return toColorMode().toJsonValue();
  }

  ColorMode toColorMode() {
    switch (this) {
      case DisplayType.blackAndWhite:
        return ColorMode.blackAndWhite;
      case DisplayType.sixColors:
        return ColorMode.sixColors;
    }
  }
}

extension DitheringMethodExtension on DitheringMethod {
  String toJsonValue() {
    return _$DitheringMethodEnumMap[this] ?? (throw ArgumentError('Unknown dithering method: $this'));
  }
}
