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
#include "unified_config.h"
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
class WifiManager {
  public:
    /**
     * @brief Constructor for WifiManager.
     *
     * Initializes the WiFi manager with default settings. WiFi credentials
     * and connection must be configured separately using init() and connect().
     */
    WifiManager();

    /**
     * @brief Destructor for WifiManager.
     *
     * Ensures proper cleanup of WiFi resources and disconnects from network
     * if still connected.
     */
    ~WifiManager();

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
    photo_frame_error_t init(const char* config_file, SdCard& sdCard);

    /**
     * @brief Initialize WiFi manager with direct credentials (single network).
     *
     * Sets WiFi credentials directly without reading from a configuration file.
     * This method is used with the unified configuration system where credentials
     * are already loaded from the main configuration file.
     *
     * @param ssid WiFi network SSID
     * @param password WiFi network password
     * @return photo_frame_error_t Error status (None on success)
     * @deprecated Use init_with_networks() for multiple network support
     */
    photo_frame_error_t initWithConfig(const String& ssid, const String& password);

    /**
     * @brief Initialize WiFi manager with multiple network credentials (v0.11.0+).
     *
     * Sets multiple WiFi credentials from the unified configuration system.
     * Supports up to WIFI_MAX_NETWORKS (default: 3) networks.
     * The manager will try to connect to each network in order until one succeeds.
     *
     * FAILOVER BEHAVIOR:
     * - Each network is tried with up to 3 connection attempts
     * - Exponential backoff with jitter between retries on same network
     * - 1-second delay when switching to next network
     * - Connection timeout: 10 seconds per attempt
     * - Detailed logging shows which network is being attempted
     *
     * USE CASES:
     * - Primary home network + backup mobile hotspot
     * - Home network + office network + travel hotspot
     * - Multiple networks at different locations
     *
     * @param wifi_config Reference to wifi_config structure containing networks array
     * @return photo_frame_error_t Error status (None on success)
     * @note Replaces init_with_config() for multi-network scenarios
     * @see init_with_config() for single-network backward compatibility
     */
    photo_frame_error_t initWithNetworks(const unified_config::wifi_config& wifi_config);

    /**
     * @brief Connect to WiFi network(s) using stored credentials.
     *
     * Attempts to establish a connection to WiFi networks using the credentials
     * loaded during initialization. For multi-network configurations (v0.11.0+),
     * tries each network sequentially until successful connection is established.
     *
     * CONNECTION PROCESS (multi-network):
     * 1. Try first network (up to 3 attempts with exponential backoff)
     * 2. If failed, try second network (up to 3 attempts)
     * 3. If failed, try third network (up to 3 attempts)
     * 4. Continue until connection succeeds or all networks exhausted
     *
     * @return photo_frame_error_t Error status (None on successful connection)
     * @note init_with_networks() or init_with_config() must be called first
     * @note Includes automatic reconnection logic with backoff
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
    DateTime fetchDatetime(photo_frame_error_t* error = nullptr);

    /**
     * @brief Set the timezone for time calculations.
     *
     * Configures the local timezone offset for proper time display.
     * Affects subsequent date/time operations and NTP synchronization.
     *
     * @param timezone Timezone string (e.g., "EST5EDT", "PST8PDT", "GMT0")
     * @note Timezone string format follows POSIX timezone specification
     */
    void setTimezone(const char* timezone);

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
    bool isConnected() const;

    /**
     * @brief Get the current IP address assigned by the network.
     *
     * @return String containing the IP address, or empty string if not connected
     * @note Only valid when connected to a WiFi network
     */
    String getIpAddress() const;

    /**
     * @brief Get the SSID of the currently connected network.
     *
     * @return String containing the network SSID, or empty string if not connected
     * @note Returns the SSID from stored credentials, not necessarily the connected network
     */
    String getSsid() const;

    /**
     * @brief Terminate WiFi operations and free resources.
     *
     * Performs complete shutdown of WiFi subsystem including disconnection
     * and resource cleanup. After calling this method, the WifiManager
     * instance should be reinitialized before use.
     */
    void end();

  private:
    String _ssid;  // Currently connected SSID (for reporting)
    String _password;  // Currently connected password
    struct wifi_network {
        String ssid;
        String password;
    };
    wifi_network _networks[WIFI_MAX_NETWORKS];
    uint8_t _network_count;
    bool _initialized;
    bool _connected;

    static const unsigned long CONNECTION_TIMEOUT_MS;
};

} // namespace photo_frame