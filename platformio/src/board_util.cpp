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
#include "esp_bt.h"
#include "string_utils.h"

#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#include <esp_adc/adc_oneshot.h>
#else
#include <driver/adc.h>
#endif

namespace photo_frame {

namespace board_utils {

    void enter_deep_sleep(esp_sleep_wakeup_cause_t wakeup_reason, uint64_t refresh_microseconds)
    {
        Serial.println(F("[board_util] Entering deep sleep..."));
        disable_built_in_led(); // Disable built-in LED before going to sleep

        Serial.println(F("[board_util] Disabling peripherals..."));
        btStop(); // Stop Bluetooth to save power
        esp_bt_controller_disable(); // Disable Bluetooth controller

// These are not valid on ESP32-C6 and ESP32-H2
#if !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TOUCHPAD); // Disable touchpad wakeup source
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO); // Disable GPIO wakeup source
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_UART); // Disable UART wakeup source
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_WIFI); // Disable WiFi wakeup source
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_BT); // Disable Bluetooth wakeup source
#endif // !ESP32C6 && !ESP32H2

#if defined(WAKEUP_EXT1)
        Serial.println(F("[board_util] Configuring EXT1 wakeup on RTC IO pin..."));

        // Test if pin supports RTC IO before configuration
        Serial.print(F("[board_util] Testing RTC IO capability for GPIO "));
        Serial.print(WAKEUP_PIN);
        Serial.print(F("... "));

        pinMode(WAKEUP_PIN, WAKEUP_PIN_MODE);

        // Read current pin state for debugging
        int pinState = digitalRead(WAKEUP_PIN);
        Serial.print(F("Pin state: "));
        Serial.print(pinState);

        // Test EXT1 wakeup configuration
        uint64_t pin_mask = 1ULL << WAKEUP_PIN;
        esp_err_t wakeup_result = esp_sleep_enable_ext1_wakeup(pin_mask, WAKEUP_LEVEL);

        Serial.print(F(" | EXT1 config: "));
        if (wakeup_result == ESP_OK) {
            Serial.println(F("SUCCESS"));
            Serial.print(F("[board_util] GPIO "));
            Serial.print(WAKEUP_PIN);
            Serial.print(F(" configured for EXT1 wakeup with mask 0x"));
            Serial.print((uint32_t)pin_mask, HEX);
            Serial.print(F(", level: "));
            Serial.println(WAKEUP_LEVEL == ESP_EXT1_WAKEUP_ANY_HIGH ? "HIGH" : "LOW");

            // Additional test: try to read the configured wakeup source
            Serial.println(F("[board_util] EXT1 wakeup source configured successfully"));
        } else {
            Serial.print(F("[board_util] FAILED - Error code: "));
            Serial.println(wakeup_result);
            Serial.println(F("[board_util] This GPIO pin does not support RTC IO / EXT1 wakeup!"));
        }

#elif defined(WAKEUP_EXT0)
        Serial.println(F("[board_util] Configuring EXT0 wakeup on RTC IO pin..."));

        // Test if pin supports RTC IO before configuration
        Serial.print(F("[board_util] Testing RTC IO capability for GPIO "));
        Serial.print(WAKEUP_PIN);
        Serial.print(F("... "));

        // Configure RTC GPIO for EXT0 wakeup (required for deep sleep)
        // First set regular GPIO mode for initial setup
        pinMode(WAKEUP_PIN, INPUT);

        // Configure RTC GPIO pull-down resistor for deep sleep wakeup
        // This is essential - regular pinMode pull resistors don't work during deep sleep
        rtc_gpio_init(WAKEUP_PIN);
        rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_dis(WAKEUP_PIN); // Disable pull-down
        rtc_gpio_pullup_en(WAKEUP_PIN); // Enable pull-up for EXT0 wakeup
        rtc_gpio_hold_dis(WAKEUP_PIN); // Disable hold to allow normal operation

        // Small delay to allow RTC GPIO to stabilize
        delay(10);

        // Read current pin state for debugging (always enabled for EXT0 issues)
        int pinState = digitalRead(WAKEUP_PIN);
        Serial.print(F("Pin state: "));
        Serial.print(pinState);

        // Verify pin is in expected idle state (should be HIGH with RTC pull-up)
        if (pinState != 1) {
            Serial.print(F(" ⚠️ WARNING: Pin should be HIGH when idle, but reads "));
            Serial.print(pinState);
            Serial.println(F(" - check wiring or RTC GPIO config"));
        } else {
            Serial.println(F(" ✅ Pin correctly reads HIGH with RTC pull-up"));
        }

        // Test EXT0 wakeup configuration
        esp_err_t wakeup_result = esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, WAKEUP_LEVEL);

        Serial.print(F(" | EXT0 config: "));
        if (wakeup_result == ESP_OK) {
            Serial.println(F("SUCCESS"));
        } else {
            Serial.print(F("FAILED - Error code: "));
            Serial.println(wakeup_result);
            Serial.println(F("[board_util] This GPIO pin does not support RTC IO / EXT0 wakeup!"));
        }

#endif // WAKEUP_EXT1

        bool delay_before_sleep = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

        if (delay_before_sleep) {
            Serial.println(F("[board_util] Wakeup reason is undefined, delaying before sleep..."));
            delay(DELAY_BEFORE_SLEEP);
        }

#ifdef WAKEUP_EXT0
        // Final check: Verify wakeup pin is in idle state before sleep
        int finalPinState = digitalRead(WAKEUP_PIN);
        Serial.print(F("[board_util] Final wakeup pin state before sleep: "));
        Serial.println(finalPinState);

        if (finalPinState == WAKEUP_LEVEL) {
            Serial.println(F("⚠️ WARNING: Wakeup pin is already at trigger level!"));
            Serial.println(F("[board_util] This would cause immediate wakeup - check button/wiring"));
        }
#endif

        // Configure timer wakeup if refresh microseconds is provided
        if (refresh_microseconds > 0) {
            Serial.print(F("[board_util] Configuring timer wakeup for "));
            Serial.print((unsigned long)(refresh_microseconds / 1000000ULL));
            Serial.println(F(" seconds"));
            esp_sleep_enable_timer_wakeup(refresh_microseconds);
        } else {
            Serial.println(F("[board_util] No timer wakeup configured - will sleep indefinitely"));
        }

        Serial.println(F("[board_util] Going to deep sleep now. Good night!"));
        Serial.flush(); // Ensure all serial output is sent before sleeping
        esp_deep_sleep_start();
    }

    esp_sleep_wakeup_cause_t get_wakeup_reason()
    {
        esp_sleep_wakeup_cause_t wakeup_reason;
        wakeup_reason = esp_sleep_get_wakeup_cause();
        return wakeup_reason;
    }

    void get_wakeup_reason_string(esp_sleep_wakeup_cause_t wakeup_reason,
        char* buffer,
        size_t buffer_size)
    {
        switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            snprintf(buffer, buffer_size, "Undefined");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            snprintf(buffer, buffer_size, "External wakeup (EXT0)");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            snprintf(buffer, buffer_size, "External wakeup (EXT1)");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            snprintf(buffer, buffer_size, "Timer wakeup");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            snprintf(buffer, buffer_size, "Touchpad wakeup");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            snprintf(buffer, buffer_size, "ULP wakeup");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            snprintf(buffer, buffer_size, "GPIO wakeup");
            break;
        case ESP_SLEEP_WAKEUP_UART:
            snprintf(buffer, buffer_size, "UART wakeup");
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            snprintf(buffer, buffer_size, "WiFi wakeup");
            break;
        case ESP_SLEEP_WAKEUP_COCPU:
            snprintf(buffer, buffer_size, "COCPU wakeup");
            break;
        case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
            snprintf(buffer, buffer_size, "COCPU trap trigger wakeup");
            break;
        case ESP_SLEEP_WAKEUP_BT:
            snprintf(buffer, buffer_size, "Bluetooth wakeup");
            break;
        default:
            snprintf(buffer, buffer_size, "Unknown wakeup reason");
        }
    } // get_wakeup_reason_string

    void print_board_stats()
    {
#ifdef DEBUG_BOARD
        Serial.println(F("[board_util] Board Statistics:"));
        Serial.print(F("[board_util] Heap: "));
        Serial.println(ESP.getHeapSize());
        Serial.print(F("[board_util] Flash: "));
        Serial.println(ESP.getFlashChipSize());
        Serial.print(F("[board_util] Free Heap: "));
        Serial.println(ESP.getFreeHeap());
        Serial.print(F("[board_util] Free Flash: "));
        Serial.println(ESP.getFreeSketchSpace());
        Serial.print(F("[board_util] Free PSRAM: "));
        Serial.println(ESP.getFreePsram());
        Serial.print(F("[board_util] PSRAM Size: "));
        Serial.println(ESP.getPsramSize());

        // PSRAM is mandatory for this configuration
        Serial.println(F("[board_util] PSRAM is required and should be available"));

#ifdef CONFIG_SPIRAM_SUPPORT
        Serial.println(F("[board_util] CONFIG_SPIRAM_SUPPORT is defined"));
#else
        Serial.println(F("[board_util] CONFIG_SPIRAM_SUPPORT is NOT defined"));
#endif

        // Check if PSRAM is actually available
        if (ESP.getPsramSize() > 0) {
            Serial.println(F("[board_util] PSRAM detected and available"));
        } else {
            Serial.println(
                F("[board_util] WARNING: PSRAM not detected - check hardware/configuration"));
        }
        Serial.print(F("[board_util] Chip Model: "));
        Serial.println(ESP.getChipModel());
        Serial.print(F("[board_util] Chip Revision: "));
        Serial.println(ESP.getChipRevision());
        Serial.print(F("[board_util] Chip Cores: "));
        Serial.println(ESP.getChipCores());
        Serial.print(F("[board_util] CPU Freq: "));
        Serial.println(ESP.getCpuFreqMHz());
        Serial.println("-----------------------------");
#endif // DEBUG_MODE
    } // print_board_stats

    void disable_built_in_led()
    {
#if defined(LED_BUILTIN)
        Serial.print(F("[board_util] Disabling built-in LED on pin "));
        Serial.println(LED_BUILTIN);
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW); // Disable the built-in LED
#else
        Serial.println(F("[board_util] LED_BUILTIN is not defined! Cannot disable the built-in LED."));
#endif
    } // disable_builtin_led

    void blink_builtin_led(int count, unsigned long on_ms, unsigned long off_ms)
    {
#if defined(LED_BUILTIN)
        Serial.print(F("[board_util] Blinking built-in LED on pin "));
        Serial.println(LED_BUILTIN);

        pinMode(LED_BUILTIN, OUTPUT);
        for (int i = 0; i < count; i++) {
#ifdef DEBUG_BOARD
            Serial.println(F("[board_util] Blinking on.."));
#endif // DEBUG_BOARD
            digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
            delay(on_ms);
#ifdef DEBUG_BOARD
            Serial.println(F("[board_util] Blinking off.."));
#endif // DEBUG_BOARD
            digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
            delay(off_ms);
        }
#else
        Serial.println(F("[board_util] LED_BUILTIN is not defined! Cannot blink the built-in LED."));
#endif
    } // blink_builtin_led

    long read_refresh_seconds(bool is_battery_low)
    {
        Serial.println(F("[board_util] Reading potentiometer..."));

        if (is_battery_low) {
            Serial.println(F("[board_util] Battery level is low, skipping potentiometer reading."));
            return REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY;
        }

        // now read the level
        pinMode(POTENTIOMETER_PWR_PIN, OUTPUT);
        digitalWrite(POTENTIOMETER_PWR_PIN, HIGH); // Power on the potentiometer

        // Set ADC attenuation for better range utilization
        analogSetPinAttenuation(POTENTIOMETER_INPUT_PIN, ADC_11db);
        delay(100); // Wait potentiometer to stabilize

        // read the potentiometer for 100ms and average the result (10 samples)
        unsigned long level = 0;
        for (int i = 0; i < 10; i++) {
            level += analogRead(POTENTIOMETER_INPUT_PIN); // Read the potentiometer
            delay(1); // Wait for 1ms
        }
        digitalWrite(POTENTIOMETER_PWR_PIN, LOW); // Power off the level shifter
        level /= 10; // Average the result

#ifdef DEBUG_BOARD
        Serial.print(F("[board_util] Raw potentiometer pin reading: "));
        Serial.println(level);
#endif // DEBUG_BOARD

        level = constrain(level, 0, POTENTIOMETER_INPUT_MAX); // Constrain the level to the maximum value

        // invert the value
        level = POTENTIOMETER_INPUT_MAX - level;

#ifdef DEBUG_BOARD
        Serial.print(F("[board_util] Potentiometer value: "));
        Serial.println(level);
#endif // DEBUG_BOARD

        long refresh_seconds = map(level,
            0,
            POTENTIOMETER_INPUT_MAX,
            REFRESH_MIN_INTERVAL_SECONDS,
            REFRESH_MAX_INTERVAL_SECONDS);

        char buffer[64];
        photo_frame::string_utils::seconds_to_human(buffer, sizeof(buffer), refresh_seconds);

#ifdef DEBUG_BOARD
        Serial.print(F("[board_util] Refresh seconds: "));
        Serial.println(buffer);
#endif // DEBUG_BOARD

        if (refresh_seconds > (REFRESH_MIN_INTERVAL_SECONDS * 2)) {
            // increase the refresh seconds to the next step
            Serial.println(F("[board_util] Increasing refresh seconds to the next step..."));
            refresh_seconds = (refresh_seconds / (long)REFRESH_STEP_SECONDS + 1) * (long)REFRESH_STEP_SECONDS;
        }

        Serial.print(F("[board_util] Final refresh seconds: "));
        Serial.println(refresh_seconds);

        return refresh_seconds;
    } // read_refresh_seconds

    void print_board_pins()
    {
        Serial.println(F("[board_util] Board Pin Assignments:"));
        Serial.print(F("[board_util] LED_BUILTIN: "));
#ifdef LED_BUILTIN
        Serial.println(LED_BUILTIN);
#else
        Serial.println(F("[board_util] N/A"));
#endif
        Serial.print(F("[board_util] SDA: "));
        Serial.println(SDA);
        Serial.print(F("[board_util] SCL: "));
        Serial.println(SCL);
        Serial.print(F("[board_util] SCK: "));
        Serial.println(SCK);
        Serial.print(F("[board_util] MOSI: "));
        Serial.println(MOSI);
        Serial.print(F("[board_util] MISO: "));
        Serial.println(MISO);
        Serial.print(F("[board_util] CS: "));
        Serial.println(SS);
    }

} // namespace board_utils

} // namespace photo_frame