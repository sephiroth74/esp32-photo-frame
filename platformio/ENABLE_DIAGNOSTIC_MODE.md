# How to Enable Display Diagnostic Mode

## Quick Setup (3 Steps)

### 1. Enable the Diagnostic Mode

Add this line to your board's config file (`platformio/include/config/feathers3_unexpectedmaker.h`):

```cpp
// Add this anywhere in the file (e.g., after line 79)
#define ENABLE_DISPLAY_DIAGNOSTIC 1
```

Or add it to the main `config.h` file to enable for all boards:

```cpp
// In platformio/include/config.h (around line 100)
#define ENABLE_DISPLAY_DIAGNOSTIC 1
```

### 2. Compile and Upload

```bash
pio run -t upload -e feathers3_unexpectedmaker
```

### 3. Run the Diagnostic

```bash
# Open serial monitor
pio device monitor -b 115200

# When you see this message:
# "Press 'd' within 5 seconds to run display diagnostics..."
#
# Type: d (lowercase only)
```

## What Happens

When diagnostic mode is triggered:
1. The display initializes using the proper driver (DESPI or Waveshare)
2. Runs power supply analysis
3. Shows color test patterns
4. Tests SPI communication
5. Reports results
6. Auto-restarts after 10 seconds

## Reading the Results

### ✅ Good Power Supply:
```
[DIAG] Idle voltage: 3.28V
[DIAG] Voltage during refresh: Min=3.15V, Max=3.30V
[DIAG] Voltage drop: 0.13V
```

### ❌ Power Problem (needs capacitor):
```
[DIAG] Idle voltage: 3.28V
[DIAG] Voltage during refresh: Min=2.85V, Max=3.30V
[DIAG] Voltage drop: 0.43V
[DIAG] WARNING: Significant voltage drop detected!
```

## Disable When Done

Comment out or remove the define when you're done testing:

```cpp
// #define ENABLE_DISPLAY_DIAGNOSTIC 1  // Disabled
```

## Alternative: Temporary Enable via Build Flags

You can also enable it temporarily without editing code:

```bash
# Enable just for this build
pio run -t upload -e feathers3_unexpectedmaker -D ENABLE_DISPLAY_DIAGNOSTIC=1
```

Or add to `platformio.ini` for your environment:

```ini
[env:feathers3_unexpectedmaker]
build_flags =
    ${common.build_flags}
    -D ENABLE_DISPLAY_DIAGNOSTIC=1  ; Remove this line when done testing
```

## Troubleshooting

- **No prompt appears**: Make sure you enabled the define and recompiled
- **Display doesn't respond**: Check your wiring connections
- **Serial monitor shows garbage**: Verify baud rate is 115200
- **Test crashes**: The display might be drawing too much power - add capacitor first

## Note

The diagnostic mode only compiles when `ENABLE_DISPLAY_DIAGNOSTIC` is defined.
When disabled, there's zero overhead - no extra code is included in your firmware.