# Fix for Display Busy Timeout & Washout Issue

## Problem
Your display shows "Busy Timeout!" errors, causing washed-out colors. The BUSY pin connection is faulty.

## Quick Fix Steps:

### 1. Check BUSY Pin Connection
The BUSY pin (GPIO 6 on FeatherS3) is critical for display synchronization.

**On your breadboard:**
- Display BUSY pin → ESP32 GPIO 6 (pin A4)
- This wire is likely loose or disconnected

### 2. Test the Connection
Add this test to verify BUSY pin is working:

```cpp
// Add to your setup() after init_display():
Serial.println("Testing BUSY pin...");
pinMode(EPD_BUSY_PIN, INPUT);
int busyState = digitalRead(EPD_BUSY_PIN);
Serial.printf("BUSY pin state: %d (should toggle during refresh)\n", busyState);
```

### 3. Physical Connection Fixes:

**Immediate fixes to try:**
1. **Reseat the BUSY wire** - Remove and reconnect firmly
2. **Use a different breadboard row** - Current row might have poor contact
3. **Replace the jumper wire** - Wire might be damaged
4. **Add pull-up resistor** - 10kΩ between BUSY and 3.3V

### 4. Wiring Checklist

Verify ALL display connections:
```
Display → FeatherS3
-------------------
VCC     → 3.3V (with capacitor!)
GND     → GND (multiple wires recommended)
BUSY    → GPIO 6 (A4) ← THIS IS YOUR PROBLEM
RST     → GPIO 5 (A5)
DC      → GPIO 10
CS      → GPIO 38
SCK     → GPIO 36
MOSI    → GPIO 35
```

### 5. Software Workaround (Temporary)

If you can't fix the hardware immediately, try increasing the timeout:

In `platformio/include/config/feathers3_unexpectedmaker.h`, add:
```cpp
// Increase busy timeout for breadboard testing
#define EPD_BUSY_TIMEOUT 30000  // 30 seconds instead of 20
```

### 6. Better Diagnostic

Add this enhanced BUSY pin test:

```cpp
void testBusyPin() {
    Serial.println("=== BUSY Pin Diagnostic ===");
    pinMode(EPD_BUSY_PIN, INPUT);

    // Check if pin is stuck
    int readings[10];
    for(int i = 0; i < 10; i++) {
        readings[i] = digitalRead(EPD_BUSY_PIN);
        delay(100);
    }

    // Check for variation
    bool stuck = true;
    for(int i = 1; i < 10; i++) {
        if(readings[i] != readings[0]) {
            stuck = false;
            break;
        }
    }

    if(stuck) {
        Serial.printf("ERROR: BUSY pin stuck at %d - CHECK CONNECTION!\n", readings[0]);
    } else {
        Serial.println("BUSY pin appears responsive");
    }
}
```

## Why This Causes Washout:

1. Display starts color refresh cycle
2. ESP32 waits for BUSY pin to signal completion
3. BUSY never signals (bad connection)
4. ESP32 times out after 20 seconds
5. Display is left in incomplete state = washed out colors

## The Capacitor Still Helps:

Even though voltage looks stable, add the capacitor anyway:
- 220-470µF across display power pins
- Will help once BUSY pin is fixed

## Expected Results After Fix:

✅ No more "Busy Timeout!" messages
✅ Refresh time ~15 seconds (not 20+)
✅ Vibrant, correct colors
✅ No washout effect

## If Still Not Working:

1. **Try pull-up resistor**: 10kΩ from BUSY to 3.3V
2. **Check display ribbon cable** - Might be damaged
3. **Test with different GPIO** - Change BUSY to GPIO 1 or 2
4. **Reduce SPI speed** - Interference might affect BUSY signal