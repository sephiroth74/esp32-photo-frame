# Bluetooth Mode Factory Reset

## Overview

The factory reset feature allows users to completely clear all Bluetooth mode state and start fresh. This is useful for troubleshooting, reconfiguration, or preparing the device for a new setup.

## How to Trigger Factory Reset

### Method: GPIO1 Long Press (5 seconds)

1. **Power on** or wake the device in Bluetooth mode
2. **Hold GPIO1 button** continuously for 5 seconds
3. **Wait for confirmation** - display shows "Factory Reset Completato!"
4. **Device automatically restarts** in first boot state

**Important**: The button must be held continuously for the full 5 seconds. Releasing early will cancel the reset.

## What Gets Reset

### 1. Bluetooth Preferences (NVS)
All BT-specific preferences are cleared:
- `bt_rotation` → 0 (landscape)
- `bt_first_boot` → true (enables 30-minute timeout)
- `bt_image_available` → false
- `bt_last_error` → 0 (no error)
- `bt_retry_count` → 0

### 2. Saved Image File
- `/bt_images/last.bin` is deleted from SD card
- This forces the device to wait for a new image via BLE

### 3. Device State
- Device restarts automatically after reset
- Next boot will be treated as **first boot** with 30-minute timeout
- BLE will start advertising and wait for new image

## What Does NOT Get Reset

- **WiFi credentials** (not used in BT mode anyway)
- **SD card TOC cache** (for normal mode operations)
- **Display settings** from config.json
- **Other modes' data** (Google Drive, SD card image mode)

## Visual Feedback

### During Reset (Button Monitoring)
```
[BT] Button pressed, monitoring for 5000 ms long press...
[BT] Button held... 4000 ms remaining
[BT] Button held... 3000 ms remaining
[BT] Button held... 2000 ms remaining
[BT] Button held... 1000 ms remaining
[BT] Factory reset button held for 5000 ms - TRIGGERING RESET
```

### On Display (After Reset)
```
╔════════════════════════════════════╗
║                                    ║
║         Factory Reset              ║
║                                    ║
║          Completato!               ║
║                                    ║
║  Tutte le impostazioni sono        ║
║     state ripristinate             ║
║                                    ║
║         Riavvio...                 ║
║                                    ║
╚════════════════════════════════════╝
```

Message displays for 3 seconds before automatic restart.

## Implementation Details

### Functions

#### `checkFactoryResetButton(gpio_num_t button_pin, uint32_t press_duration_ms)`
**Location**: `src/bt_utils.cpp`

Monitors GPIO button for continuous press:
- Configures pin with INPUT_PULLUP
- Polls button state every 100ms
- Logs progress every 1 second
- Returns `true` if held for full duration, `false` if released early

**Parameters**:
- `button_pin`: GPIO pin to monitor (default: WAKEUP_PIN/GPIO1)
- `press_duration_ms`: Required hold time in milliseconds (default: 5000)

**Returns**: 
- `true`: Factory reset should be performed
- `false`: Button not pressed or released early

#### `performFactoryReset()`
**Location**: `src/bt_utils.cpp`

Executes the factory reset sequence:
1. **Clear Preferences**: All 5 BT preferences set to default values
2. **Delete Image**: Remove `/bt_images/last.bin` from SD card
3. **Show Confirmation**: Render reset complete message on display
4. **Restart Device**: Call `ESP.restart()`

**Never returns** - device restarts after completion.

### Integration Points

#### main.cpp - setup_bluetooth_mode()
```cpp
// Check for factory reset button press (5 second long press on WAKEUP_PIN)
// This must be checked early, before any other operations
log_i("[BT] Checking for factory reset button...");
if (photo_frame::bt_utils::checkFactoryResetButton(WAKEUP_PIN, 5000)) {
    log_i("[BT] Factory reset triggered!");
    photo_frame::bt_utils::performFactoryReset();
    // Never returns - device will restart
}
log_i("[BT] No factory reset requested");
```

The check is performed **early** in `setup_bluetooth_mode()`, right after determining wakeup reason and before battery checks. This ensures users can trigger reset even in error states.

## Diagnostic Testing

### Test Suite: test_factory_reset_operations()
**Location**: `src/bt_diagnostic.cpp`

The diagnostic suite includes comprehensive factory reset testing:

**Test 10.1-10.2**: Set and verify non-default preferences  
**Test 10.3-10.4**: Clear preferences and verify reset to defaults  
**Test 10.5**: SD card file creation and deletion  
**Test 10.6**: GPIO button pin configuration and readability  

**Safety Notes**:
- Tests do NOT call `performFactoryReset()` (would restart device)
- Tests do NOT call `checkFactoryResetButton()` with real timing (would wait 5s)
- Tests simulate the operations without side effects

## Use Cases

### 1. Corrupted State Recovery
If BT preferences become corrupted or device is in an inconsistent state:
- Factory reset clears all state
- Device restarts cleanly in first boot mode

### 2. Reconfiguration
To change rotation or other BT settings:
- Factory reset clears old settings
- Send new configuration via BLE on next connection

### 3. Testing
During development or troubleshooting:
- Quick way to reset to known state
- No need to reflash firmware or manually clear preferences

### 4. Prepare for New Owner
Before giving device to someone else:
- Factory reset removes previous image
- Clears all previous configuration
- Device ready for fresh setup

## Error Handling

### SD Card Unavailable
If SD card fails to initialize during reset:
```
⚠ SD card not available, skipping file deletion
```
Preferences are still cleared, but file deletion is skipped. This is safe as missing SD card will be handled on next boot anyway.

### Display Initialization Failed
If display fails to initialize during confirmation:
```
[BT] Failed to init display for reset confirmation
```
Reset still proceeds (preferences cleared, file deleted, device restarted), but confirmation message is not shown. User will see device restart behavior instead.

### Partial Reset
All reset operations are independent:
1. Preference clearing uses PreferencesHelper (always available)
2. File deletion only if SD card is available
3. Display confirmation only if display initializes
4. Restart always happens (ESP.restart() is reliable)

Even if some steps fail, device will restart and attempt normal BT mode operation.

## Pin Configuration

**GPIO1** (WAKEUP_PIN) is used for factory reset on FeatherS3:
- Configured in `include/config/feathers3_unexpectedmaker.h`
- Uses internal pull-up resistor (INPUT_PULLUP)
- Active LOW (button press pulls to ground)
- Same pin used for deep sleep wakeup

**Hardware Requirements**:
- Button connected between GPIO1 and GND
- No external pull-up needed (internal pull-up used)
- Momentary push button recommended

## Timing Considerations

### 5 Second Hold Time
Chosen to balance:
- **Accidental activation prevention**: 5s is long enough to avoid accidental resets
- **User patience**: Not so long that users give up waiting
- **Feedback frequency**: 1-second logging provides clear progress

### Polling Interval (100ms)
- Fast enough to detect button release quickly
- Slow enough to avoid excessive CPU usage
- Good balance for responsive UX

### Confirmation Display (3 seconds)
- Long enough to read the message
- Short enough to not delay restart unnecessarily
- User has visual confirmation before device restarts

## Security Considerations

**No Authentication Required**: Factory reset can be triggered by anyone with physical access to the device and knowledge of the button combination.

**Physical Access Required**: Reset requires holding GPIO1 button for 5 seconds, which requires physical access to the device.

**Data Loss**: All BT state and saved images are permanently deleted. This is intentional but users should be aware.

**No Undo**: Once triggered, factory reset cannot be stopped or undone. The 5-second hold time is the only safeguard.

## Future Enhancements

Potential improvements for future versions:

1. **Configurable Hold Time**: Allow adjustment via config.json (e.g., 3-10 seconds)
2. **Multi-Button Combination**: Require holding multiple buttons to prevent accidental resets
3. **Confirmation Prompt**: Show "Hold to confirm" message before starting reset
4. **Partial Reset Options**: Different hold times for different reset levels
5. **Reset Counter**: Track number of factory resets in preferences for diagnostics
6. **Reset Log**: Write reset timestamp to SD card for troubleshooting

## Troubleshooting

### Factory Reset Not Triggering

**Symptom**: Holding GPIO1 doesn't trigger reset

**Possible Causes**:
1. **Button not connected properly** - Check wiring
2. **Holding too short** - Must hold for full 5 seconds
3. **Released early** - Don't release until "TRIGGERING RESET" log appears
4. **Wrong GPIO pin** - Verify WAKEUP_PIN definition in board config
5. **Pin pulled HIGH externally** - Check for conflicting external pull-up

**Debug Steps**:
1. Enable serial monitoring to see button detection logs
2. Check pin state: `pinMode(GPIO1, INPUT_PULLUP); Serial.println(digitalRead(GPIO1));`
3. Verify continuous LOW state when button pressed
4. Look for "Button pressed, monitoring..." message

### Display Doesn't Show Confirmation

**Symptom**: Reset happens but no confirmation on display

**Possible Causes**:
1. **Display not initialized** - Check display power and connections
2. **Display failed to initialize** - Check serial logs for init errors
3. **SD card conflict** - Display may fail if SD card is still active

**This is OK**: Reset still works correctly even without visual confirmation. The device will restart, which is the important part.

### Preferences Not Clearing

**Symptom**: BT state persists after factory reset

**Possible Causes**:
1. **NVS corruption** - May need to erase flash completely
2. **Wrong namespace** - Verify PreferencesHelper is using correct namespace
3. **Reset not actually triggered** - Check serial logs to confirm reset ran

**Solution**: Use PlatformIO erase flash command:
```bash
pio run -t erase
pio run -t upload
```

---

Last Updated: 2025-01-19
