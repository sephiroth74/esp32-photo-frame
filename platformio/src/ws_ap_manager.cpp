// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
  ss << WS_AP_SSID_PREFIX << "-" << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)mac[4] << std::setw(2)
     << (int)mac[5];

  return ss.str();
}

} // namespace ws
} // namespace photo_frame
