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
#include "string_utils.h"

#if defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#include <esp_adc/adc_oneshot.h>
#else
#include <driver/adc.h>
#endif

namespace photo_frame {

namespace board_utils {

void init_display_power() {
#ifdef DISPLAY_POWER_PIN
    log_i("[POWER] Initializing display power control on GPIO %d", DISPLAY_POWER_PIN);
    pinMode(DISPLAY_POWER_PIN, OUTPUT);

    // Start with display OFF
    display_power_off();
    delay(100);
#else
    log_d("[POWER] Display power control not configured (DISPLAY_POWER_PIN not defined)");
#endif
}

void display_power_on() {
#ifdef DISPLAY_POWER_PIN
#ifndef DISPLAY_POWER_ACTIVE_LOW
#error                                                                                             \
    "DISPLAY_POWER_ACTIVE_LOW must be defined when DISPLAY_POWER_PIN is defined (set to 1 for active-low or 0 for active-high)"
#endif

    log_i("[POWER] Turning display ON (GPIO %d -> %s)",
          DISPLAY_POWER_PIN,
          DISPLAY_POWER_ACTIVE_LOW ? "LOW" : "HIGH");

#if DISPLAY_POWER_ACTIVE_LOW == 1
    digitalWrite(DISPLAY_POWER_PIN, LOW); // P-MOSFET: LOW = ON
#else
    digitalWrite(DISPLAY_POWER_PIN, HIGH); // ProS3 LDO2 or N-MOSFET: HIGH = ON
#endif

    delay(200); // Allow power to stabilize (GDEP073E01 needs time)
    log_i("[POWER] Display power stabilized");
#endif
}

void display_power_off() {
#ifdef DISPLAY_POWER_PIN
#ifndef DISPLAY_POWER_ACTIVE_LOW
#error                                                                                             \
    "DISPLAY_POWER_ACTIVE_LOW must be defined when DISPLAY_POWER_PIN is defined (set to 1 for active-low or 0 for active-high)"
#endif

    log_i("[POWER] Turning display OFF (GPIO %d -> %s)",
          DISPLAY_POWER_PIN,
          DISPLAY_POWER_ACTIVE_LOW ? "HIGH" : "LOW");

#if DISPLAY_POWER_ACTIVE_LOW == 1
    digitalWrite(DISPLAY_POWER_PIN, HIGH); // P-MOSFET: HIGH = OFF
#else
    digitalWrite(DISPLAY_POWER_PIN, LOW); // ProS3 LDO2 or N-MOSFET: LOW = OFF
#endif

    delay(50); // Short delay for clean shutdown
    log_i("[POWER] Display powered off");
#endif
}

void enter_deep_sleep(esp_sleep_wakeup_cause_t wakeup_reason, uint64_t refresh_microseconds) {
    log_i("Entering deep sleep...");
    disable_built_in_led(); // Disable built-in LED before going to sleep

    // Power off display if power control is configured
    display_power_off();

    log_i("Disabling peripherals...");
    // btStop(); // Stop Bluetooth to save power
    // esp_bt_controller_disable(); // Disable Bluetooth controller

#if defined(WAKEUP_EXT1)
    log_i("Configuring EXT1 wakeup on RTC IO pin...");

    // Test if pin supports RTC IO before configuration
    pinMode(WAKEUP_PIN, WAKEUP_PIN_MODE);

    // Read current pin state for debugging
    int pinState = digitalRead(WAKEUP_PIN);

    // Test EXT1 wakeup configuration
    uint64_t pin_mask       = 1ULL << WAKEUP_PIN;
    esp_err_t wakeup_result = esp_sleep_enable_ext1_wakeup(pin_mask, WAKEUP_LEVEL);

    if (wakeup_result == ESP_OK) {
        log_i("Testing RTC IO capability for GPIO %d... Pin state: %d | EXT1 config: SUCCESS",
              WAKEUP_PIN,
              pinState);
        log_i("GPIO %d configured for EXT1 wakeup with mask 0x%X, level: %s",
              WAKEUP_PIN,
              (uint32_t)pin_mask,
              WAKEUP_LEVEL == ESP_EXT1_WAKEUP_ANY_HIGH ? "HIGH" : "LOW");
        log_i("EXT1 wakeup source configured successfully");
    } else {
        log_e("Testing RTC IO capability for GPIO %d... FAILED - Error code: %d",
              WAKEUP_PIN,
              wakeup_result);
        log_e("This GPIO pin does not support RTC IO / EXT1 wakeup!");
    }

#elif defined(WAKEUP_EXT0)
    log_i("Configuring EXT0 wakeup on RTC IO pin...");

    // Configure RTC GPIO for EXT0 wakeup (required for deep sleep)
    // First set regular GPIO mode for initial setup
    pinMode(WAKEUP_PIN, INPUT);

    // Configure RTC GPIO pull-down resistor for deep sleep wakeup
    // This is essential - regular pinMode pull resistors don't work during deep sleep
    rtc_gpio_init(WAKEUP_PIN);
    rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(WAKEUP_PIN); // Disable pull-down
    rtc_gpio_pullup_en(WAKEUP_PIN);    // Enable pull-up for EXT0 wakeup
    rtc_gpio_hold_dis(WAKEUP_PIN);     // Disable hold to allow normal operation

    // Small delay to allow RTC GPIO to stabilize
    delay(10);

    // Read current pin state for debugging (always enabled for EXT0 issues)
    int pinState = digitalRead(WAKEUP_PIN);

    // Verify pin is in expected idle state (should be HIGH with RTC pull-up)
    if (pinState != 1) {
        log_w("Testing RTC IO capability for GPIO %d... Pin state: %d - WARNING: Pin should be "
              "HIGH when idle - check wiring or RTC GPIO config",
              WAKEUP_PIN,
              pinState);
    } else {
        log_i("Testing RTC IO capability for GPIO %d... Pin state: %d - Pin correctly reads HIGH "
              "with RTC pull-up",
              WAKEUP_PIN,
              pinState);
    }

    // Test EXT0 wakeup configuration
    esp_err_t wakeup_result = esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, WAKEUP_LEVEL);

    if (wakeup_result == ESP_OK) {
        log_i("EXT0 config: SUCCESS");
    } else {
        log_e("EXT0 config: FAILED - Error code: %d", wakeup_result);
        log_e("This GPIO pin does not support RTC IO / EXT0 wakeup!");
    }

#endif // WAKEUP_EXT1

    bool delay_before_sleep = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    if (delay_before_sleep) {
        log_i("Wakeup reason is undefined, delaying before sleep...");
        delay(DELAY_BEFORE_SLEEP);
    }

#ifdef WAKEUP_EXT0
    // Final check: Verify wakeup pin is in idle state before sleep
    int finalPinState = digitalRead(WAKEUP_PIN);

    if (finalPinState == WAKEUP_LEVEL) {
        log_w("Final wakeup pin state before sleep: %d - WARNING: Wakeup pin is already at trigger "
              "level!",
              finalPinState);
        log_w("This would cause immediate wakeup - check button/wiring");
    } else {
        log_i("Final wakeup pin state before sleep: %d", finalPinState);
    }
#endif

    // Configure timer wakeup if refresh microseconds is provided
    if (refresh_microseconds > 0) {
        log_i("Configuring timer wakeup for %lu seconds",
              (unsigned long)(refresh_microseconds / 1000000ULL));
        esp_sleep_enable_timer_wakeup(refresh_microseconds);
    } else {
        log_i("No timer wakeup configured - will sleep indefinitely");
    }

#ifndef DISABLE_DEEP_SLEEP
    log_i("Going to deep sleep now. Good night!");
    Serial.flush(); // Ensure all serial output is sent before sleeping
    esp_deep_sleep_start();
#else
    log_d("Deep sleep disabled");
#endif // DISABLE_DEEP_SLEEP
}

esp_sleep_wakeup_cause_t get_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause();
    return wakeup_reason;
}

void get_wakeup_reason_string(esp_sleep_wakeup_cause_t wakeup_reason,
                              char* buffer,
                              size_t buffer_size) {
    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_UNDEFINED: snprintf(buffer, buffer_size, "Undefined"); break;
    case ESP_SLEEP_WAKEUP_EXT0:      snprintf(buffer, buffer_size, "External wakeup (EXT0)"); break;
    case ESP_SLEEP_WAKEUP_EXT1:      snprintf(buffer, buffer_size, "External wakeup (EXT1)"); break;
    case ESP_SLEEP_WAKEUP_TIMER:     snprintf(buffer, buffer_size, "Timer wakeup"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:  snprintf(buffer, buffer_size, "Touchpad wakeup"); break;
    case ESP_SLEEP_WAKEUP_ULP:       snprintf(buffer, buffer_size, "ULP wakeup"); break;
    case ESP_SLEEP_WAKEUP_GPIO:      snprintf(buffer, buffer_size, "GPIO wakeup"); break;
    case ESP_SLEEP_WAKEUP_UART:      snprintf(buffer, buffer_size, "UART wakeup"); break;
    case ESP_SLEEP_WAKEUP_WIFI:      snprintf(buffer, buffer_size, "WiFi wakeup"); break;
    case ESP_SLEEP_WAKEUP_COCPU:     snprintf(buffer, buffer_size, "COCPU wakeup"); break;
    case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
        snprintf(buffer, buffer_size, "COCPU trap trigger wakeup");
        break;
    case ESP_SLEEP_WAKEUP_BT: snprintf(buffer, buffer_size, "Bluetooth wakeup"); break;
    default:                  snprintf(buffer, buffer_size, "Unknown wakeup reason");
    }
} // get_wakeup_reason_string

void print_board_stats() {
#ifdef DEBUG_BOARD
    log_d("Board Statistics:");

    log_d("Board: %s", ARDUINO_BOARD);
    log_d("Chip Model: %s", ESP.getChipModel());
    log_d("Chip Revision: %d", ESP.getChipRevision());
    log_d("Cores: %d", ESP.getChipCores());
    log_d("CPU Frequency: %d MHz", ESP.getCpuFreqMHz());

    log_d("Flash Size: %u bytes (%.2f MB)",
          ESP.getFlashChipSize(),
          ESP.getFlashChipSize() / 1048576.0f);
    log_d("Flash Speed: %u MHz", ESP.getFlashChipSpeed() / 1000000);

    log_d("Free Heap: %u bytes (%.2f KB)", ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0f);
    log_d("Total Heap: %u bytes (%.2f KB)", ESP.getHeapSize(), ESP.getHeapSize() / 1024.0f);
    log_d(
        "Min Free Heap: %u bytes (%.2f KB)", ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024.0f);

    if (psramFound()) {
        log_d("PSRAM: FOUND");
        log_d(
            "PSRAM Size: %u bytes (%.2f MB)", ESP.getPsramSize(), ESP.getPsramSize() / 1048576.0f);
        log_d(
            "Free PSRAM: %u bytes (%.2f MB)", ESP.getFreePsram(), ESP.getFreePsram() / 1048576.0f);
        log_d("Min Free PSRAM: %u bytes (%.2f MB)",
              ESP.getMinFreePsram(),
              ESP.getMinFreePsram() / 1048576.0f);
    } else {
        log_w("PSRAM: NOT FOUND");
    }

    // SDK and MAC
    log_d("SDK Version: %s", ESP.getSdkVersion());

    log_d("Free Flash: %d", ESP.getFreeSketchSpace());

    // PSRAM is mandatory for this configuration
    log_d("PSRAM is required and should be available");

#ifdef CONFIG_SPIRAM_SUPPORT
    log_d("CONFIG_SPIRAM_SUPPORT is defined");
#else
    log_d("CONFIG_SPIRAM_SUPPORT is NOT defined");
#endif

    // Check if PSRAM is actually available
    if (ESP.getPsramSize() > 0) {
        log_d("PSRAM detected and available");
    } else {
        log_w("WARNING: PSRAM not detected - check hardware/configuration");
    }
    log_d("Chip Model: %s", ESP.getChipModel());
    log_d("Chip Revision: %d", ESP.getChipRevision());
    log_d("Chip Cores: %d", ESP.getChipCores());
    log_d("CPU Freq: %d", ESP.getCpuFreqMHz());
    log_d("-----------------------------");
#endif // DEBUG_MODE
} // print_board_stats

void disable_built_in_led() {
#if defined(LED_BUILTIN)
    log_i("Disabling built-in LED on pin %d", LED_BUILTIN);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Disable the built-in LED
#else
    log_v("LED_BUILTIN is not defined! Cannot disable the built-in LED.");
#endif
} // disable_builtin_led

void blink_builtin_led(int count, unsigned long on_ms, unsigned long off_ms) {
#if defined(LED_BUILTIN)
    log_i("Blinking built-in LED on pin %d", LED_BUILTIN);

    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < count; i++) {
#ifdef DEBUG_BOARD
        log_d("Blinking on..");
#endif                                   // DEBUG_BOARD
        digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
        delay(on_ms);
#ifdef DEBUG_BOARD
        log_d("Blinking off..");
#endif                                  // DEBUG_BOARD
        digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
        delay(off_ms);
    }
#else
    log_v("LED_BUILTIN is not defined! Cannot blink the built-in LED.");
#endif
} // blink_builtin_led

long read_refresh_seconds(const unified_config& config, bool is_battery_low) {
    if (is_battery_low) {
        log_i("Battery level is low, using critical battery interval");
        return REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY;
    }

#ifdef USE_POTENTIOMETER
    log_i("Reading potentiometer...");

    // now read the level
#if POTENTIOMETER_PWR_PIN > -1
    pinMode(POTENTIOMETER_PWR_PIN, OUTPUT);
    digitalWrite(POTENTIOMETER_PWR_PIN, HIGH); // Power on the potentiometer
#endif

    // Set ADC attenuation for better range utilization
    analogSetPinAttenuation(POTENTIOMETER_INPUT_PIN, ADC_11db);
    delay(100); // Wait potentiometer to stabilize

    // read the potentiometer for 100ms and average the result (10 samples)
    unsigned long level = 0;
    for (int i = 0; i < 10; i++) {
        level += analogRead(POTENTIOMETER_INPUT_PIN); // Read the potentiometer
        delay(1);                                     // Wait for 1ms
    }

#if POTENTIOMETER_PWR_PIN > -1
    digitalWrite(POTENTIOMETER_PWR_PIN, LOW); // Power off the level shifter
#endif
    level /= 10; // Average the result

#ifdef DEBUG_BOARD
    log_d("Raw potentiometer pin reading: %lu", level);
#endif // DEBUG_BOARD

    level =
        constrain(level, 0, POTENTIOMETER_INPUT_MAX); // Constrain the level to the maximum value

    // invert the value
    level = POTENTIOMETER_INPUT_MAX - level;

#ifdef DEBUG_BOARD
    log_d("Potentiometer value: %lu", level);
#endif // DEBUG_BOARD

    // Use exponential mapping for better control at lower values
    // Convert level to 0-1 range
    float linear_position = (float)level / POTENTIOMETER_INPUT_MAX;

    // Apply exponential curve (power of 3 for more aggressive curve)
    // This gives even more resolution at lower values:
    //   Linear 10% → Exponential 0.1%  → ~5.3 minutes
    //   Linear 25% → Exponential 1.56% → ~9 minutes
    //   Linear 50% → Exponential 12.5% → ~33 minutes
    //   Linear 75% → Exponential 42.2% → ~106 minutes
    //   Linear 100%→ Exponential 100%  → 240 minutes
    float exponential_position =
        linear_position * linear_position *
        linear_position; // Cube the position for more aggressive exponential curve

#ifdef DEBUG_BOARD
    log_d("Linear position: %.1f%% -> Exponential position: %.1f%%",
          linear_position * 100,
          exponential_position * 100);
#endif // DEBUG_BOARD

    // Map the exponential position to refresh seconds
    long refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS +
                           (long)(exponential_position *
                                  (REFRESH_MAX_INTERVAL_SECONDS - REFRESH_MIN_INTERVAL_SECONDS));

    char buffer[64];
    photo_frame::string_utils::seconds_to_human(buffer, sizeof(buffer), refresh_seconds);

#ifdef DEBUG_BOARD
    log_d("Refresh seconds: %s", buffer);
#endif // DEBUG_BOARD

    if (refresh_seconds > (REFRESH_MIN_INTERVAL_SECONDS)) {
        // increase the refresh seconds to the next step
        log_i("Increasing refresh seconds to the next step...");
        refresh_seconds =
            (refresh_seconds / (long)REFRESH_STEP_SECONDS + 1) * (long)REFRESH_STEP_SECONDS;
    }

    log_i("Final refresh seconds: %ld", refresh_seconds);

    return refresh_seconds;
#else
    // USE_POTENTIOMETER not defined - use default value from config
    long refresh_seconds = config.board.refresh.default_seconds;

    // Apply low battery multiplier if needed
    if (is_battery_low) {
        refresh_seconds *= config.board.refresh.low_battery_multiplier;
    }

    log_i("Using default refresh interval from config: %ld seconds", refresh_seconds);

    return refresh_seconds;
#endif
} // read_refresh_seconds

void print_board_pins() {
    log_i("Board Pin Assignments:");
#ifdef LED_BUILTIN
    log_i("LED_BUILTIN: %d", LED_BUILTIN);
#else
    log_i("LED_BUILTIN: N/A");
#endif
    log_i("SDA: %d", SDA);
    log_i("SCL: %d", SCL);
    log_i("SCK: %d", SCK);
    log_i("MOSI: %d", MOSI);
    log_i("MISO: %d", MISO);
    log_i("CS: %d", SS);
}

} // namespace board_utils

} // namespace photo_frame