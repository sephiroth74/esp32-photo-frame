# Complete Display Troubleshooting Guide for ESP32 Photo Frame

## Table of Contents
1. [Problem Symptoms](#problem-symptoms)
2. [Quick Diagnosis](#quick-diagnosis)
3. [Debug Mode Setup](#debug-mode-setup)
4. [Hardware Solutions](#hardware-solutions)
5. [Software Solutions](#software-solutions)
6. [Test Procedures](#test-procedures)
7. [Common Issues & Fixes](#common-issues--fixes)

---

## Problem Symptoms

### Washed-Out Display
- **Description**: Colors appear faded, especially on 6-color images
- **Occurrence**: May be intermittent (worse in mornings, with portraits)
- **Root Causes**: Power delivery, BUSY pin timeout, data line issues

### Busy Timeout Errors
- **Console Output**: `Busy Timeout!` after 20+ seconds
- **Display State**: Incomplete refresh, washed-out colors
- **Root Cause**: BUSY pin connection or timeout settings

---

## Quick Diagnosis

### Use Debug Mode First!

1. **Enable Debug Mode** in `platformio/include/config.h` (line 98):
```cpp
#define ENABLE_DISPLAY_DIAGNOSTIC 1
```

2. **Compile and Upload**:
```bash
cd platformio
pio run -t upload -e feathers3_unexpectedmaker
pio device monitor -b 115200
```

3. **Run Tests** - The system boots directly into debug mode with menu:
```
========================================
Display Debug Menu
========================================
1. Run Full Diagnostic Suite  ← Start here!
2. Color Pattern Test
3. Power Supply Test
4. Data Line Voltage Test
5. BUSY Pin Test
6. SPI Communication Test
7. Refresh Timing Test
8. Show Test Results Summary
9. Clear Display
0. Restart ESP32
```

### Interpreting Test Results

| Test Result | Issue | Solution |
|-------------|-------|----------|
| Voltage drop > 0.3V | Power delivery problem | Add capacitor |
| BUSY stuck or timeouts | BUSY pin connection | Check wire, add pull-up |
| Data lines not 3.3V | Signal integrity | Shorter wires |
| Refresh > 20 seconds | Timeout too short | Modify library |

---

## Hardware Solutions

### 1. Add Capacitors (Most Common Fix)

**Components Needed:**
- 1x 100-470µF electrolytic capacitor (16V rated)
- 1x 0.1µF ceramic capacitor (optional but recommended)

**Installation:**
```
Display Power Pins:
    [3.3V]───┬───[+]───┬──── Display VCC
             │   ╱ ╲   │
             │  │Big│  │
             │  │Cap│  ┴ Small
             │   ╲ ╱   ┬ Cap (0.1µF)
             │   [─]   │
    [GND]────┴─────────┴──── Display GND
```

**Important:**
- Electrolytic capacitors are **polarized** - long leg to positive!
- Place as close as possible to display power pins
- The stripe on the capacitor marks the negative side

### 2. Fix BUSY Pin Connection

**Add Pull-Up Resistor:**
```
BUSY (GPIO 6) ──[10kΩ]──→ 3.3V
```

**Check Connections:**
```
Display → FeatherS3
-------------------
BUSY    → GPIO 6 (A4)  ← Most likely problem!
RST     → GPIO 5 (A5)
DC      → GPIO 10
CS      → GPIO 38
SCK     → GPIO 36
MOSI    → GPIO 35
```

### 3. Improve Data Line Integrity

**Use Shortest Possible Wires:**
- Replace 6-12 inch wires with 2-3 inch wires
- Especially critical: SCK and MOSI lines

**Add Series Resistors (Optional):**
```
ESP32 GPIO ──[100Ω]──→ Display Pin
```

**Multiple Ground Connections:**
```
Display GND ←── Wire 1 ──→ ESP32 GND
Display GND ←── Wire 2 ──→ ESP32 GND
Display GND ←── Wire 3 ──→ ESP32 GND
```

### 4. Power Supply Improvements

**Better 3.3V Connection:**
- Use 3V3.2 pin instead of 3V3.1 on FeatherS3
- Connect power directly, not through breadboard rails

**Ensure Adequate Current:**
- USB source should provide at least 500mA
- Use powered USB hub or wall adapter
- Avoid long/thin USB cables

---

## Software Solutions

### 1. Enable setPaged() Workaround (Already Applied)

The code now includes this fix for GDEP073E01 displays:

```cpp
#ifdef DISP_6C
    display.epd2.setPaged();  // Special mode for GDEP073E01
#endif
```

This helps with incomplete refresh issues specific to this display model.

### 2. Increase Timeout in Library

If you still see "Busy Timeout!" errors after hardware fixes:

**Edit:** `platformio/.pio/libdeps/feathers3_unexpectedmaker/GxEPD2/src/epd7c/GxEPD2_730c_GDEP073E01.cpp`

**Line 17:** Change timeout from 20 to 30 seconds:
```cpp
// Original (20 seconds):
GxEPD2_EPD(cs, dc, rst, busy, LOW, 20000000, WIDTH, HEIGHT, ...

// Change to (30 seconds):
GxEPD2_EPD(cs, dc, rst, busy, LOW, 30000000, WIDTH, HEIGHT, ...
```

**Note:** Value is in microseconds (30000000 = 30 seconds)

---

## Test Procedures

### Manual Voltage Test

With multimeter, check during display operation:

1. **Idle Voltage**: Should be stable 3.3V
2. **During Refresh**: Should not drop below 3.0V
3. **Data Pins HIGH**: All should read 3.3V

### BUSY Pin Test

```cpp
// Quick test code (add to setup):
pinMode(EPD_BUSY_PIN, INPUT);
for(int i = 0; i < 10; i++) {
    Serial.println(digitalRead(EPD_BUSY_PIN));
    delay(100);
}
// Should show variation, not stuck at 0 or 1
```

### Refresh Timing Test

Monitor actual refresh times in debug mode:
- Good: 15-17 seconds
- Warning: 18-20 seconds (close to timeout)
- Bad: >20 seconds (timeout occurring)

---

## Common Issues & Fixes

### Issue 1: Intermittent Washout (Morning/Evening)

**Cause:** Temperature/humidity affecting breadboard resistance

**Solutions:**
1. Shorten all wires to minimum length
2. Add capacitors for power stability
3. Move to soldered connections

### Issue 2: Only Color Images Affected

**Cause:** Higher current draw during color refresh

**Solutions:**
1. Add 470µF capacitor (larger than for B&W)
2. Use multiple power/ground wires
3. Ensure adequate USB power supply

### Issue 3: Portrait Images Worse

**Cause:** Complex color patterns in combined portraits

**Solutions:**
1. Enable setPaged() mode (already in code)
2. Increase timeout to 30 seconds
3. Optimize image complexity

### Issue 4: Busy Timeout Persists

**Cause:** BUSY pin not properly connected

**Solutions:**
1. Check physical connection at both ends
2. Add 10kΩ pull-up resistor
3. Try different breadboard row
4. Replace jumper wire

---

## Recommended Fix Order

1. **Run Debug Mode** - Identify specific issue
2. **Add Capacitors** - Fixes most power-related washout
3. **Fix BUSY Pin** - Add pull-up, check connection
4. **Shorten Wires** - Improve signal integrity
5. **Increase Timeout** - If still timing out
6. **Solder Connections** - Final solution for persistent issues

---

## Debug Mode Commands Reference

| Command | Function | What to Look For |
|---------|----------|-------------------|
| 1 | Full Diagnostic | Complete system analysis |
| 2 | Color Pattern | Check for washout visually |
| 3 | Power Supply | Voltage drop measurements |
| 4 | Data Lines | Requires multimeter check |
| 5 | BUSY Pin | Connection and response |
| 6 | SPI Test | Communication integrity |
| 7 | Timing Test | Refresh duration |
| 8 | Results | Summary and diagnosis |

---

## When to Move Beyond Breadboard

Consider permanent assembly if:
- Fixes work temporarily but issues return
- You've confirmed the solution with breadboard
- You want long-term reliability

**Permanent Assembly Options:**
1. Perfboard with soldered connections
2. Custom PCB design
3. Pre-made e-paper driver boards
4. Direct cable soldering with connectors

---

## Disable Debug Mode

When testing is complete:

1. **Comment out** in `config.h`:
```cpp
// #define ENABLE_DISPLAY_DIAGNOSTIC 1
```

2. **Recompile and upload** to return to normal operation

---

## Quick Reference Card

### Your Current Setup (FeatherS3 + GDEP073E01)

**Implemented Fixes:**
- ✅ setPaged() workaround enabled
- ✅ 10kΩ pull-up on BUSY pin
- ✅ Using 3V3.2 pin for power

**Recommended Additional Fixes:**
- 🔧 Add 220-470µF capacitor
- 🔧 Shorten all wires to <3 inches
- 🔧 Increase timeout to 30 seconds if needed

**Test Results to Expect:**
- Voltage drop: Should be <0.2V with capacitor
- BUSY pin: Should toggle, not stuck
- Refresh time: 15-17 seconds typical
- No "Busy Timeout!" errors

---

## Need More Help?

If issues persist after all fixes:
1. Check display ribbon cable for damage
2. Verify ESP32 board isn't defective
3. Test with different power supply
4. Consider display panel may be faulty

Remember: The washout is almost always a power or connection issue, rarely a code problem!