# Bluetooth Mode Diagnostic Suite

Comprehensive test suite for the Bluetooth Image Transfer feature.

## Overview

The BT diagnostic mode tests all components of the Bluetooth image transfer system without actually starting BLE advertising or requiring a client connection. This allows safe, automated testing of all functionality.

## Features Tested

### 1. **Protocol Validation** (`test_bt_protocol`)
- Valid configuration structure
- Magic number validation
- Version checking
- Rotation values (0-3)
- CRC16 calculation and verification
- Invalid config detection

### 2. **Preferences Storage** (`test_bt_preferences`)
- NVS read/write operations
- Rotation storage (0-3)
- First boot flag
- Image available flag
- Error code storage
- Retry counter
- Clear preferences

### 3. **Battery Management** (`test_battery_checking`)
- Real battery reading
- Critical state detection (< 10%)
- Low state detection (< 20%)
- Empty state detection
- Status checking function
- Simulated battery states

### 4. **Image Validation** (`test_image_size_validation`)
- Exact size checking (800×480 = 384KB)
- Size mismatch detection
- Off-by-one validation
- Bounds checking

### 5. **Error Classification** (`test_error_classification`)
- Severity levels (CRITICAL, WARNING, IGNORE)
- Display decision logic
- Fallback usage rules
- Error code ranges (301-369)

### 6. **SD Card Operations** (`test_sd_card_operations`)
- Card initialization
- Directory creation (`/bt_images`)
- File existence checks
- Fallback image detection
- Clean shutdown

### 7. **Display Initialization** (`test_display_initialization`)
- Buffer allocation (PSRAM)
- Display hardware init
- Power control (GPIO)
- Rotation support (0-3)
- State tracking

### 8. **Display Messages** (`test_display_messages`)
- Text rendering (centered)
- Canvas operations
- Font sizing
- Message display
- Clean rendering

### 9. **BLE Manager** (`test_ble_manager`)
- Object creation
- Initial state verification
- Config structure
- Error state
- Safe destruction (no advertising)

## Usage

### Enable Diagnostic Mode

1. Edit `include/config.h`:
```cpp
#define ENABLE_BT_IMAGE        // Enable BT mode
#define ENABLE_BT_DIAGNOSTIC   // Enable diagnostic tests
```

2. Build and upload:
```bash
pio run -t upload
pio device monitor
```

3. Watch serial output for test results

### Expected Output

```
╔════════════════════════════════════════╗
║   BLUETOOTH MODE DIAGNOSTIC SUITE     ║
╚════════════════════════════════════════╝

========================================
TEST: BT Protocol Validation
========================================
✓ PASS: Valid config should pass validation
✓ PASS: Invalid magic number should fail
✓ PASS: Rotation > 3 should fail
...

========================================
TEST SUMMARY
========================================
Tests Passed: 45
Tests Failed: 0
Total Tests:  45
Success Rate: 100.0%
========================================
✓ ALL TESTS PASSED!
```

## Test Macros

The diagnostic uses helper macros for consistent test reporting:

```cpp
TEST_START("Test Name")           // Begin test section
TEST_ASSERT(condition, "message") // Assert with auto pass/fail
TEST_END()                        // End test section
```

## Integration

The diagnostic mode integrates with the build system:

- When `ENABLE_BT_DIAGNOSTIC` is defined, `bt_diagnostic.cpp` provides its own `setup()` and `loop()`
- The main.cpp detects this and branches appropriately
- No BLE advertising is started, making it safe to run repeatedly
- All tests are non-destructive (except clearing test preferences)

## Disable for Production

To switch back to normal BT mode:

```cpp
#define ENABLE_BT_IMAGE        // Keep BT mode enabled
// #define ENABLE_BT_DIAGNOSTIC   // Comment out diagnostic
```

## Similar Tools

- `ENABLE_DISPLAY_DIAGNOSTIC` - Display hardware testing
- This follows the same pattern for BT testing

## Test Results

Each test reports:
- ✓ PASS - Test condition met
- ✗ FAIL - Test condition failed

Summary includes:
- Total tests run
- Pass/fail counts
- Success percentage
- Overall pass/fail status

## Troubleshooting

### SD Card Tests Fail
- Check SD card is inserted
- Verify SD pins in board config
- Check file system format (FAT32)

### Display Tests Fail
- Verify display connections
- Check power control GPIO
- Ensure display driver matches hardware

### Battery Tests Show Critical
- Charge battery before testing
- Check battery connection
- Verify battery voltage divider

### BLE Manager Test Fails
- Check ESP32 BLE is enabled in platformio.ini
- Verify BLE stack memory allocation
- May need to increase heap size

## Development

To add new tests:

1. Create test function in `bt_diagnostic.cpp`:
```cpp
void test_new_feature() {
    TEST_START("New Feature");
    
    // Your test code
    TEST_ASSERT(condition, "Description");
    
    TEST_END();
}
```

2. Add to test runner:
```cpp
void run_all_bt_tests() {
    // ... existing tests ...
    test_new_feature();
}
```

3. Update this README with new test description

## File Structure

```
include/
  bt_diagnostic.h       - Header with declarations
  config.h              - Enable flags

src/
  bt_diagnostic.cpp     - Test implementation
  main.cpp              - Integration point

docs/
  BT_DIAGNOSTIC.md      - This file
```

## Safety

The diagnostic mode:
- Does NOT start BLE advertising
- Does NOT accept connections
- Does NOT enter deep sleep
- Can be run repeatedly safely
- Halts after completion

This makes it ideal for CI/CD testing and development validation.
