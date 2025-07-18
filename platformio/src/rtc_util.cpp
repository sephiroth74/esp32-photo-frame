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

DateTime fetch_remote_datetime(SDCard& sdCard)
{
    Serial.println(F("Fetching time from WiFi..."));
    DateTime result = DateTime((uint32_t)0);

    fs::File file = sdCard.open(WIFI_FILENAME, FILE_READ); // Open the file to write the time
    if (!file) {
        Serial.println(F("Failed to open WiFi credentials file!"));
        Serial.println(F("Please check if the file exists on the SD card."));
        return result;
    }

    String wifi_ssid = file.readStringUntil('\n');
    String wifi_password = file.readStringUntil('\n');

    wifi_ssid.trim();
    wifi_password.trim();

    file.close();

    if (wifi_ssid.isEmpty() || wifi_password.isEmpty()) {
        Serial.println(F("WiFi credentials are empty!"));
        Serial.println(F("Please check the WiFi credentials file on the SD card."));
        return result;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    unsigned long timeout = millis() + WIFI_CONNECT_TIMEOUT;
    wl_status_t connection_status = WiFi.status();
    while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
        Serial.print(".");
        delay(200);
        connection_status = WiFi.status();
    }
    Serial.println();

    if (connection_status != WL_CONNECTED) {
        Serial.println(F("Failed to connect to WiFi!"));
        Serial.println(F("Please check your WiFi credentials."));
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return result;
    } else {
        Serial.println(F("Connected to WiFi!"));
    }

    // acquire the time from the NTP server
    configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2);

    // wait for the time to be set
    Serial.print(F("Waiting for time."));
    while (time(nullptr) < 1000000000) {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F("done."));

    // Set the RTC to the time we just set
    // get the local time

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, NTP_TIMEOUT)) {
        Serial.println(F("Failed to obtain time from NTP server!"));
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return result;
    }

    result = DateTime(timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

    // disconnect the Wifi and turn off the WiFi module
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    return result;
} // featch_remote_datetime

DateTime fetch_datetime(SDCard& sdCard, bool reset, photo_frame_error_t* error)
{
    Serial.print("Initializing RTC, reset: ");
    Serial.println(reset ? "true" : "false");

    DateTime now;
    RTC_DS3231 rtc;

#if USE_RTC
    pinMode(RTC_POWER_PIN, OUTPUT);
    digitalWrite(RTC_POWER_PIN, HIGH); // Power on the RTC
    delay(2000); // Wait for RTC to power up

    Wire.setPins(RTC_SDA_PIN, RTC_SCL_PIN);

    if (!rtc.begin(&Wire)) {
        // set the error if provided
        Serial.println(F("Couldn't find RTC"));
        now = fetch_remote_datetime(sdCard);
        if (!now.isValid()) {
            Serial.println(F("Failed to fetch time from WiFi!"));
            if (error) {
                *error = error_type::RTCInitializationFailed;
            }
        }

    } else {
        Serial.println(F("RTC initialized successfully!"));
        rtc.disable32K();
        if (rtc.lostPower() || reset) {
            // Set the time to a default value if the RTC lost power
            Serial.println(F("RTC lost power (or force is true), setting the time!"));

            now = fetch_remote_datetime(sdCard);
            if (!now.isValid()) {
                Serial.println(F("Failed to fetch time from WiFi!"));
                Serial.println(F("Using compile time as fallback"));
                now = DateTime(__DATE__, __TIME__); // Use compile time as a fallback
                rtc.adjust(now);
            } else {
                rtc.adjust(now);
            }
        } else {
            Serial.println(F("RTC is running!"));
            now = rtc.now();
        }
    }

    digitalWrite(RTC_POWER_PIN, LOW); // Power off the RTC

#else
    Serial.println(F("RTC module is not used, fetching time from WiFi..."));
    now = fetch_remote_datetime(sdCard);
    if (!now.isValid()) {
        Serial.println(F("Failed to fetch time from WiFi!"));
        if (error) {
            *error = error_type::RTCInitializationFailed;
        }
    }
#endif
    return now;
}

void format_date_time(time_t time, char* buffer, const uint8_t buffer_size, const char* format)
{
    struct tm* tm_info = localtime(&time);
    memset(buffer, 0, buffer_size);

    Serial.print("Size of buffer: ");
    Serial.println(buffer_size);

    strftime(buffer, buffer_size, format, tm_info);
}

// format_date_time

} // namespace photo_frame