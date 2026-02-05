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

#ifndef WS_AP_MANAGER_H
#define WS_AP_MANAGER_H

#include "config.h"
#include <WiFi.h>
#include <string>

namespace photo_frame {
namespace ws {

// WiFi AP Configuration Constants
static constexpr const char* WS_AP_PASSWORD    = nullptr; // Open network (no password)
static constexpr uint8_t WS_AP_MAX_CONNECTIONS = 1;

/**
 * @class WSAPManager
 * @brief Manages ESP32 as Wi-Fi Access Point for WebSocket image transfer
 *
 * Creates an open SoftAP with SSID "PhotoFrame-XXXX" where XXXX = last 4 digits of MAC.
 * Configures IP stack: 192.168.4.1 (AP) with DHCP server.
 */
class WSAPManager {
  public:
    /**
     * @brief Initialize and start the WiFi Access Point
     * @return true if AP started successfully, false otherwise
     */
    bool begin();

    /**
     * @brief Stop the AP and cleanup
     */
    void stop();

    /**
     * @brief Check if a client is currently connected to the AP
     * @return true if at least one client is connected
     */
    bool isClientConnected() const;

    /**
     * @brief Get the current SSID of the AP
     * @return SSID string
     */
    std::string getSSID() const;

    /**
     * @brief Get the AP's IP address
     * @return IP address as string (typically "192.168.4.1")
     */
    std::string getIP() const;

  private:
    std::string ssid_;
    bool isRunning_ = false;

    /**
     * @brief Generate SSID with last 4 digits of MAC address
     * @return Full SSID string (e.g., "PhotoFrame-A1B2")
     */
    std::string generateSSID();
};

} // namespace ws
} // namespace photo_frame

#endif // WS_AP_MANAGER_H
