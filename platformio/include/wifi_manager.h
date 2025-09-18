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

#pragma once

#include "RTClib.h"
#include "config.h"
#include "errors.h"
#include "sd_card.h"
#include <WiFi.h>

namespace photo_frame {

/**
 * @brief WiFi connection manager for ESP32 photo frame.
 *
 * This class manages WiFi connectivity including connection establishment,
 * network time protocol (NTP) synchronization, and timezone configuration.
 * It handles WiFi credentials loading from configuration files and provides
 * robust connection management with timeout handling.
 */
class wifi_manager {
  public:
    /**
     * @brief Constructor for wifi_manager.
     *
     * Initializes the WiFi manager with default settings. WiFi credentials
     * and connection must be configured separately using init() and connect().
     */
    wifi_manager();

    /**
     * @brief Destructor for wifi_manager.
     *
     * Ensures proper cleanup of WiFi resources and disconnects from network
     * if still connected.
     */
    ~wifi_manager();

    /**
     * @brief Initialize WiFi manager with configuration from file.
     *
     * Loads WiFi credentials (SSID and password) from the specified configuration
     * file on the SD card. The configuration file should contain WiFi settings
     * in the expected format.
     *
     * @param config_file Path to configuration file containing WiFi credentials
     * @param sdCard Reference to SD card instance for file access
     * @return photo_frame_error_t Error status (None on success)
     * @note Configuration file must be accessible on the SD card
     */
    photo_frame_error_t init(const char* config_file, sd_card& sdCard);

    /**
     * @brief Connect to WiFi network using stored credentials.
     *
     * Attempts to establish a connection to the WiFi network using the
     * credentials loaded during initialization. Includes timeout handling
     * and connection status monitoring.
     *
     * @return photo_frame_error_t Error status (None on successful connection)
     * @note init() must be called successfully before attempting to connect
     */
    photo_frame_error_t connect();

    /**
     * @brief Fetch current date and time from NTP servers.
     *
     * Synchronizes with network time protocol servers to get accurate current
     * date and time. Requires an active WiFi connection to function properly.
     *
     * @param error Optional pointer to store error information if fetch fails
     * @return DateTime object containing current date and time, or invalid DateTime on error
     * @note WiFi connection must be established before calling this function
     */
    DateTime fetch_datetime(photo_frame_error_t* error = nullptr);

    /**
     * @brief Set the timezone for time calculations.
     *
     * Configures the local timezone offset for proper time display.
     * Affects subsequent date/time operations and NTP synchronization.
     *
     * @param timezone Timezone string (e.g., "EST5EDT", "PST8PDT", "GMT0")
     * @note Timezone string format follows POSIX timezone specification
     */
    void set_timezone(const char* timezone);

    /**
     * @brief Disconnect from the WiFi network.
     *
     * Gracefully terminates the WiFi connection while maintaining the ability
     * to reconnect later. Does not reset stored credentials or configuration.
     */
    void disconnect();

    /**
     * @brief Check if currently connected to WiFi network.
     *
     * @return true if connected to WiFi network, false otherwise
     * @note This reflects the current connection status, not credential validity
     */
    bool is_connected() const;

    /**
     * @brief Get the current IP address assigned by the network.
     *
     * @return String containing the IP address, or empty string if not connected
     * @note Only valid when connected to a WiFi network
     */
    String get_ip_address() const;

    /**
     * @brief Get the SSID of the currently connected network.
     *
     * @return String containing the network SSID, or empty string if not connected
     * @note Returns the SSID from stored credentials, not necessarily the connected network
     */
    String get_ssid() const;

    /**
     * @brief Terminate WiFi operations and free resources.
     *
     * Performs complete shutdown of WiFi subsystem including disconnection
     * and resource cleanup. After calling this method, the wifi_manager
     * instance should be reinitialized before use.
     */
    void end();

  private:
    String _ssid;
    String _password;
    bool _initialized;
    bool _connected;

    static const unsigned long CONNECTION_TIMEOUT_MS;
};

} // namespace photo_frame