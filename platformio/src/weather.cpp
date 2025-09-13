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

#include "config.h"

#ifdef USE_WEATHER
#include "weather.h"
#include <assets/icons/icons.h>

namespace photo_frame {
namespace weather {

// Static constants
const char* WeatherManager::OPENMETEO_API_BASE = "https://api.open-meteo.com/v1/forecast";

WeatherManager::WeatherManager() {
    // Initialize with default values
    current_weather      = WeatherData{};
    consecutive_failures = 0;
    last_attempt         = 0;
}

WeatherManager::~WeatherManager() {
    // Nothing to clean up for SD card implementation
}

bool WeatherManager::begin() {
    Serial.println("[WeatherManager] Initializing...");

    // Load configuration from SD card
    if (!load_config_from_sd()) {
        Serial.println("[WeatherManager] Failed to load configuration from SD card");
        return false;
    }

    // Validate configuration
    if (!config.is_valid()) {
        Serial.println("[WeatherManager] Invalid configuration loaded");
        return false;
    }

    // Try to load cached weather data
    load_weather_cache();

    Serial.printf("[WeatherManager] Initialized for location (%.3f, %.3f)\n",
                  config.latitude,
                  config.longitude);
    return true;
}

bool WeatherManager::needs_update(uint8_t battery_percent) const {
    if (!config.should_display_weather(battery_percent)) {
        return false;
    }

    uint32_t now      = time(NULL);
    uint32_t interval = get_adaptive_interval(battery_percent);

    // Check if enough time has passed since last attempt
    return (now - last_attempt) >= interval;
}

uint32_t WeatherManager::get_adaptive_interval(uint8_t battery_percent) const {
    uint32_t base_interval = config.update_interval_minutes * 60; // Convert to seconds

    // Increase interval based on consecutive failures
    if (consecutive_failures > 3) {
        base_interval *= 4; // 4x interval after many failures
    } else if (consecutive_failures > 1) {
        base_interval *= 2; // 2x interval after some failures
    }

    // Smart battery-aware interval scaling
    if (battery_percent < 10) {
        // Emergency power saving: 8x interval (8 hours if base is 1 hour)
        base_interval *= 8;
    } else if (battery_percent < 20) {
        // Critical battery: 4x interval (4 hours if base is 1 hour)
        base_interval *= 4;
    } else if (battery_percent < 30) {
        // Low battery: 2x interval (2 hours if base is 1 hour)
        base_interval *= 2;
    } else if (battery_percent < 50) {
        // Medium battery: 1.5x interval (1.5 hours if base is 1 hour)
        base_interval = (base_interval * 3) / 2;
    }
    // High battery (>50%): use base interval

    // Dynamic min/max based on battery level
    uint32_t min_interval =
        (battery_percent < 20) ? 3600 : 900; // 1 hour min when critical, 15 min otherwise
    uint32_t max_interval =
        (battery_percent < 30) ? 28800 : 14400; // 8 hours max when low, 4 hours otherwise

    uint32_t final_interval = constrain(base_interval, min_interval, max_interval);

    // Debug logging for power management decisions
    Serial.printf("[WeatherManager] Adaptive interval - Battery: %d%%, Failures: %d, Base: %ds, "
                  "Final: %us\n",
                  battery_percent,
                  consecutive_failures,
                  config.update_interval_minutes * 60,
                  final_interval);

    return final_interval;
}

String WeatherManager::build_api_url() const {
    String url = OPENMETEO_API_BASE;
    url += "?latitude=" + String(config.latitude, 4);
    url += "&longitude=" + String(config.longitude, 4);
    url += "&daily=sunrise,sunset,apparent_temperature_max,apparent_temperature_min";
    url += "&current=is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_"
           "speed_10m,apparent_temperature";
    url += "&timezone=" + config.timezone;
    url += "&forecast_days=1";
    url += "&temperature_unit=" + config.temperature_unit;
    url += "&wind_speed_unit=" + config.wind_speed_unit;
    url += "&precipitation_unit=" + config.precipitation_unit;

    return url;
}

bool WeatherManager::fetch_weather() {
    if (!config.is_valid()) {
        Serial.println("[WeatherManager] Invalid configuration, cannot fetch weather");
        return false;
    }

    last_attempt         = time(NULL);
    consecutive_failures = 0;
    Serial.println("[WeatherManager] Fetching weather data...");

    HTTPClient http;
    http.setTimeout(10000); // 10 second timeout

    String url = build_api_url();
    Serial.printf("[WeatherManager] API URL: %s\n", url.c_str());

    if (!http.begin(url)) {
        Serial.println("[WeatherManager] Failed to initialize HTTP client");
        consecutive_failures++;
        return false;
    }

    int http_code = http.GET();

    if (http_code != HTTP_CODE_OK) {
        Serial.printf("[WeatherManager] HTTP request failed with code: %d\n", http_code);
        consecutive_failures++;
        http.end();
        return false;
    }

    String response = http.getString();
    http.end();

    Serial.printf("[WeatherManager] Received response (%d bytes)\n", response.length());

    WeatherData new_weather;
    if (parse_weather_response(response, new_weather)) {
        current_weather             = new_weather;
        current_weather.last_update = last_attempt;
        current_weather.valid       = true;
        consecutive_failures        = 0;

        // Cache the weather data
        save_weather_cache();

        Serial.printf("[WeatherManager] Weather updated - %.1f°C, %s\n",
                      current_weather.temperature,
                      current_weather.description.c_str());
        return true;
    } else {
        Serial.println("[WeatherManager] Failed to parse weather response");
        consecutive_failures++;
        return false;
    }
}

bool WeatherManager::parse_weather_response(const String& json_response,
                                            WeatherData& weather_data) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, json_response);

    if (error) {
        Serial.printf("[WeatherManager] JSON parsing failed: %s\n", error.c_str());
        return false;
    }

    // Check if response contains current weather data
    if (!doc.containsKey("current")) {
        Serial.println("[WeatherManager] No current weather data in response");
        return false;
    }

    JsonObject current = doc["current"];

    // Extract apparent temperature
    if (current.containsKey("apparent_temperature")) {
        weather_data.temperature = current["apparent_temperature"].as<float>();
    } else {
        Serial.println("[WeatherManager] No apparent temperature data in response");
        return false;
    }

    // Extract is_day status
    if (current.containsKey("is_day")) {
        weather_data.is_day = current["is_day"].as<uint8_t>() == 1;
    } else {
        // Default to day if not available
        weather_data.is_day = true;
    }

    // Extract wind speed
    if (current.containsKey("wind_speed_10m")) {
        weather_data.wind_speed = current["wind_speed_10m"].as<float>();
    } else {
        weather_data.wind_speed = 0.0f;
    }

    // Extract weather code
    if (current.containsKey("weather_code")) {
        uint8_t wmo_code          = current["weather_code"].as<uint8_t>();
        weather_data.weather_code = wmo_code;
        weather_data.icon =
            wmo_code_to_icon(wmo_code, weather_data.is_day, weather_data.wind_speed);
        weather_data.description = wmo_code_to_description(wmo_code);
    } else {
        Serial.println("[WeatherManager] No weather code in response");
        return false;
    }

    // Extract temperature unit from current_units if available
    if (doc.containsKey("current_units")) {
        JsonObject current_units = doc["current_units"];
        if (current_units.containsKey("apparent_temperature")) {
            weather_data.temperature_unit = current_units["apparent_temperature"].as<String>();
        } else {
            weather_data.temperature_unit = "°C"; // Default fallback
        }
    } else {
        weather_data.temperature_unit = "°C"; // Default fallback
    }

    // Extract daily forecast data if available
    if (doc.containsKey("daily")) {
        JsonObject daily = doc["daily"];

        // Extract min/max apparent temperatures (arrays with first element for today)
        if (daily.containsKey("apparent_temperature_min") &&
            daily.containsKey("apparent_temperature_max")) {
            JsonArray temp_min_array = daily["apparent_temperature_min"];
            JsonArray temp_max_array = daily["apparent_temperature_max"];
            if (temp_min_array.size() > 0 && temp_max_array.size() > 0) {
                weather_data.temp_min = temp_min_array[0].as<float>();
                weather_data.temp_max = temp_max_array[0].as<float>();
            }
        }

        // Extract sunrise/sunset times (ISO 8601 format, need to convert to Unix timestamp)
        if (daily.containsKey("sunrise") && daily.containsKey("sunset")) {
            JsonArray sunrise_array = daily["sunrise"];
            JsonArray sunset_array  = daily["sunset"];
            if (sunrise_array.size() > 0 && sunset_array.size() > 0) {
                String sunrise_str = sunrise_array[0].as<String>();
                String sunset_str  = sunset_array[0].as<String>();

                // Convert ISO 8601 datetime to Unix timestamp
                // Format expected: "2024-12-20T06:54"
                // We want to save the sunrise and sunset times just as
                struct tm tm_sunrise = {};
                struct tm tm_sunset  = {};

                if (strptime(sunrise_str.c_str(), "%Y-%m-%dT%H:%M", &tm_sunrise)) {
                    weather_data.sunrise_time = mktime(&tm_sunrise);
                }
                if (strptime(sunset_str.c_str(), "%Y-%m-%dT%H:%M", &tm_sunset)) {
                    weather_data.sunset_time = mktime(&tm_sunset);
                }
            }
        }

        weather_data.has_daily_data = true;
        Serial.printf("[WeatherManager] Daily data - Min: %.1f°C, Max: %.1f°C\n",
                      weather_data.temp_min,
                      weather_data.temp_max);
    } else {
        weather_data.has_daily_data = false;
        Serial.println("[WeatherManager] No daily forecast data in response");
    }

    return true;
}

weather_icon_t
WeatherManager::wmo_code_to_icon(uint8_t wmo_code, bool is_day, float wind_speed) const {
    // Consider wind speed threshold for selecting wind variants (e.g., >25 km/h is windy)
    bool is_windy = wind_speed > 25.0f;

    switch (wmo_code) {
    case 0: // Clear sky
        return is_day ? weather_icon_t::clear_sky_day : weather_icon_t::clear_sky_night;

    case 1: // Mainly clear
    case 2: // Partly cloudy
    case 3: // Overcast
        // For partly cloudy conditions, check if there's precipitation
        return is_day ? weather_icon_t::cloudy_day : weather_icon_t::cloudy_night;

    case 45: // Fog
    case 48: // Depositing rime fog
        if (is_windy) {
            return weather_icon_t::overcast_wind_rain; // Windy fog conditions
        }
        return weather_icon_t::fog;

    case 51: // Light drizzle
    case 53: // Moderate drizzle
    case 56: // Light freezing drizzle
    case 57: // Dense freezing drizzle
    case 61: // Slight rain
        if (is_windy) {
            return weather_icon_t::wind_rain;
        }
        // Light rain with day/night variants for partly cloudy conditions
        return is_day ? weather_icon_t::cloudy_day_rain : weather_icon_t::cloudy_night_rain;

    case 55: // Dense drizzle
    case 63: // Moderate rain
    case 65: // Heavy rain
    case 66: // Light freezing rain
    case 67: // Heavy freezing rain
    case 80: // Slight rain showers
    case 81: // Moderate rain showers
    case 82: // Violent rain showers
        if (is_windy) {
            return weather_icon_t::wind_rain;
        }
        return weather_icon_t::rain;

    case 68:                              // Slight rain and snow mixed
    case 69:                              // Heavy rain and snow mixed
    case 83:                              // Slight snow showers
    case 84:                              // Heavy snow showers
    case 85:                              // Slight snow showers
    case 86:                              // Heavy snow showers
        return weather_icon_t::snow_rain; // Mixed precipitation

    case 71: // Slight snow fall
    case 73: // Moderate snow fall
    case 75: // Heavy snow fall
    case 77: // Snow grains
        return weather_icon_t::snow;

    case 95: // Thunderstorm: Slight or moderate
    case 96: // Thunderstorm with slight hail
    case 99: // Thunderstorm with heavy hail
        return weather_icon_t::thunderstorm;

    default:
        // Handle unknown codes by range
        if (wmo_code >= 1 && wmo_code <= 3) {
            return is_day ? weather_icon_t::cloudy_day : weather_icon_t::cloudy_night;
        } else if (wmo_code >= 45 && wmo_code <= 48) {
            return is_windy ? weather_icon_t::overcast_wind_rain : weather_icon_t::overcast;
        } else if (wmo_code >= 51 && wmo_code <= 67) {
            if (is_windy)
                return weather_icon_t::wind_rain;
            return is_day ? weather_icon_t::cloudy_day_rain : weather_icon_t::cloudy_night_rain;
        } else if (wmo_code >= 68 && wmo_code <= 86) {
            return weather_icon_t::snow_rain;
        } else if (wmo_code >= 95) {
            return weather_icon_t::thunderstorm;
        }

        // If nothing matches and it's windy, show wind icon
        if (is_windy) {
            return weather_icon_t::wind;
        }

        return weather_icon_t::unknown;
    }
}

String WeatherManager::wmo_code_to_description(uint8_t wmo_code) const {
    switch (wmo_code) {
    case 0:  return "Clear";
    case 1:  return "Mostly Clear";
    case 2:  return "Partly Cloudy";
    case 3:  return "Overcast";
    case 45: return "Fog";
    case 48: return "Rime Fog";
    case 51: return "Light Drizzle";
    case 53: return "Drizzle";
    case 55: return "Heavy Drizzle";
    case 56: return "Freezing Drizzle";
    case 57: return "Heavy Freezing Drizzle";
    case 61: return "Light Rain";
    case 63: return "Rain";
    case 65: return "Heavy Rain";
    case 66: return "Freezing Rain";
    case 67: return "Heavy Freezing Rain";
    case 71: return "Light Snow";
    case 73: return "Snow";
    case 75: return "Heavy Snow";
    case 77: return "Snow Grains";
    case 80: return "Light Showers";
    case 81: return "Showers";
    case 82: return "Heavy Showers";
    case 85: return "Light Snow Showers";
    case 86: return "Snow Showers";
    case 95: return "Thunderstorm";
    case 96: return "Thunderstorm with Hail";
    case 99: return "Heavy Thunderstorm";
    default: return "Unknown";
    }
}

void WeatherManager::save_weather_cache() {
    if (!current_weather.valid)
        return;

    DynamicJsonDocument doc(512);
    doc["temperature"]      = current_weather.temperature;
    doc["weather_code"]     = current_weather.weather_code;
    doc["humidity"]         = current_weather.humidity;
    doc["is_day"]           = current_weather.is_day;
    doc["wind_speed"]       = current_weather.wind_speed;
    doc["last_update"]      = current_weather.last_update;
    doc["description"]      = current_weather.description;
    doc["valid"]            = true;
    doc["temperature_unit"] = current_weather.temperature_unit;

    // Save daily forecast data
    doc["temp_min"]       = current_weather.temp_min;
    doc["temp_max"]       = current_weather.temp_max;
    doc["sunrise_time"]   = current_weather.sunrise_time;
    doc["sunset_time"]    = current_weather.sunset_time;
    doc["has_daily_data"] = current_weather.has_daily_data;

    File cache_file       = SD.open(WEATHER_CACHE_FILE, FILE_WRITE);
    if (!cache_file) {
        Serial.println("[WeatherManager] Failed to open cache file for writing");
        return;
    }

    serializeJson(doc, cache_file);
    cache_file.close();

    Serial.println("[WeatherManager] Weather data cached to SD card");
}

bool WeatherManager::load_weather_cache() {
    if (!SD.exists(WEATHER_CACHE_FILE)) {
        Serial.println("[WeatherManager] No cached weather data found");
        return false;
    }

    File cache_file = SD.open(WEATHER_CACHE_FILE, FILE_READ);
    if (!cache_file) {
        Serial.println("[WeatherManager] Failed to open cache file for reading");
        return false;
    }

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, cache_file);
    cache_file.close();

    if (error) {
        Serial.printf("[WeatherManager] Failed to parse cache file: %s\n", error.c_str());
        return false;
    }

    if (!doc["valid"].as<bool>()) {
        Serial.println("[WeatherManager] Cached weather data marked as invalid");
        return false;
    }

    current_weather.temperature  = doc["temperature"].as<float>();
    current_weather.weather_code = doc["weather_code"].as<uint8_t>();
    current_weather.humidity     = doc["humidity"].as<uint8_t>();
    current_weather.is_day       = doc["is_day"].as<bool>();
    current_weather.wind_speed   = doc["wind_speed"].as<float>();
    current_weather.last_update  = doc["last_update"].as<uint32_t>();
    current_weather.description  = doc["description"].as<String>();
    current_weather.valid        = true;
    current_weather.icon         = wmo_code_to_icon(
        current_weather.weather_code, current_weather.is_day, current_weather.wind_speed);
    current_weather.temperature_unit =
        doc["temperature_unit"] | "°C"; // Default to Celsius for backwards compatibility

    // Load daily forecast data (with defaults for backwards compatibility)
    current_weather.temp_min       = doc["temp_min"] | 0.0f;
    current_weather.temp_max       = doc["temp_max"] | 0.0f;
    current_weather.sunrise_time   = doc["sunrise_time"] | 0;
    current_weather.sunset_time    = doc["sunset_time"] | 0;
    current_weather.has_daily_data = doc["has_daily_data"] | false;

    Serial.printf("[WeatherManager] Loaded cached weather - %.1f°C, %s\n",
                  current_weather.temperature,
                  current_weather.description.c_str());
    return true;
}

bool WeatherManager::load_config_from_sd() {
    if (!SD.exists(WEATHER_CONFIG_FILE)) {
        Serial.println("[WeatherManager] Weather config file not found, creating example config");
        return create_example_config();
    }

    File config_file = SD.open(WEATHER_CONFIG_FILE, FILE_READ);
    if (!config_file) {
        Serial.println("[WeatherManager] Failed to open weather config file");
        return false;
    }

    String json_content = config_file.readString();
    config_file.close();

    WeatherConfig new_config;
    if (parse_config_json(json_content, new_config)) {
        config = new_config;
        Serial.println("[WeatherManager] Configuration loaded successfully");
        return true;
    } else {
        Serial.println("[WeatherManager] Failed to parse weather configuration");
        return false;
    }
}

bool WeatherManager::reload_config() { return load_config_from_sd(); }

bool WeatherManager::parse_config_json(const String& json_content, WeatherConfig& weather_config) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json_content);

    if (error) {
        Serial.printf("[WeatherManager] JSON parsing error: %s\n", error.c_str());
        return false;
    }

    // Parse configuration fields with defaults
    weather_config.enabled   = doc["enabled"].as<bool>();
    weather_config.latitude  = doc["latitude"].as<float>();
    weather_config.longitude = doc["longitude"].as<float>();
    // Parse and clamp values to valid ranges
    uint16_t update_interval = doc["update_interval_minutes"] | 60;
    weather_config.update_interval_minutes =
        max(update_interval, (uint16_t)(WEATHER_UPDATE_INTERVAL_MIN_VALUE_SECONDS / 60));

    weather_config.celsius           = doc["celsius"] | true;

    uint8_t battery_threshold        = doc["battery_threshold"] | 15;
    weather_config.battery_threshold = constrain(battery_threshold,
                                                 WEATHER_BATTERY_THRESHOLD_MIN_VALUE,
                                                 WEATHER_BATTERY_THRESHOLD_MAX_VALUE);

    uint32_t max_age                 = doc["max_age_hours"] | 3;
    weather_config.max_age_hours =
        constrain(max_age, WEATHER_MAX_AGE_HOURS_MIN_VALUE, WEATHER_MAX_AGE_HOURS_MAX_VALUE);

    weather_config.timezone           = doc["timezone"] | "auto";
    weather_config.temperature_unit   = doc["temperature_unit"] | "celsius";
    weather_config.wind_speed_unit    = doc["wind_speed_unit"] | "kmh";
    weather_config.precipitation_unit = doc["precipitation_unit"] | "mm";

    return weather_config.is_valid();
}

bool WeatherManager::create_example_config() {
    DynamicJsonDocument doc(1024);

    // Create example configuration for New York City
    doc["enabled"]                 = false; // Disabled by default
    doc["latitude"]                = 40.7128;
    doc["longitude"]               = -74.0060;
    doc["update_interval_minutes"] = 120;
    doc["celsius"]                 = true;
    doc["battery_threshold"]       = 15;
    doc["max_age_hours"]           = 3;
    doc["timezone"]                = "auto";
    doc["temperature_unit"]        = "celsius";
    doc["wind_speed_unit"]         = "kmh";
    doc["precipitation_unit"]      = "mm";

    // Add documentation comments
    doc["_comment"]      = "Weather configuration for ESP32 Photo Frame";
    doc["_instructions"] = "Set enabled=true and update latitude/longitude for your location";

    File config_file     = SD.open(WEATHER_CONFIG_FILE, FILE_WRITE);
    if (!config_file) {
        Serial.println("[WeatherManager] Failed to create example config file");
        return false;
    }

    serializeJsonPretty(doc, config_file);
    config_file.close();

    Serial.printf("[WeatherManager] Created example config at %s\n", WEATHER_CONFIG_FILE);
    Serial.println("[WeatherManager] Please edit the config file and set enabled=true");
    return false; // Return false since weather is disabled by default
}

/// Map weather icon type to dedicated weather icon names
icon_name_t weather_icon_to_system_icon(weather_icon_t weather_icon) {
    switch (weather_icon) {
    case weather_icon_t::clear_sky_day:      return icon_name_t::clear_sky_day_0deg;

    case weather_icon_t::clear_sky_night:    return icon_name_t::clear_sky_night_0deg;

    case weather_icon_t::cloudy_day:         return icon_name_t::cloudy_day_0deg;

    case weather_icon_t::cloudy_night:       return icon_name_t::cloudy_night_0deg;

    case weather_icon_t::cloudy_day_rain:    return icon_name_t::cloudy_day_rain_0deg;

    case weather_icon_t::cloudy_night_rain:  return icon_name_t::cloudy_night_rain_0deg;

    case weather_icon_t::overcast:           return icon_name_t::overcast_0deg;

    case weather_icon_t::overcast_wind_rain: return icon_name_t::overcast_wind_rain_0deg;

    case weather_icon_t::rain:               return icon_name_t::rain_0deg;

    case weather_icon_t::wind_rain:          return icon_name_t::wind_rain_0deg;

    case weather_icon_t::snow:               return icon_name_t::snow_0deg;

    case weather_icon_t::snow_rain:          return icon_name_t::snow_rain_0deg;

    case weather_icon_t::thunderstorm:       return icon_name_t::thunderstorm_0deg;

    case weather_icon_t::wind:               return icon_name_t::wind_0deg;

    case weather_icon_t::fog:                return icon_name_t::fog_0deg;

    case weather_icon_t::unknown:
    default:                                 return icon_name_t::error_icon; // Question mark for unknown weather
    }
}

/// Get weather icon bitmap data for rendering
const unsigned char* get_weather_icon_bitmap(weather_icon_t weather_icon, uint16_t size) {
    icon_name_t system_icon = weather_icon_to_system_icon(weather_icon);
    return getBitmap(system_icon, size);
}

} // namespace weather
} // namespace photo_frame

#endif // USE_WEATHER