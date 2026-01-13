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

- `wifi` - WiFi network credentials (supports up to 3 networks)
- `google_drive_config` - Google Drive integration settings
- `sd_card_config` - SD card image source settings
- `board_config` - Board-specific settings including display orientation

**Important**: The weather system has been removed as of v0.12.0. The `weather_config` section is no longer supported.

## Complete Configuration Example

```json
{
  "_comment": "ESP32 Photo Frame Unified Configuration (v0.13.0)",
  "_instructions": "Replace placeholder values with your actual configuration",

  "wifi": [
    {
      "ssid": "YourHomeNetwork",
      "password": "YourHomePassword"
    },
    {
      "ssid": "YourOfficeNetwork",
      "password": "YourOfficePassword"
    },
    {
      "ssid": "YourMobileHotspot",
      "password": "YourHotspotPassword"
    }
  ],

  "google_drive_config": {
    "enabled": true,
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

  "sd_card_config": {
    "enabled": false,
    "images_directory": "/images",
    "use_toc_cache": true,
    "toc_max_age_seconds": 86400
  },

  "board_config": {
    "portrait_mode": false,
    "day_start_hour": 6,
    "day_end_hour": 23,
    "refresh": {
      "_comment": "Refresh interval settings (in seconds)",
      "min_seconds": 600,
      "max_seconds": 14400,
      "step": 300,
      "default": 1800,
      "low_battery_multiplier": 3
    }
  }
}
```

## Configuration Sections

### WiFi Configuration

**Required**: Yes
**Top-level key**: `wifi`

The WiFi configuration supports up to 3 networks. The system will attempt to connect to each network in order until a successful connection is made.

#### Array Format (v0.11.0+)
```json
{
  "wifi": [
    {"ssid": "Network1", "password": "password1"},
    {"ssid": "Network2", "password": "password2"},
    {"ssid": "Network3", "password": "password3"}
  ]
}
```

#### Legacy Format (v0.10.0 and earlier - still supported)
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

**Required**: No (can be disabled if using SD Card as image source)
**Top-level key**: `google_drive_config`

The Google Drive configuration allows using Google Drive as an image source. It can be enabled or disabled based on your needs.

#### Enable/Disable

```json
{
  "google_drive_config": {
    "enabled": true
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | true | Enable or disable Google Drive as image source |

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
| `service_account_email` | string | Yes* | Google service account email |
| `private_key_pem` | string | Yes* | Private key in PEM format (include BEGIN/END markers) |
| `client_id` | string | Yes* | Client ID from service account |

*Required only if Google Drive is enabled

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

### SD Card Configuration

**Required**: No (can be disabled if using Google Drive as image source)
**Top-level key**: `sd_card_config`

The SD Card configuration allows using local SD card directory as an image source instead of or in addition to Google Drive.

```json
{
  "sd_card_config": {
    "enabled": false,
    "images_directory": "/images",
    "use_toc_cache": true,
    "toc_max_age_seconds": 86400
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | false | Enable/disable SD card as image source |
| `images_directory` | string | "/images" | Directory path containing image files |
| `use_toc_cache` | boolean | true | Enable TOC caching for performance |
| `toc_max_age_seconds` | integer | 86400 | Maximum TOC cache age (24 hours default) |

**Note**: At least one image source (Google Drive or SD Card) must be enabled. If both are enabled, SD Card takes precedence.

### Board Configuration

**Required**: No (uses defaults from firmware)
**Top-level key**: `board_config`

```json
{
  "board_config": {
    "portrait_mode": false,
    "day_start_hour": 6,
    "day_end_hour": 23,
    "refresh": {
      "min_seconds": 600,
      "max_seconds": 14400,
      "step": 300,
      "default": 1800,
      "low_battery_multiplier": 3
    }
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `portrait_mode` | boolean | false | Enable portrait (vertical) display orientation |
| `day_start_hour` | integer | 6 | Hour when display updates start (0-23) |
| `day_end_hour` | integer | 23 | Hour when display updates stop (0-23) |

#### Refresh Settings

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `min_seconds` | integer | 600 | Minimum refresh interval (seconds) |
| `max_seconds` | integer | 14400 | Maximum refresh interval (seconds) |
| `step` | integer | 300 | Step size for potentiometer adjustment (seconds) |
| `default` | integer | 1800 | Default interval when potentiometer is not used |
| `low_battery_multiplier` | integer | 3 | Multiplier for refresh interval when battery is low |

**Note**: The `default` value is used when `USE_POTENTIOMETER` is not defined in the firmware. When using a potentiometer, the `min_seconds`, `max_seconds`, and `step` values control the potentiometer range.

## Migration Notes

### Version Changes

#### v0.13.0
- Added `portrait_mode` to `board_config` for dynamic display orientation
- Added `sd_card_config` section for local image source support

#### v0.12.0
- **Removed** `weather_config` section entirely (weather system removed)

#### v0.11.0
- WiFi configuration now supports array format for multiple networks (up to 3)
- Legacy single network format still supported for backward compatibility

#### v0.7.0
If you have an existing configuration file using the old key names, you must update them:

| Old Key (v0.6.x and earlier) | New Key (v0.7.0+) |
|------------------------------|-------------------|
| `"google_drive"` | `"google_drive_config"` |
| `"weather"` | `"weather_config"` (removed in v0.12.0) |
| `"board"` | `"board_config"` |

## Minimal Configuration Examples

### Using Google Drive as Image Source

```json
{
  "wifi": [
    {
      "ssid": "YourWiFiNetwork",
      "password": "YourWiFiPassword"
    }
  ],
  "google_drive_config": {
    "enabled": true,
    "authentication": {
      "service_account_email": "photoframe@myproject.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
      "client_id": "123456789"
    },
    "drive": {
      "folder_id": "1ABC...XYZ",
      "use_insecure_tls": true
    }
  }
}
```

### Using SD Card as Image Source

```json
{
  "wifi": [
    {
      "ssid": "YourWiFiNetwork",
      "password": "YourWiFiPassword"
    }
  ],
  "sd_card_config": {
    "enabled": true,
    "images_directory": "/images"
  },
  "google_drive_config": {
    "enabled": false
  }
}
```

### Portrait Mode Configuration

```json
{
  "wifi": [
    {
      "ssid": "YourWiFiNetwork",
      "password": "YourWiFiPassword"
    }
  ],
  "board_config": {
    "portrait_mode": true
  },
  "sd_card_config": {
    "enabled": true,
    "images_directory": "/6c/portrait/bin"
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

### No Images Displaying

1. Verify at least one image source is enabled (`google_drive_config.enabled` or `sd_card_config.enabled`)
2. For SD Card: Check that images exist in the configured `images_directory`
3. For Google Drive: Verify authentication credentials and folder_id
4. Check that image files are in binary format (`.bin` extension)

### Google Drive Authentication Failing

1. Verify `"google_drive_config"` key (not `"google_drive"`)
2. Check service account email is correct
3. Ensure private key includes BEGIN/END markers
4. Verify folder_id is accessible by service account
5. Check system time is correct (required for JWT authentication)

### WiFi Connection Issues

1. If using array format, verify all network entries are valid
2. Check that at least one configured network is available
3. Monitor serial console for connection attempts
4. Ensure passwords don't contain special JSON characters without proper escaping

### Display Orientation Wrong

1. Check `board_config.portrait_mode` setting matches your display mounting
2. Portrait mode requires images processed for vertical orientation
3. Verify image directory contains appropriately oriented images

## Important Notes

- **Image Source Priority**: If both Google Drive and SD Card are enabled, SD Card takes precedence
- **Portrait Mode**: This is a runtime configuration - no firmware recompilation needed
- **WiFi Networks**: System tries networks in order until successful connection
- **Binary Format Only**: ESP32 firmware only supports `.bin` image format (v0.12.0+)
- **Configuration Validation**: At least one image source must be enabled for valid configuration

## See Also

- [example_config.json](example_config.json) - Complete example configuration
- [Google Drive API Documentation](../docs/google_drive_api.md)
- [README.md](../README.md) - Project overview
