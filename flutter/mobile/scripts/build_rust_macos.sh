#!/bin/bash
set -e

# Build Rust library for macOS development/testing
echo "Building photoframe-lib for macOS..."

cd "$(dirname "$0")/../../../rust/photoframe-lib"

# Build for macOS (x86_64 and arm64)
cargo build --release

# Copy library to Flutter assets for macOS development
MACOS_LIB_DIR="../../../flutter/mobile/macos/Frameworks"
mkdir -p "$MACOS_LIB_DIR"

cp target/release/libphotoframe_lib.dylib "$MACOS_LIB_DIR/"

echo "✅ Built libphotoframe_lib.dylib for macOS"
