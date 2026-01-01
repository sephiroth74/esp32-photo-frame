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
        log_e("WiFi config file not found: %s", config_file);
        return error_type::WifiCredentialsNotFound;
    }

    fs::File file = sdCard.open(config_file, FILE_READ);
    if (!file) {
        log_e("Failed to open WiFi config file: %s", config_file);
        return error_type::SdCardFileOpenFailed;
    }

    _ssid     = file.readStringUntil('\n');
    _password = file.readStringUntil('\n');

    _ssid.trim();
    _password.trim();

    file.close();

    if (_ssid.isEmpty()) {
        log_e("SSID not found in config file");
        return error_type::WifiCredentialsNotFound;
    }

#ifdef DEBUG_WIFI
    log_d("Loaded WiFi credentials for SSID: %s", _ssid.c_str());
#endif // DEBUG_WIFI

    _initialized = true;
    return error_type::None;
}

photo_frame_error_t wifi_manager::init_with_config(const String& ssid, const String& password) {
    // skip if already initialized
    if (_initialized) {
        return error_type::None;
    }

    if (ssid.isEmpty()) {
        log_e("SSID is empty");
        return error_type::WifiCredentialsNotFound;
    }

    // Store as single network
    _network_count = 1;
    _networks[0].ssid = ssid;
    _networks[0].password = password;
    _ssid = ssid;
    _password = password;

#ifdef DEBUG_WIFI
    log_d("Loaded WiFi credentials for SSID: %s", _ssid.c_str());
#endif // DEBUG_WIFI

    _initialized = true;
    return error_type::None;
}

photo_frame_error_t wifi_manager::init_with_networks(const unified_config::wifi_config& wifi_config) {
    // skip if already initialized
    if (_initialized) {
        return error_type::None;
    }

    if (wifi_config.network_count == 0) {
        log_e("No WiFi networks configured");
        return error_type::WifiCredentialsNotFound;
    }

    // Copy all configured networks
    _network_count = wifi_config.network_count;
    for (uint8_t i = 0; i < _network_count; i++) {
        _networks[i].ssid = wifi_config.networks[i].ssid;
        _networks[i].password = wifi_config.networks[i].password;
        log_i("Loaded WiFi network #%d: %s", i + 1, _networks[i].ssid.c_str());
    }

    // Set first network as default (will be updated when connected)
    _ssid = _networks[0].ssid;
    _password = _networks[0].password;

    _initialized = true;
    log_i("WiFi manager initialized with %d network(s)", _network_count);
    return error_type::None;
}

void wifi_manager::set_timezone(const char* timezone) {
    log_i("Setting timezone to: %s", timezone);
    setenv("TZ", timezone, 1);
    tzset();
}

photo_frame_error_t wifi_manager::connect() {
    // Check if the WiFi manager is initialized
    if (!_initialized) {
        log_e("WiFi manager not initialized");
        return error_type::WifiCredentialsNotFound;
    }

    // then check if we're already connected
    if (_connected) {
        log_i("Already connected to WiFi");
        return error_type::None;
    }

    if (_network_count == 0) {
        log_e("No WiFi networks configured");
        return error_type::WifiCredentialsNotFound;
    }

    WiFi.mode(WIFI_STA);

    const uint8_t maxRetriesPerNetwork = 2;
    const unsigned long baseDelay = 2000; // 2 seconds base delay
    bool connected = false;

    // Try each configured network in order
    for (uint8_t network_idx = 0; network_idx < _network_count && !connected; network_idx++) {
        const String& ssid = _networks[network_idx].ssid;
        const String& password = _networks[network_idx].password;

        if (ssid.isEmpty()) {
            log_w("Network #%d has empty SSID, skipping", network_idx + 1);
            continue;
        }

        log_i("Trying WiFi network #%d/%d: %s", network_idx + 1, _network_count, ssid.c_str());

        // Try connecting to this network with retries
        for (uint8_t attempt = 0; attempt < maxRetriesPerNetwork && !connected; attempt++) {
            log_i("Connection attempt %u/%u for network: %s",
                  attempt + 1, maxRetriesPerNetwork, ssid.c_str());

            WiFi.begin(ssid.c_str(), password.c_str());

            unsigned long timeout = millis() + CONNECTION_TIMEOUT_MS;
            wl_status_t connection_status = WiFi.status();
            while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
                delay(500);
                connection_status = WiFi.status();
            }

            if (connection_status == WL_CONNECTED) {
                connected = true;
                _connected = true;
                _ssid = ssid;  // Update to reflect currently connected network
                _password = password;
                log_i("WiFi connected to '%s'! IP address: %s", ssid.c_str(), WiFi.localIP().toString().c_str());
            } else {
                log_w("Failed to connect to '%s' (attempt %u/%u)",
                      ssid.c_str(), attempt + 1, maxRetriesPerNetwork);

                WiFi.disconnect(true);

                // Exponential backoff with jitter for retries (only within same network)
                if (attempt < maxRetriesPerNetwork - 1) {
                    unsigned long backoffDelay = baseDelay * (1UL << attempt);
                    unsigned long jitter = random(0, backoffDelay / 4);
                    backoffDelay += jitter;

                    log_i("Retrying same network in %lu ms...", backoffDelay);
                    delay(backoffDelay);
                }
            }
        }

        // If this network failed, try next network after a short delay
        if (!connected && network_idx < _network_count - 1) {
            log_i("Network '%s' failed, trying next network...", ssid.c_str());
            delay(1000);
        }
    }

    if (!connected) {
        log_e("Failed to connect to any configured WiFi network!");
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
        log_e("Not connected to WiFi, cannot fetch time");
        if (error) {
            *error = error_type::WifiConnectionFailed;
        }
        return DateTime(); // Invalid DateTime
    }

    set_timezone(TIMEZONE);
    configTime(0, 0, NTP_SERVER1, NTP_SERVER2);

    log_i("Waiting for NTP time sync...");

    unsigned long startTime = millis();
    time_t now              = time(nullptr);
    while (now < 1000000000 &&
           (millis() - startTime) < (NTP_TIMEOUT * 1000)) { // NTP_TIMEOUT in seconds
        delay(200);
        now = time(nullptr);
    }

    if (now < 1000000000) {
        log_e("NTP time sync timeout!");
        if (error) {
            *error = error_type::WifiConnectionFailed;
        }
        return DateTime(); // Invalid DateTime
    }

    log_i("Time synchronized.");

    // Print the current time
    struct tm timeinfo;

    if (getLocalTime(&timeinfo)) {
        set_timezone(TIMEZONE);

        getLocalTime(&timeinfo);

        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%A, %B %d %Y %H:%M:%S", &timeinfo);
        log_i("Current time is: %s", timeStr);

        return DateTime(timeinfo.tm_year + 1900,
                        timeinfo.tm_mon + 1,
                        timeinfo.tm_mday,
                        timeinfo.tm_hour,
                        timeinfo.tm_min,
                        timeinfo.tm_sec);
    }

    log_e("Failed to obtain time from NTP server within timeout");
    if (error) {
        *error = error_type::HttpGetFailed;
    }
    return DateTime(); // Invalid DateTime
}

void wifi_manager::disconnect() {
    if (_connected) {
        log_i("Disconnecting from WiFi...");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        _connected = false;
        log_i("WiFi disconnected and turned off");
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