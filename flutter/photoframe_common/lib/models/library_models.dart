import 'package:json_annotation/json_annotation.dart';

part 'library_models.g.dart';

@JsonEnum(alwaysCreate: true)
enum Orientation {
  @JsonValue('0')
  landscape,
  @JsonValue('1')
  portrait,
  @JsonValue('2')
  landscapeReverse,
  @JsonValue('3')
  portraitReverse,
}

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
  return _$ColorModeEnumMap.entries
      .firstWhere(
        (entry) => entry.value == value,
        orElse: () => throw ArgumentError('Unknown color mode: $value'),
      )
      .key;
}

ColorMode colorModeFromInt(int value) {
  switch (value) {
    case 0:
      return ColorMode.blackAndWhite;
    case 1:
      return ColorMode.sixColors;
    default:
      throw ArgumentError('Unknown color mode value: $value');
  }
}

DisplayType displayTypeFromString(String value) {
  return _$DisplayTypeEnumMap.entries
      .firstWhere(
        (entry) => entry.value == value,
        orElse: () => throw ArgumentError('Unknown display type: $value'),
      )
      .key;
}

Orientation orientationFromInt(int value) {
  return _$OrientationEnumMap.entries
      .firstWhere(
        (entry) => entry.value == value.toString(),
        orElse: () => throw ArgumentError('Unknown orientation value: $value'),
      )
      .key;
}

Orientation orientationFromString(String value) {
  return _$OrientationEnumMap.entries
      .firstWhere(
        (entry) => entry.value == value,
        orElse: () => throw ArgumentError('Unknown orientation: $value'),
      )
      .key;
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

  int toInt() {
    switch (this) {
      case ColorMode.blackAndWhite:
        return 0;
      case ColorMode.sixColors:
        return 1;
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
    return _$DitheringMethodEnumMap[this] ??
        (throw ArgumentError('Unknown dithering method: $this'));
  }
}

extension OrientationExtension on Orientation {
  String toJsonValue() {
    return _$OrientationEnumMap[this] ??
        (throw ArgumentError('Unknown orientation: $this'));
  }

  int toInt() {
    final value = _$OrientationEnumMap[this];
    if (value == null) {
      throw ArgumentError('Unknown orientation: $this');
    }
    return int.parse(value);
  }

  String toReadableString() {
    switch (this) {
      case Orientation.landscape:
        return 'Landscape';
      case Orientation.portrait:
        return 'Portrait';
      case Orientation.landscapeReverse:
        return 'Landscape (Reverse)';
      case Orientation.portraitReverse:
        return 'Portrait (Reverse)';
    }
  }
}
