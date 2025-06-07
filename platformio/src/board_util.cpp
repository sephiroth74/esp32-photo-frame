#include "board_util.h"
#include "driver/adc.h"
#include "esp_bt.h"

namespace photo_frame {

void enter_deep_sleep() {
    Serial.println(F("Entering deep sleep..."));
    disable_rgb_led();      // Disable RGB LED before going to sleep
    disable_built_in_led(); // Disable built-in LED before going to sleep

    Serial.println(F("Disabling peripherals..."));
    btStop();                                              // Stop Bluetooth to save power
    esp_bt_controller_disable();                           // Disable Bluetooth controller
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); // Disable all wakeup sources

#if DEBUG_LOG
    delay(DELAY_BEFORE_SLEEP);
#endif
    esp_deep_sleep_start();
}

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:    Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP:      Serial.println("Wakeup caused by ULP program"); break;
    default:                        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason); break;
    }
}

void print_board_stats() {
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
    Serial.println("");
} // print_board_stats

void disable_rgb_led() {
#if HAS_RGB_LED
    Serial.print(F("Disabling RGB LED"));
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW); // Disable the RGB LED
#endif
}

void toggle_rgb_led(uint8_t red, uint8_t green, uint8_t blue) {
#if HAS_RGB_LED
    Serial.print(F("Toggling RGB LED to R: "));
    Serial.print(red);
    Serial.print(F(", G: "));
    Serial.print(green);
    Serial.print(F(", B: "));
    Serial.println(blue);

    digitalWrite(LED_RED, red);
    digitalWrite(LED_GREEN, green);
    digitalWrite(LED_BLUE, blue);
#endif
}

void disable_built_in_led() {
#if defined(LED_BUILTIN)
    Serial.print(F("Disabling built-in LED on pin "));
    Serial.println(LED_BUILTIN);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // Disable the built-in LED
#else
    Serial.println(F("LED_BUILTIN is not defined! Cannot disable the built-in LED."));
#endif
} // disable_builtin_led

void blink_builtin_led(photo_frame_error_t error) {
    Serial.print(F("Blinking built-in LED with error code: "));
    Serial.print(error.code);
    Serial.print(F(" ("));
    Serial.print(error.message);
    Serial.print(F(") - "));
    Serial.print(F("Blink count: "));
    Serial.println(error.blink_count);

    blink_builtin_led(error.blink_count, 200);
} // blink_builtin_led with error code

void blink_builtin_led(int count, uint32_t delay_ms) {
#if defined(LED_BUILTIN)
    Serial.println(F("Blinking built-in LED..."));
    pinMode(LED_BUILTIN, OUTPUT);
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_BUILTIN, HIGH); // Turn the LED on
        delay(delay_ms);
        digitalWrite(LED_BUILTIN, LOW); // Turn the LED off
        delay(delay_ms);
    }
#else
    Serial.println(F("LED_BUILTIN is not defined! Cannot blink the built-in LED."));
#endif
} // blink_builtin_led

} // namespace photo_frame