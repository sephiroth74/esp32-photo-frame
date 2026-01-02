# Configuration File Reference

## Overview

The ESP32 Photo Frame uses a unified configuration system via a single `/config.json` file located on the SD card root. This document describes the complete configuration structure and all available options.

## Configuration File Location

The configuration file must be placed at the root of your SD card:
```
/config.json
```

## Complete Configuration Structure

The configuration file uses the following top-level keys:

- `wifi` - WiFi network credentials
- `google_drive_config` - Google Drive integration settings
- `weather_config` - Weather display configuration
- `board_config` - Board-specific settings

**Important**: The configuration keys must match exactly as shown below. Previous versions used different key names (`google_drive`, `weather`, `board`) which are **not compatible** with the current firmware.

## Complete Configuration Example

```json
{
  "_comment": "ESP32 Photo Frame Unified Configuration (v0.7.1)",
  "_instructions": "Replace placeholder values with your actual configuration",

  "wifi": {
    "ssid": "YourWiFiNetwork",
    "password": "YourWiFiPassword"
  },

  "google_drive_config": {
    "authentication": {
      "service_account_email": "your-service-account@your-project.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\nYOUR_PRIVATE_KEY_CONTENT_HERE\n-----END PRIVATE KEY-----\n",
      "client_id": "your-client-id"
    },
    "drive": {
      "folder_id": "your-google-drive-folder-id",
      "root_ca_path": "/certs/google_root_ca.pem",
      "list_page_size": 100,
      "use_insecure_tls": false
    },
    "caching": {
      "local_path": "/gdrive",
      "toc_max_age_seconds": 604800
    },
    "rate_limiting": {
      "max_requests_per_window": 100,
      "rate_limit_window_seconds": 100,
      "min_request_delay_ms": 500,
      "max_retry_attempts": 3,
      "backoff_base_delay_ms": 5000,
      "max_wait_time_ms": 30000
    }
  },

  "weather_config": {
    "_location_name": "New York City, USA",
    "enabled": true,
    "latitude": 40.7128,
    "longitude": -74.0060,
    "update_interval_minutes": 120,
    "celsius": true,
    "battery_threshold": 15,
    "max_age_hours": 3,
    "timezone": "auto",
    "temperature_unit": "celsius",
    "wind_speed_unit": "kmh",
    "precipitation_unit": "mm"
  },

  "board_config": {
    "day_start_hour": 6,
    "day_end_hour": 23,
    "refresh": {
      "_comment": "Refresh interval settings (in seconds) controlled by potentiometer",
      "min_seconds": 600,
      "max_seconds": 14400,
      "step": 300,
      "low_battery_multiplier": 3
    }
  }
}
```

## Configuration Sections

### WiFi Configuration

**Required**: Yes
**Top-level key**: `wifi`

```json
{
  "wifi": {
    "ssid": "YourWiFiNetwork",
    "password": "YourWiFiPassword"
  }
}
```

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `ssid` | string | Yes | WiFi network name |
| `password` | string | Yes | WiFi network password |

### Google Drive Configuration

**Required**: Yes
**Top-level key**: `google_drive_config`

#### Authentication Section

```json
{
  "google_drive_config": {
    "authentication": {
      "service_account_email": "photoframe@myproject.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
      "client_id": "123456789"
    }
  }
}
```

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `service_account_email` | string | Yes | Google service account email |
| `private_key_pem` | string | Yes | Private key in PEM format (include BEGIN/END markers) |
| `client_id` | string | Yes | Client ID from service account |

#### Drive Section

```json
{
  "google_drive_config": {
    "drive": {
      "folder_id": "1ABC...XYZ",
      "root_ca_path": "/certs/google_root_ca.pem",
      "list_page_size": 100,
      "use_insecure_tls": false
    }
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `folder_id` | string | Required | Google Drive folder ID containing images |
| `root_ca_path` | string | "" | Path to root CA certificate (optional if using insecure TLS) |
| `list_page_size` | integer | 100 | Number of files per API request (max 1000) |
| `use_insecure_tls` | boolean | true | Skip TLS certificate verification (use true for simplicity) |

#### Caching Section

```json
{
  "google_drive_config": {
    "caching": {
      "local_path": "/gdrive",
      "toc_max_age_seconds": 604800
    }
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `local_path` | string | "/gdrive" | SD card path for cache storage |
| `toc_max_age_seconds` | integer | 604800 | Table of contents max age (7 days) |

#### Rate Limiting Section

```json
{
  "google_drive_config": {
    "rate_limiting": {
      "max_requests_per_window": 100,
      "rate_limit_window_seconds": 100,
      "min_request_delay_ms": 500,
      "max_retry_attempts": 3,
      "backoff_base_delay_ms": 5000,
      "max_wait_time_ms": 30000
    }
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `max_requests_per_window` | integer | 100 | Maximum API requests per time window |
| `rate_limit_window_seconds` | integer | 100 | Time window for rate limiting (seconds) |
| `min_request_delay_ms` | integer | 500 | Minimum delay between requests (ms) |
| `max_retry_attempts` | integer | 3 | Maximum retry attempts for failed requests |
| `backoff_base_delay_ms` | integer | 5000 | Base delay for exponential backoff (ms) |
| `max_wait_time_ms` | integer | 30000 | Maximum wait time before giving up (ms) |

### Weather Configuration

**Required**: No (can be disabled)
**Top-level key**: `weather_config`

```json
{
  "weather_config": {
    "enabled": true,
    "latitude": 40.7128,
    "longitude": -74.0060,
    "update_interval_minutes": 120,
    "celsius": true,
    "battery_threshold": 15,
    "max_age_hours": 3,
    "timezone": "auto",
    "temperature_unit": "celsius",
    "wind_speed_unit": "kmh",
    "precipitation_unit": "mm"
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | false | Enable/disable weather display |
| `latitude` | float | 0.0 | Location latitude (-90 to 90) |
| `longitude` | float | 0.0 | Location longitude (-180 to 180) |
| `update_interval_minutes` | integer | 120 | Weather update frequency (minutes) |
| `celsius` | boolean | true | Use Celsius (true) or Fahrenheit (false) |
| `battery_threshold` | integer | 15 | Disable weather below this battery % |
| `max_age_hours` | integer | 3 | Maximum data age before considered stale |
| `timezone` | string | "auto" | IANA timezone or "auto" for automatic |
| `temperature_unit` | string | "celsius" | API temperature unit ("celsius", "fahrenheit") |
| `wind_speed_unit` | string | "kmh" | API wind speed unit ("kmh", "mph", "ms", "kn") |
| `precipitation_unit` | string | "mm" | API precipitation unit ("mm", "inch") |

### Board Configuration

**Required**: No (uses defaults from firmware)
**Top-level key**: `board_config`

```json
{
  "board_config": {
    "day_start_hour": 6,
    "day_end_hour": 23,
    "refresh": {
      "min_seconds": 600,
      "max_seconds": 14400,
      "step": 300,
      "low_battery_multiplier": 3
    }
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `day_start_hour` | integer | 6 | Hour when display updates start (0-23) |
| `day_end_hour` | integer | 23 | Hour when display updates stop (0-23) |

#### Refresh Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `min_seconds` | integer | 300 | Minimum refresh interval (seconds) |
| `max_seconds` | integer | 14400 | Maximum refresh interval (seconds) |
| `step` | integer | 300 | Step size for potentiometer adjustment (seconds) |
| `low_battery_multiplier` | integer | 3 | Multiplier for refresh interval when battery is low |

## Migration from Old Configuration

If you have an existing configuration file using the old key names, you must update it to use the new keys:

| Old Key (v0.6.x and earlier) | New Key (v0.7.0+) |
|------------------------------|-------------------|
| `"google_drive"` | `"google_drive_config"` |
| `"weather"` | `"weather_config"` |
| `"board"` | `"board_config"` |

The `wifi` section remains unchanged.

## Minimal Configuration Example

The minimal configuration requires only WiFi and Google Drive authentication:

```json
{
  "wifi": {
    "ssid": "YourWiFiNetwork",
    "password": "YourWiFiPassword"
  },
  "google_drive_config": {
    "authentication": {
      "service_account_email": "photoframe@myproject.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
      "client_id": "123456789"
    },
    "drive": {
      "folder_id": "1ABC...XYZ",
      "use_insecure_tls": true
    }
  },
  "weather_config": {
    "enabled": false
  }
}
```

All other settings will use firmware defaults.

## Validation

The firmware validates the configuration file at startup and will:
- Log warnings for missing optional sections
- Use fallback values from firmware defines for missing parameters
- Report errors for missing required parameters (WiFi, Google Drive authentication)

Check the serial console output for configuration validation messages:
```
[unified_config] Loading configuration from: /config.json
[unified_config] WiFi configuration - SSID: YourWiFiNetwork
[unified_config] Google Drive authentication - email: photoframe@...
[unified_config] Configuration loaded successfully
```

## Troubleshooting

### Configuration Not Loading

1. Verify the file is at `/config.json` on SD card root
2. Check JSON syntax is valid (use a JSON validator)
3. Ensure key names match exactly (case-sensitive)
4. Check serial console for error messages

### Weather Not Working

1. Verify `"weather_config"` key (not `"weather"`)
2. Check `"enabled": true` is set
3. Verify latitude/longitude are valid coordinates
4. Check battery level is above threshold

### Google Drive Authentication Failing

1. Verify `"google_drive_config"` key (not `"google_drive"`)
2. Check service account email is correct
3. Ensure private key includes BEGIN/END markers
4. Verify folder_id is accessible by service account

## See Also

- [example_config.json](example_config.json) - Complete example configuration
- [Google Drive API Documentation](../docs/google_drive_api.md)
- [Weather Feature Documentation](../docs/weather.md)
- [README.md](../README.md) - Project overview
