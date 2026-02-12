# Photoframe Common

Shared models and utilities for ESP32 Photo Frame Flutter applications (mobile & desktop).

## Purpose

This package contains models and enums shared between the mobile and desktop Flutter applications to:
- Ensure consistency across platforms
- Centralize protocol definitions
- Simplify maintenance (one source of truth)
- Enable type-safe FFI communication with Rust library

## Contents

### Models

#### `library_models.dart`
FFI-compatible enums for Rust `photoframe_lib`:
- **DitheringMethod**: Floyd-Steinberg, Atkinson, Stucki, Jarvis-Judice-Ninke, Ordered
- **DisplayType**: BlackAndWhite, SixColors  
- **ColorMode**: BlackAndWhite, SixColors

These enums use `@JsonEnum` annotations with exact string mappings to match Rust `serde` serialization:
```dart
@JsonEnum(alwaysCreate: true)
enum DitheringMethod {
  @JsonValue('floyd-steinberg')
  floydSteinberg,
  // ...
}
```

**Extensions:**
- `ColorModeExtension`: Convert to JSON value, convert to DisplayType
- `DisplayTypeExtension`: Convert to JSON value, convert to ColorMode  
- `DitheringMethodExtension`: Convert to JSON value

#### `ws_messages.dart`
WebSocket protocol models for ESP32 device communication:
- **WsMessageType**: Enum for all WebSocket message types (type-safe string constants)
- **BoardConfig**: Device configuration (display size, orientation, battery, etc.)
- **WsErrorInfo**: Error responses
- **WsChunkAck**: Upload chunk acknowledgments
- **WsReadyInfo**: Session initialization
- **WsFinalResponse**: Upload completion response with success/failure status
- **WsMessageInfo**: General messages

**Note:** BoardConfig uses `DisplayType` enum for type-safe display configuration.

## Usage

### In Mobile/Desktop Applications

Add dependency in `pubspec.yaml`:
```yaml
dependencies:
  photoframe_common:
    path: ../photoframe_common
```

Import in Dart files:
```dart
import 'package:photoframe_common/photoframe_common.dart';

// Use models
final method = DitheringMethod.floydSteinberg;
final config = BoardConfig.fromJson(jsonData);
final displayType = DisplayType.sixColors;
```

### Code Generation

The package uses `json_serializable` for enum mappings. After modifying enums:

```bash
cd photoframe_common
dart run build_runner build --delete-conflicting-outputs
```

This generates `library_models.g.dart` with enum mapping tables.

## Architecture

```
photoframe_common/
├── lib/
│   ├── photoframe_common.dart    # Public exports
│   └── models/
│       ├── library_models.dart   # FFI enums
│       ├── library_models.g.dart # Generated enum maps
│       └── ws_messages.dart      # WebSocket protocol
└── pubspec.yaml
```

## Integration with Rust

The enums match Rust `photoframe_lib` types:

**Rust (photoframe_lib/src/types/mod.rs):**
```rust
#[repr(C)]
pub enum DitheringMethod {
    FloydSteinberg,    // → 'floyd-steinberg'
    Atkinson,          // → 'atkinson'
    // ...
}

#[repr(C)]
pub enum ColorMode {
    BlackAndWhite = 0,  // → 'black-and-white'
    SixColors = 1,      // → 'six-colors'
}
```

**Dart:**
```dart
DitheringMethod.floydSteinberg  // JSON: 'floyd-steinberg'
ColorMode.sixColors             // JSON: 'six-colors', FFI: 1
```

The `#[repr(C)]` and matching order ensures ABI compatibility for FFI calls.

## Version History

- **0.0.1** - Initial release with library_models and ws_messages

## Maintenance

When updating:
1. Keep enum values in sync with Rust library
2. Run code generation after enum changes
3. Update both mobile and desktop if protocol changes
4. Document breaking changes in this README
