# How to Run the Display Diagnostic

## Option 1: Quick Test (Add to your existing setup)

Add these lines to your `main.cpp` file:

### Step 1: Add the include at the top of main.cpp
```cpp
// Add this with your other includes (around line 36)
#include "display_diagnostic.h"
```

### Step 2: Add diagnostic trigger in setup()
Add this code in your `setup()` function, right after the Serial initialization (around line 225):

```cpp
void setup()
{
    // ... existing initialization code ...

    Serial.print(F("Wakeup reason: "));
    Serial.print(wakeup_reason_string);
    // ... etc ...

    // ADD THIS: Check for diagnostic mode trigger
    Serial.println(F("\n================================="));
    Serial.println(F("Press 'D' within 5 seconds to run display diagnostics..."));
    Serial.println(F("=================================\n"));

    unsigned long diagnostic_wait = millis();
    bool run_diagnostic = false;

    while(millis() - diagnostic_wait < 5000) {
        if(Serial.available()) {
            char c = Serial.read();
            if(c == 'D' || c == 'd') {
                run_diagnostic = true;
                break;
            }
        }
    }

    if(run_diagnostic) {
        Serial.println(F("Starting display diagnostics..."));
        display.init(115200, true, 2, false);  // Initialize display first
        display_diagnostic::runFullDiagnostic();

        Serial.println(F("Diagnostic complete. System will restart in 10 seconds..."));
        delay(10000);
        ESP.restart();
    }

    // ... continue with normal setup ...
}
```

## Option 2: Dedicated Diagnostic Mode (Recommended)

Create a separate test sketch just for diagnostics:

### Create a new file: `platformio/src/main_diagnostic.cpp`

```cpp
// Rename your current main.cpp to main_normal.cpp temporarily
// Then create this as main.cpp for testing

#include <Arduino.h>
#include "config.h"
#include "renderer.h"
#include "display_diagnostic.h"
#include "battery.h"

// Display instance
#ifdef DISP_BW_V2
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>
    display(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
#elif defined(DISP_7C_F)
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>
    display(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
#elif defined(DISP_6C)
GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)>
    display(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
#endif

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println(F("\n\n================================="));
    Serial.println(F("ESP32 Photo Frame - Diagnostic Mode"));
    Serial.println(F("=================================\n"));

    // Initialize display
    Serial.println(F("Initializing display..."));
    display.init(115200, true, 2, false);
    display.setRotation(0);

    delay(1000);

    // Run full diagnostic
    display_diagnostic::runFullDiagnostic();

    Serial.println(F("\n================================="));
    Serial.println(F("All tests complete!"));
    Serial.println(F("================================="));
}

void loop() {
    Serial.println(F("\nPress 'R' to repeat tests, 'T' for test pattern only"));

    while(!Serial.available()) {
        delay(100);
    }

    char cmd = Serial.read();

    if(cmd == 'R' || cmd == 'r') {
        display_diagnostic::runFullDiagnostic();
    }
    else if(cmd == 'T' || cmd == 't') {
        display_diagnostic::displayColorTestPattern();
    }
    else if(cmd == 'V' || cmd == 'v') {
        display_diagnostic::measureVoltage();
    }
}
```

## Option 3: Quick Color Test Only

If you just want to test the display colors quickly, add this simple function to your main.cpp:

```cpp
void quickColorTest() {
    Serial.println(F("Quick color test..."));

    display.setFullWindow();
    display.firstPage();
    do {
        // Draw color bars
        int barWidth = DISP_WIDTH / 6;
        display.fillRect(0, 0, barWidth, DISP_HEIGHT, GxEPD_BLACK);
        display.fillRect(barWidth, 0, barWidth, DISP_HEIGHT, GxEPD_WHITE);
        display.fillRect(2*barWidth, 0, barWidth, DISP_HEIGHT, GxEPD_RED);
        display.fillRect(3*barWidth, 0, barWidth, DISP_HEIGHT, GxEPD_GREEN);
        display.fillRect(4*barWidth, 0, barWidth, DISP_HEIGHT, GxEPD_BLUE);
        display.fillRect(5*barWidth, 0, barWidth, DISP_HEIGHT, GxEPD_YELLOW);
    } while (display.nextPage());

    Serial.println(F("Color test complete - check display"));
}
```

Then call `quickColorTest();` in your setup() function.

## How to Upload and Run:

1. **Save your changes** to main.cpp

2. **Compile and upload**:
```bash
pio run -t upload -e feathers3_unexpectedmaker
```

3. **Open Serial Monitor**:
```bash
pio device monitor -b 115200
```

4. **When prompted**, press 'D' within 5 seconds to enter diagnostic mode

## What the Diagnostic Will Show:

1. **Power Supply Analysis**: Measures voltage drops during display refresh
2. **Color Test Pattern**: Shows all 6 colors to check for washout
3. **SPI Communication Test**: Verifies data integrity
4. **Refresh Mode Test**: Tests full and partial refresh

## Reading the Results:

Look for these key indicators:

### Good Results ✅:
```
[DIAG] Idle voltage: 3.28V
[DIAG] Voltage during refresh: Min=3.15V, Max=3.30V
[DIAG] Voltage drop: 0.13V
```

### Problem Results ❌:
```
[DIAG] Idle voltage: 3.28V
[DIAG] Voltage during refresh: Min=2.85V, Max=3.30V
[DIAG] Voltage drop: 0.43V
[DIAG] WARNING: Significant voltage drop detected!
```

If you see voltage drops > 0.3V, that confirms the power delivery issue causing the washout.