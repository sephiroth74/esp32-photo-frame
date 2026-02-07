#!/bin/bash

# Temporarily modify PATH to prioritize Xcode tools over swiftly
export PATH="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/Applications/Xcode.app/Contents/Developer/usr/bin:/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin:$PATH"

# Verify clang is available
echo "Checking for clang..."
which clang

# Run build_runner
echo "Running build_runner..."
dart run build_runner build --delete-conflicting-outputs
