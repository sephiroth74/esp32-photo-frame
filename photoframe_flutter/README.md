# Photo Frame Processor - Flutter macOS

Native macOS GUI for ESP32 Photo Frame image processor, built with Flutter and [appkit_ui_elements](https://github.com/sephiroth74/appkit_ui_elements).

## ✨ Features

- **Native macOS Interface**: Uses AppKit UI Elements for a native user experience
- **Theme System**: Full support for automatic light/dark mode
- **Real-time Progress**: Progress monitoring via JSON output
- **Configuration Persistence**: Automatic settings save
- **Rust Integration**: Direct execution of `photoframe-processor` binary

## 📋 Requirements

- Flutter SDK 3.10.3 or higher
- macOS 11.0 (Big Sur) or higher
- Xcode 13 or higher
- Rust photoframe-processor compiled (v1.1.0+)

## 🚀 Installation

```bash
# Clone the repository (if you haven't already)
cd /path/to/esp32-photo-frame

# Install Flutter dependencies
cd photoframe_flutter
flutter pub get

# Generate JSON serialization code
flutter pub run build_runner build
```

## 🛠️ Building

### Development
```bash
flutter run -d macos
```

### Release
```bash
flutter build macos --release
```

The compiled binary will be available at:
```
build/macos/Build/Products/Release/photoframe_flutter.app
```

## 📦 Project Structure

```
photoframe_flutter/
├── lib/
│   ├── main.dart                      # Entry point with AppKitApp
│   ├── models/
│   │   └── processing_config.dart     # Configuration model
│   ├── providers/
│   │   └── processing_provider.dart   # State management with Provider
│   ├── screens/
│   │   └── home_screen.dart           # Main screen
│   └── services/
│       ├── font_service.dart          # System font enumeration
│       └── window_preferences.dart    # Window size persistence
├── pubspec.yaml                       # Flutter dependencies
└── README.md                          # This file
```

## 🎨 User Interface

The interface uses the following AppKit components:

- **AppKitScaffold**: Main structure with toolbar
- **AppKitGroupBox**: Control groups with title
- **AppKitButton**: Native macOS buttons
- **AppKitPopupButton**: Dropdown menus
- **AppKitCheckbox**: Native checkboxes
- **AppKitSlider**: Native sliders
- **AppKitProgressBar**: Progress bar
- **AppKitTextField**: Native text fields
- **AppKitDialog**: Modal dialogs

### Interface Sections

1. **File Selection**
   - Input and Output directories with native file picker

2. **Display Settings**
   - Display Type (Black & White, 6-Color, 7-Color)
   - Target Orientation (Landscape, Portrait)

3. **Dithering Settings**
   - Auto-optimize checkbox
   - Dither method popup
   - Strength and Contrast sliders (when auto-optimize is off)

4. **Output Formats**
   - Multiple checkboxes for BMP, BIN, JPEG, PNG

5. **People Detection** (AI)
   - Enable/disable people detection
   - Python script and interpreter paths
   - Confidence threshold slider

6. **Annotation Settings**
   - Enable/disable annotation
   - System font selector (native font enumeration)
   - Font size and background color

7. **Divider Settings**
   - Divider width and color for portrait image pairing

8. **Advanced Options**
   - Force overwrite
   - Dry run mode
   - Debug mode
   - Generate processing report
   - Parallel jobs
   - File extensions filter

9. **Processor Binary**
   - Custom binary path selection
   - Automatic fallback to common locations

10. **Process Button & Progress**
    - Large "Process Images" button
    - Modal progress dialog with real-time updates
    - Success/error messages

## 🔧 Configuration

### Configuration Persistence

Configuration is automatically saved to:
```
~/Library/Application Support/it.sephiroth.photoframeFlutter/config.json
```

Includes all parameters:
- Input/output paths
- Display type and orientation
- Dithering settings
- Output formats
- People detection settings
- Annotation settings
- Divider settings
- Advanced options
- Processor binary path

### Window Persistence

Window size and position are automatically saved to:
```
~/Library/Application Support/it.sephiroth.photoframeFlutter/window_preferences.json
```

### Rust Binary Integration

The provider searches for the `photoframe-processor` binary in these locations:
1. `../rust/photoframe-processor/target/release/photoframe-processor` (relative)
2. Absolute path in the project
3. System PATH
4. Custom path configured in the UI

## 📝 Main Dependencies

```yaml
dependencies:
  appkit_ui_elements: ^0.2.2      # Native macOS UI components
  provider: ^6.1.2                # State management
  file_picker: ^10.3.8            # Native file/directory picker
  path_provider: ^2.1.5           # System paths
  json_annotation: ^4.9.0         # JSON serialization
  process_run: ^1.2.1             # Process execution
  window_manager: ^0.5.0          # Window management
```

## 🐛 Troubleshooting

### Build fails
Make sure you've run code generation:
```bash
flutter pub run build_runner build --delete-conflicting-outputs
```

### Binary not found
Verify that the Rust binary is compiled:
```bash
cd ../rust/photoframe-processor
cargo build --release
```

### AppKit UI doesn't appear correctly
Verify that the appkit_ui_elements version is correct:
```bash
flutter pub outdated
flutter pub upgrade appkit_ui_elements
```

### File picker error
Ensure entitlements are properly configured:
- `com.apple.security.files.user-selected.read-only`
- `com.apple.security.files.user-selected.read-write`

These are already configured in `DebugProfile.entitlements` and `Release.entitlements`.

## 📚 References

- [Flutter Documentation](https://docs.flutter.dev/)
- [appkit_ui_elements](https://github.com/sephiroth74/appkit_ui_elements)
- [Provider Package](https://pub.dev/packages/provider)
- [ESP32 Photo Frame Processor](https://github.com/sephiroth74/arduino/tree/main/esp32-photo-frame)

## 📄 License

This project is part of the ESP32 Photo Frame project and is released under the MIT license.

## 👤 Author

Alessandro Crugnola

## 🤝 Contributing

Contributions, issues and feature requests are welcome!

---

**Note**: This Flutter app replaces the previous Rust GUI (photoframe-processor-gui). All features have been implemented with native macOS components for a better user experience.
