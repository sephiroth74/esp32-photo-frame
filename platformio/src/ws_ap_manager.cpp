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

#include "ws_ap_manager.h"
#include <Arduino.h>
#include <iomanip>
#include <sstream>

namespace photo_frame {
namespace ws {

bool WSAPManager::begin() {
    ssid_ = generateSSID();

    log_i("[WS-AP] Starting Access Point");
    log_i("[WS-AP] SSID: %s (open, no password)", ssid_.c_str());
    log_i("[WS-AP] Channel: %d", WS_AP_CHANNEL);
    log_i("[WS-AP] Max connections: %d", WS_AP_MAX_CONNECTIONS);

    // Configure soft AP
    WiFi.mode(WIFI_AP);

    // Set IP configuration
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    if (!WiFi.softAPConfig(local_ip, gateway, subnet)) {
        log_e("[WS-AP] Failed to configure AP IP");
        return false;
    }

    // Start the AP (open network - no password)
    bool result = WiFi.softAP(ssid_.c_str(),        // SSID
                              WS_AP_PASSWORD,       // Password (nullptr = open)
                              WS_AP_CHANNEL,        // Channel
                              false,                // SSID hidden (false = visible)
                              WS_AP_MAX_CONNECTIONS // Max connections
    );

    if (!result) {
        log_e("[WS-AP] Failed to start Access Point");
        return false;
    }

    isRunning_ = true;

    log_i("[WS-AP] Access Point started successfully");
    log_i("[WS-AP] IP address: %s", WiFi.softAPIP().toString().c_str());
    log_i("[WS-AP] MAC address: %s", WiFi.softAPmacAddress().c_str());

    return true;
}

void WSAPManager::stop() {
    if (!isRunning_) {
        return;
    }

    log_i("[WS-AP] Stopping Access Point");
    WiFi.softAPdisconnect(true); // turn_off_wifi=true
    WiFi.mode(WIFI_OFF);
    isRunning_ = false;

    log_i("[WS-AP] Access Point stopped");
}

bool WSAPManager::isClientConnected() const {
    if (!isRunning_) {
        return false;
    }
    return WiFi.softAPgetStationNum() > 0;
}

std::string WSAPManager::getSSID() const { return ssid_; }

std::string WSAPManager::getIP() const {
    if (!isRunning_) {
        return "";
    }
    return WiFi.softAPIP().toString().c_str();
}

std::string WSAPManager::generateSSID() {
    // Get MAC address
    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);

    // Extract last 2 bytes (4 hex digits)
    std::stringstream ss;
    ss << WS_AP_SSID_PREFIX << "-" << std::uppercase << std::hex << std::setfill('0')
       << std::setw(2) << (int)mac[4] << std::setw(2) << (int)mac[5];

    return ss.str();
}

} // namespace ws
} // namespace photo_frame
