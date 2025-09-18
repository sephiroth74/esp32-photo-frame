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

#ifdef USE_WEATHER

#include "datetime_utils.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SD.h>
#include <WiFi.h>
#include <assets/icons/icons.h>

// Weather configuration constants
#define WEATHER_UPDATE_INTERVAL_MIN_VALUE_SECONDS 3600 // 1 hour minimum update interval
#define WEATHER_BATTERY_THRESHOLD_MIN_VALUE       5    // 5% minimum battery threshold
#define WEATHER_BATTERY_THRESHOLD_MAX_VALUE       50   // 50% maximum battery threshold
#define WEATHER_MAX_AGE_HOURS_MIN_VALUE           1    // 1 hour minimum max age
#define WEATHER_MAX_AGE_HOURS_MAX_VALUE           24   // 24 hours maximum max age

namespace photo_frame {
namespace weather {

/// Weather icon types that correspond to available weather icons
/// These map to specific SVG icons in the weather_icons folder
enum class weather_icon_t : uint8_t {
    unknown = 0,
    clear_sky_day,      // WMO 0 (day)
    clear_sky_night,    // WMO 0 (night)
    cloudy_day,         // WMO 1-3 (day, no rain)
    cloudy_night,       // WMO 1-3 (night, no rain)
    cloudy_day_rain,    // WMO 1-3 (day, with rain)
    cloudy_night_rain,  // WMO 1-3 (night, with rain)
    overcast,           // WMO 45, 48 (dense fog/overcast)
    overcast_wind_rain, // WMO 45, 48 (with wind and rain)
    rain,               // WMO 51-65 (rain/drizzle)
    wind_rain,          // WMO 51-65 (rain with wind)
    snow,               // WMO 71-77 (snow)
    snow_rain,          // WMO 68-69, 83-86 (mixed precipitation)
    thunderstorm,       // WMO 95-99 (thunderstorm)
    wind,               // Any condition with high wind speed
    fog                 // WMO 45, 48 (fog/mist)
};

/// Weather data structure containing current conditions
struct WeatherData {
    float temperature       = 0.0f; // Temperature in Celsius
    uint8_t weather_code    = 255;  // WMO weather code (0-99, 255=invalid)
    weather_icon_t icon     = weather_icon_t::unknown;
    String description      = "";    // Human readable description
    uint32_t last_update    = 0;     // Unix timestamp of last successful update
    uint8_t humidity        = 0;     // Relative humidity percentage (0-100)
    bool is_day             = true;  // True if daytime, false if nighttime
    float wind_speed        = 0.0f;  // Wind speed in km/h
    bool valid              = false; // True if data is valid
    String temperature_unit = "°C";  // Temperature unit from API (e.g., "°C", "°F")

    // Daily forecast data
    float temp_min        = 0.0f;  // Daily minimum temperature in Celsius
    float temp_max        = 0.0f;  // Daily maximum temperature in Celsius
    uint32_t sunrise_time = 0;     // Sunrise time as Unix timestamp
    uint32_t sunset_time  = 0;     // Sunset time as Unix timestamp
    bool has_daily_data   = false; // True if daily forecast data is available

    /// Check if weather data is too old to display
    bool is_stale(uint32_t max_age_seconds = WEATHER_MAX_AGE_SECONDS) const {
        uint32_t now = time(NULL);
        return valid && (now - last_update > max_age_seconds);
    }

    /// Check if weather data is displayable (valid and not too stale)
    bool is_displayable(uint32_t max_age_seconds = WEATHER_MAX_AGE_SECONDS) const {
        return valid && !is_stale(max_age_seconds);
    }

    /// Get formatted temperature string
    String get_temperature_string(bool celsius = true) const {
        if (!valid)
            return "";

        float temp = celsius ? temperature : (temperature * 9.0f / 5.0f + 32.0f);
        return String(temp, 0) + (celsius ? "°C" : "°F");
    }
};

/// Weather configuration structure
struct WeatherConfig {
    bool enabled                     = false;     // Enable/disable weather feature
    float latitude                   = 0.0f;      // Location latitude (-90 to 90)
    float longitude                  = 0.0f;      // Location longitude (-180 to 180)
    uint16_t update_interval_minutes = 60;        // Update interval in minutes
    bool celsius                     = true;      // Temperature unit preference
    uint8_t battery_threshold        = 15;        // Disable weather below this battery %
    uint32_t max_age_hours           = 3;         // Max age before weather is considered stale
    String timezone                  = "auto";    // Timezone (e.g., "Europe/Berlin", "auto")
    String temperature_unit          = "celsius"; // Temperature unit ("celsius", "fahrenheit")
    String wind_speed_unit           = "kmh";     // Wind speed unit ("kmh", "mph", "ms", "kn")
    String precipitation_unit        = "mm";      // Precipitation unit ("mm", "inch")

    /// Validate configuration values (only essential checks, other values are clamped)
    bool is_valid() const {
        return enabled && (latitude >= -90.0f && latitude <= 90.0f) &&
               (longitude >= -180.0f && longitude <= 180.0f);
    }

    /// Check if weather should be displayed based on battery level
    bool should_display_weather(uint8_t battery_percent) const {
        return enabled && is_valid() && (battery_percent > battery_threshold);
    }
};

/// Weather manager class handling API communication and data management
class WeatherManager {
  private:
    WeatherConfig config;
    WeatherData current_weather;
    uint8_t consecutive_failures      = 0;
    uint32_t last_attempt             = 0;
    static const uint8_t MAX_FAILURES = 5;
    static const char* OPENMETEO_API_BASE;

    /// Map WMO weather code to weather icon considering day/night and wind
    weather_icon_t wmo_code_to_icon(uint8_t wmo_code, bool is_day, float wind_speed) const;

    /// Map WMO weather code to description string
    String wmo_code_to_description(uint8_t wmo_code) const;

    /// Build Open-Meteo API URL
    String build_api_url() const;

    /// Parse JSON response from Open-Meteo API
    bool parse_weather_response(const String& json_response, WeatherData& weather_data);

    /// Get adaptive update interval based on failures and battery
    uint32_t get_adaptive_interval(uint8_t battery_percent) const;

    /// Save weather data to SD card for offline fallback
    void save_weather_cache();

    /// Load cached weather data from SD card
    bool load_weather_cache();

    /// Parse weather configuration from JSON
    bool parse_config_json(const String& json_content, WeatherConfig& weather_config);

  public:
    /**
     * @brief Constructor for WeatherManager.
     *
     * Initializes the weather manager with default settings. Configuration
     * must be loaded separately using begin() or load_config_from_sd().
     */
    WeatherManager();

    /**
     * @brief Destructor for WeatherManager.
     *
     * Ensures proper cleanup of resources and saves current weather data
     * to cache if available.
     */
    ~WeatherManager();

    /**
     * @brief Initialize weather manager by loading configuration from SD card.
     *
     * Loads weather configuration from the default configuration file on SD card
     * and initializes the weather system. Also attempts to load cached weather
     * data if available.
     *
     * @return true if initialization successful, false on error
     * @note Configuration file must exist on SD card for successful initialization
     */
    bool begin();

    /**
     * @brief Get current weather configuration.
     *
     * @return WeatherConfig structure containing current configuration settings
     */
    WeatherConfig get_config() const { return config; }

    /**
     * @brief Check if weather data needs updating based on interval and battery level.
     *
     * Determines if weather data should be fetched based on update interval,
     * battery level thresholds, and consecutive failure count.
     *
     * @param battery_percent Current battery percentage (0-100)
     * @return true if weather update is needed and advisable, false otherwise
     */
    bool needs_update(uint8_t battery_percent) const;

    /**
     * @brief Fetch current weather data from Open-Meteo API.
     *
     * Makes HTTP request to Open-Meteo weather API, parses the response,
     * and updates internal weather data. Handles network errors and
     * maintains failure count for adaptive behavior.
     *
     * @return true if weather data was successfully fetched, false on error
     * @note Requires active WiFi connection to function
     */
    bool fetch_weather();

    /**
     * @brief Get current cached weather data.
     *
     * @return WeatherData structure containing current weather information
     * @note Data may be stale; check WeatherData.is_displayable() before use
     */
    WeatherData get_current_weather() const { return current_weather; }

    /**
     * @brief Check if weather manager has valid configuration.
     *
     * @return true if configuration is valid and weather system is ready for use
     */
    bool is_configured() const { return config.is_valid(); }

    /**
     * @brief Reset consecutive failures counter.
     *
     * Clears the failure count, useful after successful network recovery
     * or manual intervention to resume normal update intervals.
     */
    void reset_failures() { consecutive_failures = 0; }

    /**
     * @brief Get current failure count for diagnostics.
     *
     * @return Number of consecutive API fetch failures
     * @note Used for adaptive update intervals and error reporting
     */
    uint8_t get_failure_count() const { return consecutive_failures; }

    /**
     * @brief Load weather configuration from SD card JSON file.
     *
     * Reads and parses the weather configuration file from SD card,
     * validating all settings and applying defaults where necessary.
     *
     * @return true if configuration loaded successfully, false on error
     * @note Configuration file path is predefined in the implementation
     */
    bool load_config_from_sd();

    /**
     * @brief Reload configuration from SD card.
     *
     * Reloads weather configuration from SD card, useful for applying
     * configuration changes without restarting the system.
     *
     * @return true if configuration reloaded successfully, false on error
     */
    bool reload_config();

    /**
     * @brief Create example weather configuration file on SD card.
     *
     * Generates a template weather configuration file with default values
     * and comments to help users configure the weather system.
     *
     * @return true if example file created successfully, false on error
     * @note Will not overwrite existing configuration files
     */
    bool create_example_config();
};

/**
 * @brief Map weather icon type to system icon name for compatibility.
 *
 * Converts weather-specific icon types to the general system icon enumeration
 * used by the renderer. This provides compatibility with the existing icon
 * rendering system.
 *
 * @param weather_icon Weather icon type to convert
 * @return Corresponding system icon name, or icon_unknown if no mapping exists
 */
icon_name_t weather_icon_to_system_icon(weather_icon_t weather_icon);

/**
 * @brief Get weather icon bitmap data for direct rendering.
 *
 * Retrieves the bitmap data for a specific weather icon and size.
 * Used for rendering weather icons directly without going through
 * the system icon infrastructure.
 *
 * @param weather_icon Weather icon type to get bitmap for
 * @param size Requested icon size in pixels (e.g., 16, 32, 64)
 * @return Pointer to bitmap data, or nullptr if icon/size not available
 * @note Bitmap format depends on the display type and color depth
 */
const unsigned char* get_weather_icon_bitmap(weather_icon_t weather_icon, uint16_t size);

} // namespace weather
} // namespace photo_frame

#endif // USE_WEATHER