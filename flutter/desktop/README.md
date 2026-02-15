# Photo Frame Processor - Flutter macOS

Native macOS GUI for ESP32 Photo Frame image processor, built with Flutter and [appkit_ui_elements](https://github.com/sephiroth74/appkit_ui_elements).

## ✨ Features

- **Native macOS Interface**: Uses AppKit UI Elements for a native user experience
- **Theme System**: Full support for automatic light/dark mode
- **Real-time Progress**: Progress monitoring via JSON output
- **Configuration Persistence**: Automatic settings save
- **Rust Integration**: Direct execution of the rust `processor` binary with embedded AI

## 📋 Requirements

- Flutter SDK 3.10.3 or higher
- macOS 11.0 (Big Sur) or higher
- Xcode 13 or higher
- Rust processor compiled with AI features
- [ImageMagick](https://imagemagick.org/) (recommended)

## 🚀 Installation

```bash
# Clone the repository (if you haven't already)
cd /path/to/esp32-photo-frame

# Install Flutter dependencies
cd flutter/desktop
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

### Rust Binary Integration

The provider searches for the `processor` binary in these locations:
1. `../rust/processor/target/release/processor` (relative)
2. Absolute path in the project
3. System PATH
4. Custom path configured in the UI

## 🐛 Troubleshooting

### Build fails
Make sure you've run code generation:
```bash
flutter pub run build_runner build --delete-conflicting-outputs
```

### Binary not found
Verify that the Rust binary is compiled:
```bash
cd ../rust/processor
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

## 📄 License

This project is part of the ESP32 Photo Frame project and is released under the GNU General Public License v3 (GPL-3.0).

## 👤 Author

Alessandro Crugnola

---
