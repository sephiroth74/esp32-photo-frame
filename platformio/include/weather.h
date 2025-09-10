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

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <FS.h>
#include "datetime_utils.h"
#include <assets/icons/icons.h>

// Weather configuration constants
#define WEATHER_UPDATE_INTERVAL_MIN_VALUE_SECONDS 3600 // 1 hour minimum update interval
#define WEATHER_BATTERY_THRESHOLD_MIN_VALUE 5           // 5% minimum battery threshold
#define WEATHER_BATTERY_THRESHOLD_MAX_VALUE 50          // 50% maximum battery threshold  
#define WEATHER_MAX_AGE_HOURS_MIN_VALUE 1               // 1 hour minimum max age 
#define WEATHER_MAX_AGE_HOURS_MAX_VALUE 24              // 24 hours maximum max age

namespace photo_frame {
namespace weather {

/// Weather icon types that correspond to available weather icons
/// These map to specific SVG icons in the weather_icons folder
enum class weather_icon_t : uint8_t {
    unknown = 0,
    clear_sky_day,       // WMO 0 (day)
    clear_sky_night,     // WMO 0 (night)
    cloudy_day,          // WMO 1-3 (day, no rain)
    cloudy_night,        // WMO 1-3 (night, no rain)
    cloudy_day_rain,     // WMO 1-3 (day, with rain)
    cloudy_night_rain,   // WMO 1-3 (night, with rain)
    overcast,            // WMO 45, 48 (dense fog/overcast)
    overcast_wind_rain,  // WMO 45, 48 (with wind and rain)
    rain,                // WMO 51-65 (rain/drizzle)
    wind_rain,           // WMO 51-65 (rain with wind)
    snow,                // WMO 71-77 (snow)
    snow_rain,           // WMO 68-69, 83-86 (mixed precipitation)
    thunderstorm,        // WMO 95-99 (thunderstorm)
    wind,                // Any condition with high wind speed
    fog                  // WMO 45, 48 (fog/mist)
};

/// Weather data structure containing current conditions
struct WeatherData {
    float temperature = 0.0f;          // Temperature in Celsius
    uint8_t weather_code = 255;        // WMO weather code (0-99, 255=invalid)
    weather_icon_t icon = weather_icon_t::unknown;
    String description = "";           // Human readable description
    uint32_t last_update = 0;          // Unix timestamp of last successful update
    uint8_t humidity = 0;              // Relative humidity percentage (0-100)
    bool is_day = true;                // True if daytime, false if nighttime
    float wind_speed = 0.0f;           // Wind speed in km/h
    bool valid = false;                // True if data is valid
    
    // Daily forecast data
    float temp_min = 0.0f;             // Daily minimum temperature in Celsius
    float temp_max = 0.0f;             // Daily maximum temperature in Celsius
    uint32_t sunrise_time = 0;         // Sunrise time as Unix timestamp
    uint32_t sunset_time = 0;          // Sunset time as Unix timestamp
    bool has_daily_data = false;       // True if daily forecast data is available

    /// Check if weather data is too old to display
    bool is_stale(uint32_t max_age_seconds = 10800) const { // 3 hours default
        uint32_t now = time(NULL);
        return valid && (now - last_update > max_age_seconds);
    }

    /// Check if weather data is displayable (valid and not too stale)
    bool is_displayable(uint32_t max_age_seconds = 10800) const {
        return valid && !is_stale(max_age_seconds);
    }

    /// Get formatted temperature string
    String get_temperature_string(bool celsius = true) const {
        if (!valid) return "";
        
        float temp = celsius ? temperature : (temperature * 9.0f / 5.0f + 32.0f);
        return String(temp, 0) + (celsius ? "°C" : "°F");
    }
};

/// Weather configuration structure
struct WeatherConfig {
    bool enabled = false;                   // Enable/disable weather feature
    float latitude = 0.0f;                  // Location latitude (-90 to 90)
    float longitude = 0.0f;                 // Location longitude (-180 to 180)
    uint16_t update_interval_minutes = 60;  // Update interval in minutes
    bool celsius = true;                    // Temperature unit preference
    uint8_t battery_threshold = 15;         // Disable weather below this battery %
    uint32_t max_age_hours = 3;            // Max age before weather is considered stale
    String timezone = "auto";               // Timezone (e.g., "Europe/Berlin", "auto")

    /// Validate configuration values (only essential checks, other values are clamped)
    bool is_valid() const {
        return enabled && 
               (latitude >= -90.0f && latitude <= 90.0f) &&
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
    uint8_t consecutive_failures = 0;
    uint32_t last_attempt = 0;
    static const uint8_t MAX_FAILURES = 5;
    static const char* OPENMETEO_API_BASE;
    static const char* WEATHER_CONFIG_FILE;
    static const char* WEATHER_CACHE_FILE;

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
    WeatherManager();
    ~WeatherManager();

    /// Initialize weather manager (loads config from SD card)
    bool begin();
    
    /// Get current configuration
    WeatherConfig get_config() const { return config; }
    
    /// Check if weather data needs updating
    bool needs_update(uint8_t battery_percent) const;
    
    /// Fetch weather data from Open-Meteo API
    bool fetch_weather();
    
    /// Get current weather data
    WeatherData get_current_weather() const { return current_weather; }
    
    /// Check if weather manager is properly configured
    bool is_configured() const { return config.is_valid(); }
    
    /// Reset consecutive failures counter
    void reset_failures() { consecutive_failures = 0; }
    
    /// Get failure count for diagnostics
    uint8_t get_failure_count() const { return consecutive_failures; }
    
    /// Load configuration from SD card JSON file
    bool load_config_from_sd();
    
    /// Reload configuration (useful for runtime config changes)
    bool reload_config();
    
    /// Create example weather configuration file on SD card
    bool create_example_config();
};

/// Map weather icon type to system icon name (for existing icon compatibility)
icon_name_t weather_icon_to_system_icon(weather_icon_t weather_icon);

/// Get weather icon bitmap data for rendering
const unsigned char* get_weather_icon_bitmap(weather_icon_t weather_icon, uint16_t size);

} // namespace weather
} // namespace photo_frame