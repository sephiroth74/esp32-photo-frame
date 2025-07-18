// MIT License
//
// Copyright (c) 2025 Alessandro Crugnola
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "board_util.h"
#include "driver/adc.h"
#include "esp_bt.h"

namespace photo_frame {

void enter_deep_sleep(esp_sleep_wakeup_cause_t wakeup_reason)
{
    Serial.println(F("Entering deep sleep..."));
    disable_rgb_led(); // Disable RGB LED before going to sleep
    disable_built_in_led(); // Disable built-in LED before going to sleep

    Serial.println(F("Disabling peripherals..."));
    btStop(); // Stop Bluetooth to save power
    esp_bt_controller_disable(); // Disable Bluetooth controller
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT0); // Disable external wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_EXT1); // Disable external wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD); // Disable touchpad wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ULP); // Disable ULP wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO); // Disable GPIO wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_UART); // Disable UART wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_WIFI); // Disable WiFi wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_COCPU); // Disable COCPU wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG); // Disable COCPU trap trigger
                                                                       // wakeup source
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_BT); // Disable Bluetooth wakeup source

    bool delay_before_sleep = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

#if DEBUG_MODE
    delay_before_sleep = true;
#endif

    if (delay_before_sleep) {
        Serial.println(F("Wakeup reason is undefined or debug mode is on, delaying before "
                         "sleep..."));
        delay(DELAY_BEFORE_SLEEP);
    }

    esp_deep_sleep_start();
}

esp_sleep_wakeup_cause_t get_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    return wakeup_reason;
}

String get_wakeup_reason_string(esp_sleep_wakeup_cause_t wakeup_reason)
{
    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:
        return "Undefined";
    case ESP_SLEEP_WAKEUP_EXT0:
        return "External wakeup (EXT0)";
    case ESP_SLEEP_WAKEUP_EXT1:
        return "External wakeup (EXT1)";
    case ESP_SLEEP_WAKEUP_TIMER:
        return "Timer wakeup";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        return "Touchpad wakeup";
    case ESP_SLEEP_WAKEUP_ULP:
        return "ULP wakeup";
    case ESP_SLEEP_WAKEUP_GPIO:
        return "GPIO wakeup";
    case ESP_SLEEP_WAKEUP_UART:
        return "UART wakeup";
    case ESP_SLEEP_WAKEUP_WIFI:
        return "WiFi wakeup";
    case ESP_SLEEP_WAKEUP_COCPU:
        return "COCPU wakeup";
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
        return "COCPU trap trigger wakeup";
    case ESP_SLEEP_WAKEUP_BT:
        return "Bluetooth wakeup";
    default:
        return "Unknown wakeup reason";
    }
} // get_wakeup_reason_string

void print_board_stats()
{
#if DEBUG_MODE
    Serial.println(F("Board Statistics:"));
    Serial.print("Heap:");
    Serial.println(ESP.getHeapSize());
    Serial.print("Flash: ");
    Serial.println(ESP.getFlashChipSize());
    Serial.print("Free Heap: ");
    Serial.println(ESP.getFreeHeap());
    Serial.print("Free Flash: ");
    Serial.println(ESP.getFreeSketchSpace());
    Serial.print("Free PSRAM: ");
    Serial.println(ESP.getFreePsram());
    Serial.print("Chip Model: ");
    Serial.println(ESP.getChipModel());
    Serial.print("Chip Revision: ");
    Serial.println(ESP.getChipRevision());
    Serial.print("Chip Cores: ");
    Serial.println(ESP.getChipCores());
    Serial.print("CPU Freq: ");
    Serial.println(ESP.getCpuFreqMHz());
    Serial.println("-----------------------------");
#endif // DEBUG_MODE
} // print_board_stats

void disable_rgb_led()
{
#if HAS_RGB_LED
    Serial.println(F("Disabling RGB LED..."));
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
#endif
}

void toggle_rgb_led(bool red, bool green, bool blue)
{
#if HAS_RGB_LED
    Serial.print(F("Toggling RGB LED to R: "));
    Serial.print(red);
    Serial.print(F(", G: "));
    Serial.print(green);
    Serial.print(F(", B: "));
    Serial.println(blue);

    digitalWrite(LED_RED, red ? LOW : HIGH); // LOW to turn on the LED
    digitalWrite(LED_GREEN, green ? LOW : HIGH);
    digitalWrite(LED_BLUE, blue ? LOW : HIGH);
#endif
}

void disable_built_in_led()
{
#if defined(LED_BUILTIN)
    Serial.print(F("Disabling built-in LED on pin "));
    Serial.println(LED_BUILTIN);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Disable the built-in LED
#else
    Serial.println(F("LED_BUILTIN is not defined! Cannot disable the built-in LED."));
#endif
} // disable_builtin_led

void blink_builtin_led(photo_frame_error_t error)
{
#if defined(LED_BUILTIN)
    Serial.print(F("Blinking built-in LED with error code: "));
    Serial.print(error.code);
    Serial.print(F(" ("));
    Serial.print(error.message);
    Serial.print(F(") - "));
    Serial.print(F("Blink count: "));
    Serial.println(error.blink_count);
    blink_builtin_led(error.blink_count);
#endif
} // blink_builtin_led with error code

void blink_builtin_led(int count, unsigned long on_ms, unsigned long off_ms)
{
#if defined(LED_BUILTIN)
    Serial.println(F("Blinking built-in LED..."));
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
        delay(on_ms);
        digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
        delay(off_ms);
    }
#else
    Serial.println(F("LED_BUILTIN is not defined! Cannot blink the built-in LED."));
#endif
} // blink_builtin_led

void blink_rgb_led(uint32_t count, bool red, bool green, bool blue, unsigned long on_ms, unsigned long off_ms)
{
#if HAS_RGB_LED
    Serial.print(F("Blinking RGB LED..."));
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    for (int i = 0; i < count; i++) {
        toggle_rgb_led(red, green, blue);
        delay(on_ms);
        toggle_rgb_led(false, false, false);
        delay(off_ms);
    }
#endif
} // blin,_rgb_led

void blink_rgb_led(photo_frame_error_t error)
{
#if HAS_RGB_LED
    Serial.print(F("Blinking RGB LED with error code: "));
    Serial.print(error.code);
    Serial.print(F(" ("));
    Serial.print(error.message);
    Serial.print(F(") - "));
    Serial.print(F("Blink count: "));
    Serial.println(error.blink_count);

    blink_rgb_led(error.blink_count, true, false, false);
#endif
} // blink_rgb_led with error code

void blink_error(photo_frame_error_t error)
{
    Serial.print(F("Blinking error: "));
    Serial.print(error.message);
    Serial.print(F(" (Code: "));
    Serial.print(error.code);
    Serial.print(F(", Blink count: "));
    Serial.println(error.blink_count);

#if HAS_RGB_LED
    blink_rgb_led(error);
#elif defined(LED_BUILTIN)
    blink_builtin_led(error);
#endif
} // blink_error

long read_refresh_seconds(bool is_battery_low)
{
    Serial.println(F("Reading potentiometer..."));

    if (is_battery_low) {
        Serial.println(F("Battery level is low, skipping potentiometer reading."));
        return REFRESH_INTERVAL_SECONDS_LOW_BATTERY;
    }

    // now read the level
    pinMode(POTENTIOMETER_PWR_PIN, OUTPUT);
    pinMode(POTENTIOMETER_INPUT_PIN, INPUT);

    digitalWrite(POTENTIOMETER_PWR_PIN, HIGH); // Power on the potentiometer
    delay(100); // Wait potentiometer to stabilize

    // read the potentiometer for 100ms and average the result (10 samples)
    unsigned long level = 0;
    for (int i = 0; i < 10; i++) {
        level += analogRead(POTENTIOMETER_INPUT_PIN); // Read the potentiometer
        delay(1); // Wait for 10ms
    }
    digitalWrite(POTENTIOMETER_PWR_PIN, LOW); // Power off the level shifter
    level /= 10; // Average the result

#if DEBUG_MODE
    Serial.print(F("Raw potentiometer pin reading: "));
    Serial.println(level);
#endif

    level = constrain(level,
        0,
        POTENTIOMETER_INPUT_MAX); // Constrain the level to the maximum value

    // invert the value
    level = POTENTIOMETER_INPUT_MAX - level;

    Serial.println(F("Potentiometer value: "));
    Serial.println(level);

    long refresh_seconds = map(level,
        0,
        POTENTIOMETER_INPUT_MAX,
        REFRESH_MIN_INTERVAL_SECONDS,
        REFRESH_MAX_INTERVAL_SECONDS);

    Serial.print(F("Refresh seconds: "));
    Serial.println(refresh_seconds);

    if (refresh_seconds > (REFRESH_MIN_INTERVAL_SECONDS * 2)) {
        // increase the refresh seconds to the next step
        Serial.println(F("Increasing refresh seconds to the next step..."));
        refresh_seconds = (refresh_seconds / (long)REFRESH_STEP_SECONDS + 1) * (long)REFRESH_STEP_SECONDS;
    }

    Serial.print(F("Final refresh seconds: "));
    Serial.println(refresh_seconds);

    return refresh_seconds;
} // read_refresh_seconds

} // namespace photo_frame