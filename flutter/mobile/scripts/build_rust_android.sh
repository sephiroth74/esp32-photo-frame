#!/bin/bash
set -e

# Build Rust library for Android
echo "Building photoframe-lib for Android..."

cd "$(dirname "$0")/../../../rust/photoframe-lib"

# Install targets if not already installed
rustup target add aarch64-linux-android armv7-linux-androideabi i686-linux-android x86_64-linux-android

# Auto-detect NDK if ANDROID_NDK_HOME is not set
if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "ANDROID_NDK_HOME not set, attempting to auto-detect NDK..."
    NDK_BASE="$HOME/Library/Android/sdk/ndk"
    
    if [ -d "$NDK_BASE" ]; then
        # Find the most recent NDK version
        LATEST_NDK=$(ls -1 "$NDK_BASE" | grep -E '^[0-9]+\.[0-9]+\.[0-9]+' | sort -V | tail -1)
        if [ -n "$LATEST_NDK" ]; then
            export ANDROID_NDK_HOME="$NDK_BASE/$LATEST_NDK"
            echo "✅ Using NDK: $ANDROID_NDK_HOME"
        else
            echo "❌ Error: No NDK found in $NDK_BASE"
            echo "Please install Android NDK or set ANDROID_NDK_HOME manually"
            exit 1
        fi
    else
        echo "❌ Error: NDK directory not found at $NDK_BASE"
        echo "Please install Android NDK or set ANDROID_NDK_HOME manually"
        exit 1
    fi
else
    echo "Using ANDROID_NDK_HOME: $ANDROID_NDK_HOME"
fi

# Configure cargo to use NDK linkers
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android21-clang"
export CARGO_TARGET_ARMV7_LINUX_ANDROIDEABI_LINKER="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/armv7a-linux-androideabi21-clang"
export CARGO_TARGET_I686_LINUX_ANDROID_LINKER="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/i686-linux-android21-clang"
export CARGO_TARGET_X86_64_LINUX_ANDROID_LINKER="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/darwin-x86_64/bin/x86_64-linux-android21-clang"

# Build for all Android architectures
cargo build --release --target aarch64-linux-android
cargo build --release --target armv7-linux-androideabi
cargo build --release --target i686-linux-android
cargo build --release --target x86_64-linux-android

# Copy libraries to Flutter Android jniLibs
JNI_LIBS="../../flutter/mobile/android/app/src/main/jniLibs"
mkdir -p "$JNI_LIBS/arm64-v8a"
mkdir -p "$JNI_LIBS/armeabi-v7a"
mkdir -p "$JNI_LIBS/x86"
mkdir -p "$JNI_LIBS/x86_64"

cp target/aarch64-linux-android/release/libphotoframe_lib.so "$JNI_LIBS/arm64-v8a/"
cp target/armv7-linux-androideabi/release/libphotoframe_lib.so "$JNI_LIBS/armeabi-v7a/"
cp target/i686-linux-android/release/libphotoframe_lib.so "$JNI_LIBS/x86/"
cp target/x86_64-linux-android/release/libphotoframe_lib.so "$JNI_LIBS/x86_64/"

echo "✅ Built and copied libphotoframe_lib.so for all Android architectures"
