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

#ifdef USE_RTC

#if defined(RTC_CLASS_PCF8523)
RTC_PCF8523 rtc;
#elif defined(RTC_CLASS_DS3231)
RTC_DS3231 rtc;
#endif // RTC_CLASS == RTC_PCF8523

#endif // USE_RTC

namespace photo_frame {

namespace rtc_utils {

DateTime fetch_remote_datetime(wifi_manager& wifiManager, photo_frame_error_t* error) {
    if (!wifiManager.is_connected()) {
        *error = wifiManager.connect();
        if (error && *error != error_type::None) {
            log_e("[rtc_util] Failed to connect to WiFi!");
            return DateTime();
        }
    }
    return wifiManager.fetch_datetime(error);
}

DateTime fetch_datetime(wifi_manager& wifiManager, bool reset, photo_frame_error_t* error) {
    log_i("[rtc_util] Initializing RTC, reset: %s", reset ? "true" : "false");

    DateTime now;
    
#ifdef USE_RTC
    // Configure I2C pins for RTC - this is safe to call multiple times
    Wire.setPins(RTC_SDA_PIN, RTC_SCL_PIN);
    // Note: begin() is idempotent and safe to call multiple times
    Wire.begin();
    // Lower I2C clock speed to reduce ESP32-C6 WiFi interference
    Wire.setClock(100000); // 100kHz instead of default 400kHz

    if (!rtc.begin(&Wire)) {
        // set the error if provided
        log_e("[rtc_util] Couldn't find PCF8523 RTC");

        // I2C is already shut down by caller to prevent ESP32-C6 WiFi interference
        now = fetch_remote_datetime(wifiManager, error);

        if (!now.isValid() || (error && *error != error_type::None)) {
            log_e("[rtc_util] Failed to fetch time from WiFi!");
            if (error) {
                *error = error_type::RTCInitializationFailed;
            }
        } else {
            log_i("[rtc_util] Using NTP time only - RTC hardware unavailable");
        }

    } else {
#ifdef RTC_CLASS_PCF8523
        log_i("[rtc_util] PCF8523 RTC initialized successfully!");
        rtc.start();
#else
        log_i("[rtc_util] DS3231 RTC initialized successfully!");
        rtc.begin();
        #endif // RTC_CLASS == RTC_PCF8523

        #ifdef RTC_CLASS_DS3231
        rtc.disable32K(); // Disable 32kHz output to save power (DS3231 only)
        #endif
        if (rtc.lostPower() || reset) {
            // Set the time to a default value if the RTC lost power
            log_i("[rtc_util] RTC lost power (or force is true), setting the time!");

            now = fetch_remote_datetime(wifiManager, error);

            if (error && *error != error_type::None) {
                log_e("[rtc_util] Failed to fetch time from WiFi!");
                log_e("[rtc_util] Cannot synchronize RTC - both RTC and WiFi failed");
                return DateTime(); // Return invalid DateTime
            } else {
                // Update RTC immediately with correct time
                rtc.adjust(now);
                log_i("[rtc_util] RTC updated with NTP time: %s", now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
            }
        } else {
            log_i("[rtc_util] RTC is running!");
            now = rtc.now();

            // Validate RTC time - check if it's reasonable (between 2020 and 2100)
            if (!now.isValid() || now.year() < 2020 || now.year() > 2100) {
                log_w("[rtc_util] RTC time is invalid or unreasonable, fetching from WiFi...");

                // Get correct time from NTP
                now = fetch_remote_datetime(wifiManager, error);

                if (error && *error != error_type::None) {
                    log_e("[rtc_util] Failed to fetch time from WiFi!");
                    log_e("[rtc_util] Cannot synchronize - both RTC time invalid and WiFi failed");
                    return DateTime(); // Return invalid DateTime
                } else {
                    // Update RTC immediately with correct time
                    rtc.adjust(now);
                    log_i("[rtc_util] RTC updated with correct NTP time: %s", now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
                }
            } else {
                log_i("[rtc_util] Using RTC time: %s", now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
            }
        }
        // Stop the RTC to save power - it will keep running internally
#ifdef RTC_CLASS_PCF8523
        rtc.stop();
#endif // RTC_CLASS == RTC_PCF8523
    }

#else
    log_i("[rtc_util] RTC module is not used, fetching time from WiFi...");
    now = fetch_remote_datetime(wifiManager, error);
    if (!now.isValid() || (error && *error != error_type::None)) {
        log_e("[rtc_util] Failed to fetch time from WiFi!");
        if (error) {
            *error = error_type::RTCInitializationFailed;
        }
    }
#endif
    return now;
}

void format_date_time(time_t time, char* buffer, const uint8_t buffer_size, const char* format) {
    struct tm* tm_info = localtime(&time);
    memset(buffer, 0, buffer_size);
    strftime(buffer, buffer_size, format, tm_info);
}

// format_date_time

} // namespace rtc_utils

} // namespace photo_frame