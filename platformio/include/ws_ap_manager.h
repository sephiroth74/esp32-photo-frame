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

#ifndef WS_AP_MANAGER_H
#define WS_AP_MANAGER_H

#include "config.h"
#include <WiFi.h>
#include <string>

namespace photo_frame {
namespace ws {

    // WiFi AP Configuration Constants
    static constexpr const char* WS_AP_PASSWORD = nullptr; // Open network (no password)
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
