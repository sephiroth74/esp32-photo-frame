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

#include "wifi_manager.h"
#include <Arduino.h>

namespace photo_frame {

const unsigned long wifi_manager::CONNECTION_TIMEOUT_MS = 30000;

wifi_manager::wifi_manager() : _initialized(false), _connected(false) {}

wifi_manager::~wifi_manager() { disconnect(); }

photo_frame_error_t wifi_manager::init(const char* config_file, sd_card& sdCard) {
    // skip if already initialized
    if (_initialized) {
        return error_type::None;
    }

    if (!sdCard.file_exists(config_file)) {
        Serial.print(F("[wifi_manager] WiFi config file not found: "));
        Serial.println(config_file);
        return error_type::WifiCredentialsNotFound;
    }

    fs::File file = sdCard.open(config_file, FILE_READ);
    if (!file) {
        Serial.print(F("[wifi_manager] Failed to open WiFi config file: "));
        Serial.println(config_file);
        return error_type::SdCardFileOpenFailed;
    }

    _ssid     = file.readStringUntil('\n');
    _password = file.readStringUntil('\n');

    _ssid.trim();
    _password.trim();

    file.close();

    if (_ssid.isEmpty()) {
        Serial.println(F("[wifi_manager] SSID not found in config file"));
        return error_type::WifiCredentialsNotFound;
    }

    Serial.print(F("[wifi_manager] Loaded WiFi credentials for SSID: "));
    Serial.println(_ssid);

    _initialized = true;
    return error_type::None;
}

void wifi_manager::set_timezone(const char* timezone) {
    Serial.print(F("[wifi_manager] Setting timezone to: "));
    Serial.println(timezone);
    setenv("TZ", timezone, 1);
    tzset();
}

photo_frame_error_t wifi_manager::connect() {
    // Check if the WiFi manager is initialized
    if (!_initialized) {
        Serial.println(F("[wifi_manager] WiFi manager not initialized"));
        return error_type::WifiCredentialsNotFound;
    }

    // then check if we're already connected
    if (_connected) {
        Serial.println(F("[wifi_manager] Already connected to WiFi"));
        return error_type::None;
    }

    if (_ssid.isEmpty() || _password.isEmpty()) {
        Serial.println(F("[wifi_manager] WiFi credentials are empty"));
        return error_type::WifiCredentialsNotFound;
    }

    Serial.print(F("[wifi_manager] Connecting to WiFi network: "));
    Serial.println(_ssid);

    WiFi.mode(WIFI_STA);

    const uint8_t maxRetries      = 2;
    const unsigned long baseDelay = 2000; // 2 seconds base delay
    bool connected                = false;

    for (uint8_t attempt = 0; attempt < maxRetries && !connected; attempt++) {
        Serial.print(F("[wifi_manager] WiFi connection attempt "));
        Serial.print(attempt + 1);
        Serial.print(F("/"));
        Serial.println(maxRetries);

        WiFi.begin(_ssid.c_str(), _password.c_str());

        unsigned long timeout         = millis() + CONNECTION_TIMEOUT_MS;
        wl_status_t connection_status = WiFi.status();
        while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
            Serial.print(".");
            delay(500);
            connection_status = WiFi.status();
        }
        Serial.println();

        if (connection_status == WL_CONNECTED) {
            connected  = true;
            _connected = true;
            Serial.print(F("[wifi_manager] WiFi connected! IP address: "));
            Serial.println(WiFi.localIP());
        } else {
            Serial.print(F("[wifi_manager] WiFi connection failed (attempt "));
            Serial.print(attempt + 1);
            Serial.println(F(")"));

            WiFi.disconnect(true);

            // Exponential backoff with jitter for retries
            if (attempt < maxRetries - 1) {
                unsigned long backoffDelay = baseDelay * (1UL << attempt);
                unsigned long jitter       = random(0, backoffDelay / 4); // Add up to 25% jitter
                backoffDelay += jitter;

                Serial.print(F("[wifi_manager] Retrying in "));
                Serial.print(backoffDelay);
                Serial.println(F("ms..."));
                delay(backoffDelay);
            }
        }
    }

    if (!connected) {
        Serial.println(F("[wifi_manager] Failed to connect to WiFi after all retries!"));
        _connected = false;
        return error_type::WifiConnectionFailed;
    }

    return error_type::None;
}

DateTime wifi_manager::fetch_datetime(photo_frame_error_t* error) {
    if (error) {
        *error = error_type::None;
    }

    if (!is_connected()) {
        Serial.println(F("[wifi_manager] Not connected to WiFi, cannot fetch time"));
        if (error) {
            *error = error_type::WifiConnectionFailed;
        }
        return DateTime(); // Invalid DateTime
    }

    set_timezone(TIMEZONE);
    configTime(0, 0, NTP_SERVER1, NTP_SERVER2);

    Serial.print(F("[wifi_manager] Waiting for NTP time sync..."));

    unsigned long startTime = millis();
    time_t now = time(nullptr);
    while (now < 1000000000 && (millis() - startTime) < (NTP_TIMEOUT * 1000)) { // NTP_TIMEOUT in seconds
        delay(200);
        Serial.print(".");
        now = time(nullptr);
    }
    
    if (now < 1000000000) {
        Serial.println("\nNTP time sync timeout!");
        if (error) {
            *error = error_type::WifiConnectionFailed;
        }
        return DateTime(); // Invalid DateTime
    }
    
    Serial.println("\nTime synchronized.");

    // Print the current time
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
        set_timezone(TIMEZONE);

        getLocalTime(&timeinfo);

        Serial.print(F("[wifi_manager] Current time is: "));
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

        return DateTime(timeinfo.tm_year + 1900,
                        timeinfo.tm_mon + 1,
                        timeinfo.tm_mday,
                        timeinfo.tm_hour,
                        timeinfo.tm_min,
                        timeinfo.tm_sec);
    }

    Serial.println();
    Serial.println(F("[wifi_manager] Failed to obtain time from NTP server within timeout"));
    if (error) {
        *error = error_type::HttpGetFailed;
    }
    return DateTime(); // Invalid DateTime
}

void wifi_manager::disconnect() {
    if (_connected) {
        Serial.println(F("[wifi_manager] Disconnecting from WiFi..."));
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        _connected = false;
        Serial.println(F("[wifi_manager] WiFi disconnected and turned off"));
    }
}

void wifi_manager::end() { disconnect(); }

bool wifi_manager::is_connected() const { return _connected && WiFi.status() == WL_CONNECTED; }

String wifi_manager::get_ip_address() const {
    if (is_connected()) {
        return WiFi.localIP().toString();
    }
    return String("");
}

String wifi_manager::get_ssid() const { return _ssid; }

} // namespace photo_frame