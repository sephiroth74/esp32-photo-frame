#!/bin/bash
#!/bin/bash
#
# This script builds the Rust static library for iOS (device + simulator) and copies it to the Flutter iOS project.
#
# iOS Integration Instructions:
#
# 1. Make sure you have cbindgen installed (`cargo install cbindgen`).
# 2. Run this script to build the universal static library and copy it to ios/Runner/libs.
# 3. Generate the C header file for FFI:
#      cbindgen --lang c --output include/photoframe_lib.h
# 4. Copy the generated photoframe_lib.h to ios/Runner/libs (or a suitable include path).
# 5. In Xcode, add libphotoframe_lib.a to "Frameworks, Libraries, and Embedded Content" for the Runner target.
# 6. Add the header search path (e.g., $(SRCROOT)/Runner/libs) to "Header Search Paths" in Xcode build settings.
# 7. Import the header in your Runner-Bridging-Header.h:
#      #include "photoframe_lib.h"
# 8. You can now use the Rust FFI functions from Swift/Objective-C and expose them to Dart via dart:ffi.
# 9. IMPORTANT: To ensure Rust FFI symbols are visible to Flutter (dlsym), add the following to Xcode Build Settings → Other Linker Flags:
#      -force_load $(PROJECT_DIR)/Runner/libs/libphotoframe_lib.a
#    (Adjust the path if needed.)
#
# Note: If you change the Rust FFI interface, always regenerate the header and rebuild the library.

set -e

# Build Rust library for iOS (staticlib)
echo "Building photoframe-lib for iOS..."

cd "$(dirname "$0")/../../../rust/photoframe_lib"

# Install iOS targets if not already installed
rustup target add aarch64-apple-ios x86_64-apple-ios

# Build for device (aarch64) and simulator (x86_64)
cargo build --release --target aarch64-apple-ios
cargo build --release --target x86_64-apple-ios

# Create universal (fat) static library using lipo
IOS_TARGET_DIR="target/universal-ios"
mkdir -p "$IOS_TARGET_DIR"
lipo -create \
  target/aarch64-apple-ios/release/libphotoframe_lib.a \
  target/x86_64-apple-ios/release/libphotoframe_lib.a \
  -output "$IOS_TARGET_DIR/libphotoframe_lib.a"

echo "✅ Built universal libphotoframe_lib.a for iOS (device + simulator)"

# Copy to Flutter iOS project
IOS_LIBS="../../flutter/mobile/ios/libs"
mkdir -p "$IOS_LIBS"
cp "$IOS_TARGET_DIR/libphotoframe_lib.a" "$IOS_LIBS/"

echo "✅ Copied libphotoframe_lib.a to ios/libs/"

echo "➡️  Remember to add the static library and headers to your Xcode project if not already done."

cbindgen --lang c --output include/photoframe_lib.h

echo "✅ Generated photoframe_lib.h using cbindgen"

cp include/photoframe_lib.h "$IOS_LIBS/"

echo "✅ Copied photoframe_lib.h to ios/libs/"

rm include/photoframe_lib.h

echo "\n================ iOS Integration Instructions ================"
echo "1. Make sure you have cbindgen installed (cargo install cbindgen)."
echo "2. Run this script to build the universal static library and copy it to ios/libs."
echo "3. Generate the C header file for FFI:"
echo "     cbindgen --lang c --output include/photoframe_lib.h"
echo "4. Copy the generated photoframe_lib.h to ios/libs (or a suitable include path)."
echo "5. In Xcode, add libphotoframe_lib.a to 'Frameworks, Libraries, and Embedded Content' for the Runner target."
echo "6. Add the header search path (e.g., /ios/libs) to 'Header Search Paths' in Xcode build settings."
echo "7. Import the header in your Runner-Bridging-Header.h:"
echo "     #include \"photoframe_lib.h\""
echo "8. You can now use the Rust FFI functions from Swift/Objective-C and expose them to Dart via dart:ffi."
echo "============================================================\n"
