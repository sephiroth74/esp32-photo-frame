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

#include "rtc_util.h"
#include "config.h"
#include <WiFi.h>

namespace photo_frame {

namespace rtc_utils {

// Global variable to store pending RTC update
static DateTime pendingRtcUpdate = DateTime((uint32_t)0);
static bool needsRtcUpdate = false;

DateTime fetch_remote_datetime(wifi_manager& wifiManager, photo_frame_error_t* error) {
    if (!wifiManager.is_connected()) {
        *error = wifiManager.connect();
        if (error && *error != error_type::None) {
            Serial.println(F("Failed to connect to WiFi!"));
            return DateTime();
        }
    }
    return wifiManager.fetch_datetime(error);
}

DateTime fetch_datetime(wifi_manager& wifiManager, bool reset, photo_frame_error_t* error) {
    Serial.print("Initializing RTC, reset: ");
    Serial.println(reset ? "true" : "false");

    DateTime now;
    RTC_DS3231 rtc;

#ifdef USE_RTC

    // Configure I2C pins for RTC - this is safe to call multiple times
    Wire.setPins(RTC_SDA_PIN, RTC_SCL_PIN);
    // Note: begin() is idempotent and safe to call multiple times
    Wire.begin();
    // Lower I2C clock speed to reduce ESP32-C6 WiFi interference
    Wire.setClock(100000); // 100kHz instead of default 400kHz

    if (!rtc.begin(&Wire)) {
        // set the error if provided
        Serial.println(F("Couldn't find RTC"));
        
        // I2C is already shut down by caller to prevent ESP32-C6 WiFi interference
        now = fetch_remote_datetime(wifiManager, error);
        
        if (!now.isValid() || (error && *error != error_type::None)) {
            Serial.println(F("Failed to fetch time from WiFi!"));
            if (error) {
                *error = error_type::RTCInitializationFailed;
            }
        } else {
            Serial.println(F("Using NTP time only - RTC hardware unavailable"));
        }

    } else {
        Serial.println(F("RTC initialized successfully!"));
        rtc.disable32K();
        if (rtc.lostPower() || reset) {
            // Set the time to a default value if the RTC lost power
            Serial.println(F("RTC lost power (or force is true), setting the time!"));
            
            // I2C is already shut down by caller to prevent ESP32-C6 WiFi interference
            now = fetch_remote_datetime(wifiManager, error);
            
            if (error && *error != error_type::None) {
                Serial.println(F("Failed to fetch time from WiFi!"));
                Serial.println(F("Cannot synchronize RTC - both RTC and WiFi failed"));
                return DateTime(); // Return invalid DateTime
            } else {
                // Store the time for RTC update after I2C is restarted by caller
                pendingRtcUpdate = now;
                needsRtcUpdate = true;
                Serial.println(F("Time fetched from WiFi, RTC will be updated after I2C restart"));
            }
        } else {
            Serial.println(F("RTC is running!"));
            now = rtc.now();
        }
    }

#else
    Serial.println(F("RTC module is not used, fetching time from WiFi..."));
    now = fetch_remote_datetime(wifiManager, error);
    if (!now.isValid() || (error && *error != error_type::None)) {
        Serial.println(F("Failed to fetch time from WiFi!"));
        if (error) {
            *error = error_type::RTCInitializationFailed;
        }
    }
#endif
    return now;
}

bool update_rtc_after_restart(const DateTime& dateTime) {
#ifdef USE_RTC
    if (needsRtcUpdate && pendingRtcUpdate.isValid()) {
        Serial.println(F("Updating RTC with time fetched during WiFi operations..."));
        
        RTC_DS3231 rtc;
        // I2C should already be initialized by caller, don't reinitialize
        
        if (rtc.begin(&Wire)) {
            rtc.adjust(pendingRtcUpdate);
            Serial.print(F("RTC updated successfully with time: "));
            Serial.println(pendingRtcUpdate.timestamp(DateTime::TIMESTAMP_FULL));
            
            // Clear the pending update
            needsRtcUpdate = false;
            pendingRtcUpdate = DateTime((uint32_t)0);
            return true;
        } else {
            Serial.println(F("Failed to reconnect to RTC for time update"));
            return false;
        }
    } else if (dateTime.isValid()) {
        // Use the provided dateTime if no pending update
        Serial.println(F("Updating RTC with provided time..."));
        
        RTC_DS3231 rtc;
        // I2C should already be initialized by caller, don't reinitialize
        
        if (rtc.begin(&Wire)) {
            rtc.adjust(dateTime);
            Serial.print(F("RTC updated successfully with provided time: "));
            Serial.println(dateTime.timestamp(DateTime::TIMESTAMP_FULL));
            return true;
        } else {
            Serial.println(F("Failed to connect to RTC for time update"));
            return false;
        }
    }
#endif
    return false;
}

void format_date_time(time_t time, char* buffer, const uint8_t buffer_size, const char* format) {
    struct tm* tm_info = localtime(&time);
    memset(buffer, 0, buffer_size);

    Serial.print("Size of buffer: ");
    Serial.println(buffer_size);

    strftime(buffer, buffer_size, format, tm_info);
}

// format_date_time

} // namespace rtc_utils

} // namespace photo_frame