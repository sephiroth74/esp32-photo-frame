import 'dart:math' as math;

import 'package:json_annotation/json_annotation.dart';

part 'library_models.g.dart';

@JsonEnum(alwaysCreate: true, valueField: 'value')
enum Orientation {
  landscape(0),
  portrait(1),
  landscapeReverse(2),
  portraitReverse(3);

  final int value;
  const Orientation(this.value);
}

@JsonEnum(alwaysCreate: true)
enum DitheringMethod {
  @JsonValue('floyd-steinberg')
  floydSteinberg(0),
  @JsonValue('atkinson')
  atkinson(1),
  @JsonValue('stucki')
  stucki(2),
  @JsonValue('jarvis-judice-ninke')
  jarvisJudiceNinke(3),
  @JsonValue('ordered')
  ordered(4);

  final int value;
  const DitheringMethod([this.value = 0]);
}

@JsonEnum(alwaysCreate: true)
enum DisplayType {
  @JsonValue('black-and-white')
  blackAndWhite(0),
  @JsonValue('six-colors')
  sixColors(1);

  final int value;
  const DisplayType([this.value = 0]);
}

@JsonEnum(alwaysCreate: true)
enum ColorMode {
  @JsonValue('black-and-white')
  blackAndWhite(0),
  @JsonValue('six-colors')
  sixColors(1);

  final int value;
  const ColorMode([this.value = 0]);
}

// ----------- Extensions for FFI enums ------------

ColorMode colorModeFromString(String value) {
  return _$ColorModeEnumMap.entries.firstWhere((entry) => entry.value == value, orElse: () => throw ArgumentError('Unknown color mode: $value')).key;
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
      .firstWhere((entry) => entry.value == value, orElse: () => throw ArgumentError('Unknown display type: $value'))
      .key;
}

Orientation orientationFromInt(int value) {
  return _$OrientationEnumMap.entries
      .firstWhere((entry) => entry.value == value, orElse: () => throw ArgumentError('Unknown orientation value: $value'))
      .key;
}

Orientation orientationFromString(String value) {
  return _$OrientationEnumMap.entries
      .firstWhere((entry) => entry.value == int.parse(value), orElse: () => throw ArgumentError('Unknown orientation: $value'))
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
    return _$DitheringMethodEnumMap[this] ?? (throw ArgumentError('Unknown dithering method: $this'));
  }
}

extension OrientationExtension on Orientation {
  String toJsonValue() {
    return _$OrientationEnumMap[this].toString();
  }

  int toInt() {
    final value = _$OrientationEnumMap[this];
    if (value == null) {
      throw ArgumentError('Unknown orientation: $this');
    }
    return value;
  }

  double toRadians() {
    switch (this) {
      case Orientation.landscape:
        return 0.0;
      case Orientation.portrait:
        return math.pi / 2;
      case Orientation.landscapeReverse:
        return math.pi;
      case Orientation.portraitReverse:
        return 3 * math.pi / 2;
    }
  }

  int toDegrees() {
    switch (this) {
      case Orientation.landscape:
        return 0;
      case Orientation.portrait:
        return 90;
      case Orientation.landscapeReverse:
        return 180;
      case Orientation.portraitReverse:
        return 270;
    }
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
