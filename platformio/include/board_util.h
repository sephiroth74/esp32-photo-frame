#ifndef PHOTO_FRAME_BOARD_UTIL_H__
#define PHOTO_FRAME_BOARD_UTIL_H__

#include "config.h"
#include <battery.h>

namespace photo_frame {

void disable_builtin_led() {
// disable the built-in LED
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW); // turn the LED off
    gpio_hold_en(static_cast<gpio_num_t>(LED_BUILTIN));
    gpio_deep_sleep_hold_en();
#endif
}

void sleep_enable_timer_wakeup(Battery& battery) {
    // Disable the built-in LED to save power
#ifdef ESP32
    Serial.print(F("Enable deep sleep timer... "));

    if (battery) {
        uint32_t sleep_duration = SLEEP_TIMEOUT_MINUTES_ON_BATTERY;
        if (battery.voltage > BATTERY_CRITICAL_VOLTAGE) {
            if (battery.voltage < BATTERY_ALERT_VOLTAGE) {
                sleep_duration = SLEEP_TIMEOUT_MINUTES_ON_BATTERY * 2;
            }
            Serial.print(sleep_duration);
            Serial.println(F(" minutes"));
            esp_sleep_enable_timer_wakeup(sleep_duration * 60 * 1000000);
        }
    } else {
        Serial.print(SLEEP_TIMEOUT_MINUTES_ON_USB);
        Serial.println(F(" minutes"));
        esp_sleep_enable_timer_wakeup(SLEEP_TIMEOUT_MINUTES_ON_USB * 60 * 1000000);
    }

#endif
}

void enter_deep_sleep() {
    // Disable the built-in LED to save power
#ifdef ESP32
    Serial.print(F("Entering deep sleep..."));
    esp_deep_sleep_start();
#endif
}

} // namespace photo_frame

#endif // PHOTO_FRAME_BOARD_UTIL_H__