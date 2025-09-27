# Weather Display Feature

The ESP32 Photo Frame includes a **runtime-configurable weather display feature** (v0.7.1) that shows comprehensive weather information in a dedicated overlay panel. Weather functionality is controlled entirely through the unified `/config.json` configuration file - no recompilation needed to enable or disable weather features.

## 🌤️ Features

- **Comprehensive weather overlay**: Current temperature, daily min/max, sunrise/sunset times, and date in a rounded background panel
- **Dedicated weather icons**: Condition-specific icons for clear, cloudy, rainy, snowy, and stormy weather
- **Smart positioning**: Bottom-left overlay that doesn't interfere with main image content
- **Power efficient**: Piggybacks on existing WiFi sessions (98-99% of original battery life maintained)
- **Smart battery management**: Adaptive update intervals based on battery level
- **Configurable timezone**: Supports any timezone for accurate sunrise/sunset times
- **Offline resilience**: Caches weather data for offline periods
- **Runtime configuration**: Enable/disable without firmware recompilation via unified `/config.json`
- **Easy configuration**: Part of unified configuration system
- **Graceful degradation**: Works seamlessly even when disabled or misconfigured
- **Single firmware**: Same firmware supports weather on/off through configuration

## 📍 Quick Setup (v0.7.1)

### 1. Unified Configuration System

Weather is now configured as part of the **unified `/config.json`** file on your SD card (no separate weather file needed).

### 2. Configure Weather in Unified Config

Add the weather section to your `/config.json` on the SD card:

```json
{
  "wifi": {
    "ssid": "YourWiFiNetwork",
    "password": "YourWiFiPassword"
  },
  "google_drive": {
    // ... your Google Drive config
  },
  "weather": {
    "enabled": true,
    "latitude": 40.7128,
    "longitude": -74.0060,
    "timezone": "America/New_York",
    "update_interval_minutes": 60,
    "celsius": true,
    "battery_threshold": 15,
    "max_age_hours": 3,
    "temperature_unit": "celsius",
    "wind_speed_unit": "kmh",
    "precipitation_unit": "mm"
  },
  "board": {
    // ... your board config
  }
}
```

### 3. Find Your Coordinates
- Visit [latlong.net](https://www.latlong.net/) or use your phone's GPS
- Enter your city name to get latitude and longitude
- Update the values in the `weather` section of `/config.json`

### 4. Runtime Control
- **Enable**: Set `"weather.enabled": true` in `/config.json`
- **Disable**: Set `"weather.enabled": false` in `/config.json`
- **No recompilation needed**: Same firmware works with weather on or off

### 5. Restart Your Photo Frame
The weather feature will automatically initialize based on your configuration.

## ⚙️ Configuration Options (v0.7.1)

### Unified Configuration Schema

Weather configuration is now part of the unified `/config.json` system:

```json
{
  "weather": {
    "enabled": false,
    "latitude": 40.7128,
    "longitude": -74.0060,
    "timezone": "Europe/Berlin",
    "update_interval_minutes": 60,
    "celsius": true,
    "battery_threshold": 15,
    "max_age_hours": 3,
    "temperature_unit": "celsius",
    "wind_speed_unit": "kmh",
    "precipitation_unit": "mm"
  }
}
```

### Configuration Parameters

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `enabled` | boolean | - | `false` | Enable/disable weather display |
| `latitude` | float | -90 to 90 | `40.7128` | Your location latitude |
| `longitude` | float | -180 to 180 | `-74.0060` | Your location longitude |
| `timezone` | string | - | `"auto"` | Timezone (e.g., "Europe/Berlin", "auto") |
| `update_interval_minutes` | integer | 60-1440* | `60` | Weather update frequency (min 1 hour) |
| `celsius` | boolean | - | `true` | Temperature unit (true=°C, false=°F) |
| `temperature_unit` | string | - | `"celsius"` | API temperature unit ("celsius", "fahrenheit") |
| `wind_speed_unit` | string | - | `"kmh"` | API wind speed unit ("kmh", "mph", "ms", "kn") |
| `precipitation_unit` | string | - | `"mm"` | API precipitation unit ("mm", "inch") |
| `battery_threshold` | integer | 5-50* | `15` | Disable weather below this battery % |
| `max_age_hours` | integer | 1-24* | `3` | Max age before data is stale |

*Values outside valid ranges are automatically clamped to the nearest valid value.

**Unit Options:**
- **Temperature**: "celsius" (°C), "fahrenheit" (°F)
- **Wind Speed**: "kmh" (km/h), "mph" (miles/h), "ms" (m/s), "kn" (knots)
- **Precipitation**: "mm" (millimeters), "inch" (inches)

### Example Locations
```json
{
  "new_york": { "latitude": 40.7128, "longitude": -74.0060 },
  "london": { "latitude": 51.5074, "longitude": -0.1278 },
  "tokyo": { "latitude": 35.6762, "longitude": 139.6503 },
  "sydney": { "latitude": -33.8688, "longitude": 151.2093 },
  "san_francisco": { "latitude": 37.7749, "longitude": -122.4194 }
}
```

## 🔋 Power Management

### Battery-Aware Update Intervals

The weather system automatically adjusts update frequency based on battery level:

| Battery Level | Update Interval | Behavior |
|---------------|----------------|----------|
| **>50%** | Base interval (default: 1 hour) | Normal operation |
| **30-50%** | 1.5x base interval | Moderate power saving |
| **20-30%** | 2x base interval | Low battery mode |
| **10-20%** | 4x base interval | Critical power saving |
| **<10%** | 8x base interval | Emergency power saving |
| **<threshold** | Disabled | Weather completely disabled |

### Failure Recovery

The system implements smart failure recovery:
- **1-3 failures**: 2x update interval
- **>3 failures**: 4x update interval
- **Minimum interval**: 15 minutes (1 hour when battery critical)
- **Maximum interval**: 4 hours (8 hours when battery low)

### Power Consumption

- **Normal operation**: ~0.2mAh per weather update
- **WiFi overhead**: None (uses existing WiFi sessions)
- **Battery impact**: <2% reduction in total battery life
- **Update strategy**: Only during image refresh cycles

## 🎨 Display Integration

### Display Layout
```
┌─────────────────────────────────────────────────────────────┐
│ 🕐 12:34 PM           📷 1/25 ☁️            🔋 85% 3.8V    │
│                                                             │
│                    [Main Image]                             │
│                                                             │
│ ┌─────────────────┐                                         │
│ │ ☁️  22°C        │                                         │
│ │     14°C/22°C   │                                         │
│ │     sunrise 6:54│                                         │
│ │     sunset: 19:44│                                        │
│ │     2024/12/20  │                                         │
│ └─────────────────┘                                         │
└─────────────────────────────────────────────────────────────┘
```

### Weather Overlay Panel

The weather information is displayed in a dedicated rounded panel in the bottom-left corner:

- **Weather Icon** (48x48px): Condition-specific icon on the left
- **Current Temperature**: Large, prominent display with API units (e.g., "22°C")
- **Daily Range**: Min/max temperatures with units (e.g., "14°C/22°C")
- **Sunrise Time**: Local sunrise time (e.g., "sunrise 6:54")
- **Sunset Time**: Local sunset time (e.g., "sunset: 19:44")
- **Current Date**: Today's date (e.g., "2024/12/20")

### Weather Icons

The weather display uses dedicated weather-specific icons that automatically adapt to day/night conditions:

| Weather Condition | Day Icon | Night Icon | WMO Codes |
|-------------------|----------|------------|-----------|
| **Clear Sky** | `clear_sky_day` | `clear_sky_night` | 0 |
| **Partly Cloudy** | `cloudy_day` | `cloudy_night` | 1-3 |
| **Cloudy with Rain** | `cloudy_day_rain` | `cloudy_night_rain` | Light rain conditions |
| **Overcast** | `overcast` | `overcast` | 45, 48 |
| **Overcast with Wind/Rain** | `overcast_wind_rain` | `overcast_wind_rain` | Windy overcast |
| **Rain** | `rain` | `rain` | 51-65 (moderate to heavy) |
| **Wind with Rain** | `wind_rain` | `wind_rain` | Rainy + wind >25 km/h |
| **Snow** | `snow` | `snow` | 71-77 |
| **Mixed Precipitation** | `snow_rain` | `snow_rain` | 68-69, 83-86 |
| **Thunderstorm** | `thunderstorm` | `thunderstorm` | 95-99 |
| **Wind** | `wind` | `wind` | High wind conditions |
| **Fog** | `fog` | `fog` | 45, 48 |
| **Unknown** | `error_icon` | `error_icon` | Invalid data |

**Smart Icon Selection:**
- Icons automatically switch between day/night variants based on current time
- Wind variants (>25 km/h) are selected for better weather accuracy  
- All icons are rendered at 48x48 pixels for optimal visibility

## 🔧 Technical Details

### Data Source
- **API Provider**: [Open-Meteo.com](https://open-meteo.com/)
- **API Key**: Not required (free service)
- **Update Protocol**: HTTPS GET requests
- **Data Format**: JSON response parsing

### API Endpoint
```
https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&daily=sunrise,sunset,apparent_temperature_max,apparent_temperature_min&current=is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m,apparent_temperature&timezone={timezone}&forecast_days=1&temperature_unit={temp_unit}&wind_speed_unit={wind_unit}&precipitation_unit={precip_unit}
```

**API Parameters:**
- **Current Data**: Apparent temperature, weather conditions, wind speed, precipitation, day/night status
- **Daily Data**: Min/max apparent temperatures, sunrise/sunset times
- **Timezone**: Configurable from `weather.json` (e.g., "Europe/Berlin", "America/New_York")
- **Units**: All measurement units are configurable via `weather.json`
- **Forecast**: 1-day forecast for daily min/max temperatures

**Example with Units:**
```
https://api.open-meteo.com/v1/forecast?latitude=47.3667&longitude=8.55&daily=sunrise,sunset,apparent_temperature_max,apparent_temperature_min&current=is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m,apparent_temperature&timezone=Europe%2FBerlin&forecast_days=1&temperature_unit=fahrenheit&wind_speed_unit=mph&precipitation_unit=inch
```

### Data Storage
- **Configuration**: `weather` section in unified `/config.json` on SD card
- **Cache**: `/weather_cache.json` on SD card (unchanged)
- **Cache Duration**: Based on `max_age_hours` setting
- **Offline Fallback**: Uses cached data when network unavailable
- **Fallback Values**: If config fails to load, uses values from `config.h` preprocessor constants

### Memory Usage
- **RAM Impact**: Minimal (~2KB additional)
- **Flash Impact**: ~15KB additional code
- **SD Storage**: ~1KB for config and cache files

## 🔍 Troubleshooting

### Weather Not Displaying

1. **Check Unified Configuration**
   ```bash
   # Verify unified config file exists and is valid JSON
   cat /path/to/sdcard/config.json

   # Check specifically for weather section
   jq '.weather' /path/to/sdcard/config.json
   ```

2. **Verify Weather is Enabled**
   ```bash
   # Check if weather is enabled in config
   jq '.weather.enabled' /path/to/sdcard/config.json
   ```

3. **Verify Coordinates**
   ```bash
   # Check coordinate values in unified config
   jq '.weather.latitude, .weather.longitude' /path/to/sdcard/config.json
   ```
   - Ensure latitude is between -90 and 90
   - Ensure longitude is between -180 and 180
   - Test coordinates at [open-meteo.com](https://open-meteo.com/)

4. **Check Battery Level**
   - Weather is disabled when battery < `weather.battery_threshold`
   - Default threshold is 15%
   - Check current setting: `jq '.weather.battery_threshold' /path/to/sdcard/config.json`

5. **Verify Network Connection**
   - Weather requires WiFi connection
   - Check if image updates are working (uses same WiFi session)

### Serial Monitor Debugging

Enable serial monitoring to see weather debug messages:

```
WeatherManager: Initializing from unified configuration...
WeatherManager: Initialized from unified config for location (40.713, -74.006)
WeatherManager: Fetching weather data during WiFi session...
WeatherManager: API URL: https://api.open-meteo.com/v1/forecast?latitude=40.7128&longitude=-74.0060&current=temperature_2m,relative_humidity_2m,weather_code&timezone=auto&forecast_days=1
WeatherManager: Received response (1234 bytes)
WeatherManager: Weather updated - 22.5°C, Clear
WeatherManager: Adaptive interval - Battery: 85%, Failures: 0, Base: 3600s, Final: 3600s
```

### Common Issues

| Issue | Cause | Solution |
|-------|--------|----------|
| **No weather icon** | Weather disabled or stale | Check config and battery level |
| **Wrong temperature** | Incorrect coordinates | Verify lat/lon values |
| **Stale data** | Network issues | Check WiFi connection |
| **High failure count** | API issues | Wait for automatic recovery |

### Reset Weather Settings

To reset weather settings in the unified configuration:

```bash
# Option 1: Disable weather in config
jq '.weather.enabled = false' /path/to/sdcard/config.json > temp.json && mv temp.json /path/to/sdcard/config.json

# Option 2: Remove entire weather section
jq 'del(.weather)' /path/to/sdcard/config.json > temp.json && mv temp.json /path/to/sdcard/config.json

# Option 3: Clear weather cache (keep config)
rm /path/to/sdcard/weather_cache.json

# Option 4: Complete reset (remove entire unified config)
rm /path/to/sdcard/config.json
# System will use fallback values from config.h
```

## 📊 Monitoring

### Debug Information

The weather system provides detailed logging for unified configuration:

```cpp
// Unified configuration initialization
WeatherManager: Initializing from unified configuration...
WeatherManager: Initialized from unified config for location (40.713, -74.006)
WeatherManager: Weather manager initialization failed or disabled

// Runtime control
WeatherManager: Weather is disabled in unified configuration
WeatherManager: Weather manager not configured - skipping weather fetch

// Update decisions
WeatherManager: Weather update not needed at this time
WeatherManager: Skipping weather update due to critical battery level (12%) - preserving power

// API communication
WeatherManager: Connecting to WiFi for weather data...
WeatherManager: Fetching weather data...
WeatherManager: Weather data updated successfully
WeatherManager: Weather fetch failed (2 consecutive failures)

// Display decisions
drawWeatherInfo: 22.1°C, Clear
drawWeatherInfo: Weather data invalid or stale, skipping display
```

### Performance Metrics

Monitor these values for optimal operation:

- **Battery impact**: Should be <2% reduction
- **Update success rate**: Should be >90% with good network
- **Failure recovery**: Should reset after successful update
- **Cache utilization**: Should provide offline resilience

## 🤝 Contributing

### Adding New Weather Icons

To add dedicated weather icons:

1. Create SVG icons in `icons/svg/`
2. Generate bitmap versions with `final_generate_icons_h.py`
3. Update `weather_icon_to_system_icon()` mapping
4. Test with different weather conditions

### Extending Weather Data

To add more weather information (humidity, wind, pressure):

1. Update `WeatherData` structure in `weather.h`
2. Modify API URL in `build_api_url()`
3. Extend JSON parsing in `parse_weather_response()`
4. Update display rendering in `draw_weather_info()`

## 📜 License

This weather feature is part of the ESP32 Photo Frame project and is licensed under the MIT License. Weather data is provided by [Open-Meteo.com](https://open-meteo.com/) under their open data license.

---

**Enjoy your weather-enhanced photo frame!** 🌤️📸