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

#include "config.h"

#define STR(s) #s
#define X_LOCALE(code) STR(code)

const int8_t SD_CS_PIN = D3;
const int8_t SD_MISO_PIN = D5;
const int8_t SD_MOSI_PIN = D4;
const int8_t SD_SCK_PIN = D6;

const int8_t EPD_CS_PIN = D9;
const int8_t EPD_DC_PIN = D10;
const int8_t EPD_RST_PIN = D11;
const int8_t EPD_BUSY_PIN = D12;
const int8_t EPD_MISO_PIN = -1; // MISO
const int8_t EPD_MOSI_PIN = D7; // MOSI (DIN)
const int8_t EPD_SCK_PIN = D8;
const int8_t EPD_PWR_PIN = -1;

const int8_t RTC_POWER_PIN = A1;
const int8_t RTC_SDA_PIN = A2;
const int8_t RTC_SCL_PIN = A3;

const int8_t POTENTIOMETER_PWR_PIN = A4;
const int8_t POTENTIOMETER_INPUT_PIN = A5;

const int8_t BATTERY_PIN = A7;
const double BATTERY_RESISTORS_RATIO = 0.4910588235; // Ratio of the voltage divider resistors (R1 / (R1 + R2))

const uint32_t BATTERY_CHARGING_MILLIVOLTS = 4300; // Millivolts above which the battery is considered charging
const uint8_t BATTERY_PERCENT_EMPTY = 5;
const uint8_t BATTERY_PERCENT_CRITICAL = 10;
const uint8_t BATTERY_PERCENT_LOW = 25;

const char* WIFI_FILENAME = "/wifi.txt";

const char* TOC_FILENAME = "/toc.txt";

#if defined(ESP32)
const uint16_t POTENTIOMETER_INPUT_MAX = 4095; // Maximum value for the level input pin (12-bit ADC)
#else
const uint16_t LEVEL_INPUT_MAX = 1023; // Default for other platforms (10-bit ADC)
#endif // ESP32

// validate DAY_START and DAY_END
#if (DAY_START_HOUR < 0 || DAY_START_HOUR > 23)
#error "DAY_START_HOUR must be between 0 and 23"
#endif // DAY_START_HOUR
#if (DAY_END_HOUR < 0 || DAY_END_HOUR > 23)
#error "DAY_END_HOUR must be between 0 and 23"
#endif // DAY_END_HOUR

// validate REFRESH_MIN_INTERVAL_SECONDS and REFRESH_MAX_INTERVAL_SECONDS
#if (REFRESH_MIN_INTERVAL_SECONDS < (5 * SECONDS_IN_MINUTE) || REFRESH_MIN_INTERVAL_SECONDS > (2 * SECONDS_IN_HOUR))
#error "REFRESH_MIN_INTERVAL_SECONDS must be between 5 minutes and 2 hours"
#endif // REFRESH_MIN_INTERVAL_SECONDS

#if (REFRESH_MAX_INTERVAL_SECONDS < (10 * SECONDS_IN_MINUTE) || REFRESH_MAX_INTERVAL_SECONDS > (4 * SECONDS_IN_HOUR))
#error "REFRESH_MAX_INTERVAL_SECONDS must be between 10 minutes and 4 hours"
#endif // REFRESH_MAX_INTERVAL_SECONDS

#if (REFRESH_MIN_INTERVAL_SECONDS > REFRESH_MAX_INTERVAL_SECONDS)
#error "REFRESH_MIN_INTERVAL_SECONDS must be less than REFRESH_MAX_INTERVAL_SECONDS"
#endif // REFRESH_MIN_INTERVAL_SECONDS > REFRESH_MAX_INTERVAL_SECONDS

#if (REFRESH_MAX_INTERVAL_SECONDS > ((24 - DAY_END_HOUR) + DAY_START_HOUR) * SECONDS_IN_HOUR)
#error \
    "REFRESH_MAX_INTERVAL_SECONDS must be less than or equal to the number of seconds between DAY_START_HOUR and DAY_END_HOUR"
#endif // REFRESH_MAX_INTERVAL_SECONDS > (DAY_END_HOUR - DAY_START_HOUR) * SECONDS_IN_HOUR

// Include local configuration overrides
// Here you can define the local configuration overrides not to be committed to the repository

// validate battery settings
#if (BATTERY_NUM_READINGS < 1 || BATTERY_NUM_READINGS > 100)
#error "BATTERY_NUM_READINGS must be between 1 and 100"
#endif // BATTERY_NUM_READINGS
#if (BATTERY_DELAY_BETWEEN_READINGS < 1 || BATTERY_DELAY_BETWEEN_READINGS > 1000)
#error "BATTERY_DELAY_BETWEEN_READINGS must be between 0 and 1000 milliseconds"
#endif // BATTERY_DELAY_BETWEEN_READINGS

#include "config.local"

namespace photo_frame {
void print_config()
{
#if DEBUG_MODE
    Serial.println(F("Configuration:"));
    Serial.print(F("SD_CS_PIN: "));
    Serial.println(SD_CS_PIN);
    Serial.print(F("SD_MISO_PIN: "));
    Serial.println(SD_MISO_PIN);
    Serial.print(F("SD_MOSI_PIN: "));
    Serial.println(SD_MOSI_PIN);
    Serial.print(F("SD_SCK_PIN: "));
    Serial.println(SD_SCK_PIN);

    Serial.print(F("EPD_CS_PIN: "));
    Serial.println(EPD_CS_PIN);
    Serial.print(F("EPD_DC_PIN: "));
    Serial.println(EPD_DC_PIN);
    Serial.print(F("EPD_RST_PIN: "));
    Serial.println(EPD_RST_PIN);
    Serial.print(F("EPD_BUSY_PIN: "));
    Serial.println(EPD_BUSY_PIN);
    Serial.print(F("EPD_MISO_PIN: "));
    Serial.println(EPD_MISO_PIN);
    Serial.print(F("EPD_MOSI_PIN: "));
    Serial.println(EPD_MOSI_PIN);
    Serial.print(F("EPD_SCK_PIN: "));
    Serial.println(EPD_SCK_PIN);
    Serial.print(F("EPD_PWR_PIN: "));
    Serial.println(EPD_PWR_PIN);

    Serial.print(F("RTC_SDA_PIN: "));
    Serial.println(RTC_SDA_PIN);
    Serial.print(F("RTC_SCL_PIN: "));
    Serial.println(RTC_SCL_PIN);
    Serial.print(F("RTC_POWER_PIN: "));
    Serial.println(RTC_POWER_PIN);

    Serial.print(F("POTENTIOMETER_PWR_PIN: "));
    Serial.println(POTENTIOMETER_PWR_PIN);
    Serial.print(F("POTENTIOMETER_INPUT_PIN: "));
    Serial.println(POTENTIOMETER_INPUT_PIN);
    Serial.print(F("POTENTIOMETER_INPUT_MAX: "));
    Serial.println(POTENTIOMETER_INPUT_MAX);

    Serial.print(F("BATTERY_PIN: "));
    Serial.println(BATTERY_PIN);
    Serial.print(F("BATTERY_RESISTORS_RATIO: "));
    Serial.println(BATTERY_RESISTORS_RATIO);
    Serial.print(F("BATTERY_NUM_READINGS: "));
    Serial.println(BATTERY_NUM_READINGS);
    Serial.print(F("BATTERY_DELAY_BETWEEN_READINGS: "));
    Serial.println(BATTERY_DELAY_BETWEEN_READINGS);
    Serial.print(F("BATTERY_PERCENT_EMPTY: "));
    Serial.println(BATTERY_PERCENT_EMPTY);
    Serial.print(F("BATTERY_PERCENT_CRITICAL: "));
    Serial.println(BATTERY_PERCENT_CRITICAL);
    Serial.print(F("BATTERY_PERCENT_LOW: "));
    Serial.println(BATTERY_PERCENT_LOW);

    Serial.print(F("LED_BUILTIN: "));
    Serial.println(LED_BUILTIN);

    Serial.print(F("DAY_START_HOUR: "));
    Serial.println(DAY_START_HOUR);
    Serial.print(F("DAY_END_HOUR: "));
    Serial.println(DAY_END_HOUR);

    Serial.print(F("REFRESH_MIN_INTERVAL_SECONDS: "));
    Serial.println(REFRESH_MIN_INTERVAL_SECONDS);
    Serial.print(F("REFRESH_MAX_INTERVAL_SECONDS: "));
    Serial.println(REFRESH_MAX_INTERVAL_SECONDS);
    Serial.print(F("REFRESH_INTERVAL_SECONDS_LOW_BATTERY: "));
    Serial.println(REFRESH_INTERVAL_SECONDS_LOW_BATTERY);

    Serial.print(F("FONT_HEADER: "));
    Serial.println(FONT_HEADER);

    Serial.print(F("LOCALE: "));
    Serial.println(X_LOCALE(LOCALE));

    Serial.print(F("WIFI_FILENAME: "));
    Serial.println(WIFI_FILENAME);

    Serial.print(F("HAS_RGB_LED: "));
    Serial.println(HAS_RGB_LED ? "true" : "false");

    Serial.print(F("TOC_FILENAME: "));
    Serial.println(TOC_FILENAME);
    Serial.println(F("-----------------------------"));
#endif // DEBUG_MODE
}
} // namespace photo_frame