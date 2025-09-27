# Google Drive API Documentation

## Overview

The ESP32 Photo Frame project includes a comprehensive Google Drive integration system that provides reliable cloud storage access with advanced caching, streaming architecture, and robust error handling. **Version 0.7.1** introduces unified configuration system integration, allowing Google Drive settings to be managed through a single `/config.json` file alongside WiFi, Weather, and Board configurations.

## Architecture

### Two-File TOC System (2025)

The enhanced Table of Contents (TOC) system uses two separate files for improved data integrity:

- **`toc_data.txt`** - Contains file entries in `id|name` format
- **`toc_meta.txt`** - Contains metadata: timestamp, file count, and data file size

**Benefits:**
- Enhanced data integrity with size verification
- Better error recovery and corruption detection
- Atomic operations for metadata updates
- Graceful fallback to cached content

### Memory-Efficient Streaming

All Google Drive operations use streaming architecture to handle large collections (350+ files) on any ESP32 variant:

- Direct disk writing eliminates intermediate storage
- Memory usage stays constant regardless of collection size
- Automatic memory monitoring with safety margins
- PSRAM detection for performance bonuses

## Core Classes

### `google_drive` Class

High-level interface for Google Drive operations with caching and optimization features.

#### Constructor

```cpp
google_drive::google_drive()
```

Creates a new Google Drive instance with default configuration.

#### Configuration Methods

##### `initialize_from_unified_config()`

```cpp
photo_frame_error_t initialize_from_unified_config(const unified_config::google_drive_config& config)
```

Initialize Google Drive client from unified configuration structure.

**Parameters:**
- `config` - Google Drive configuration from unified config system

**Returns:** Error code indicating success or failure

**Unified Configuration Format:**
```json
{
  "wifi": {
    "ssid": "YourWiFiNetwork",
    "password": "YourWiFiPassword"
  },
  "google_drive": {
    "authentication": {
      "service_account_email": "your-service-account@project.iam.gserviceaccount.com",
      "private_key_pem": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n",
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
  "weather": {
    // ... weather config
  },
  "board": {
    // ... board config
  }
}
```

##### `create_directories()`

```cpp
photo_frame_error_t create_directories(sd_card& sdCard)
```

Create necessary directories on SD card for Google Drive local cache.

**Parameters:**
- `sdCard` - Reference to SD card object

**Returns:** Error code indicating success or failure

**Created Structure:**
```
/gdrive/
├── cache/      # Downloaded images
├── temp/       # Temporary files
├── toc_data.txt
└── toc_meta.txt
```

#### TOC (Table of Contents) Methods

##### `retrieve_toc()`

```cpp
size_t retrieve_toc(sd_card& sdCard, bool batteryConservationMode = false)
```

Retrieve the Table of Contents from Google Drive and store locally using the 2-file system.

**Parameters:**
- `sdCard` - Reference to SD card object
- `batteryConservationMode` - If true, uses cached TOC even if expired to save battery

**Returns:** Total number of files in TOC, or 0 if failed

**Behavior:**
- Checks local TOC age against `toc_max_age_seconds`
- Downloads fresh TOC if expired (unless battery conservation mode)
- Uses streaming architecture for memory efficiency
- Automatically falls back to cached TOC on network failures

##### `get_toc_file_count()`

```cpp
size_t get_toc_file_count(sd_card& sdCard, photo_frame_error_t* error = nullptr)
```

Get the file count from the TOC metadata efficiently.

**Parameters:**
- `sdCard` - Reference to SD card object
- `error` - Optional pointer to error code

**Returns:** Number of files in TOC, or 0 if error

**Features:**
- Reads from `toc_meta.txt` for fast access
- Validates data file integrity
- Provides detailed error information

##### `get_toc_file_by_index()`

```cpp
google_drive_file get_toc_file_by_index(sd_card& sdCard, size_t index, photo_frame_error_t* error = nullptr)
```

Get a specific file entry by index from the TOC.

**Parameters:**
- `sdCard` - Reference to SD card object
- `index` - Zero-based index of file to retrieve
- `error` - Optional pointer to error code

**Returns:** `google_drive_file` at specified index, or empty file if error

##### `get_toc_file_by_name()`

```cpp
google_drive_file get_toc_file_by_name(sd_card& sdCard, const char* filename, photo_frame_error_t* error = nullptr)
```

Find a file by name in the TOC.

**Parameters:**
- `sdCard` - Reference to SD card object
- `filename` - Name of file to search for
- `error` - Optional pointer to error code

**Returns:** `google_drive_file` with specified name, or empty file if not found

#### File Operations

##### `download_file()`

```cpp
fs::File download_file(sd_card& sdCard, google_drive_file file, photo_frame_error_t* error)
```

Download a file from Google Drive to SD card cache.

**Parameters:**
- `sdCard` - Reference to SD card object
- `file` - Google Drive file object to download
- `error` - Pointer to store error code

**Returns:** `fs::File` object for downloaded file, or empty file on failure

**Features:**
- Smart caching (checks for existing cached files)
- Atomic operations (download to temp, then move)
- Automatic token refresh if expired
- Detailed error reporting

#### Utility Methods

##### `get_last_image_source()`

```cpp
image_source_t get_last_image_source() const
```

Get the source of the last downloaded/accessed file.

**Returns:** `IMAGE_SOURCE_CLOUD` or `IMAGE_SOURCE_LOCAL_CACHE`

##### `set_last_image_source()`

```cpp
void set_last_image_source(image_source_t source)
```

Set the source of the current image for tracking purposes.

**Parameters:**
- `source` - Source type (cloud or local cache)

##### `cleanup_temporary_files()`

```cpp
uint32_t cleanup_temporary_files(sd_card& sdCard, bool force)
```

Clean up temporary files and manage cache storage.

**Parameters:**
- `sdCard` - Reference to SD card object
- `force` - If true, forces cleanup of all files including cache

**Returns:** Number of files cleaned up

**Features:**
- Removes temporary files from incomplete downloads
- Cleans orphaned cache files not in current TOC
- Monitors disk space (auto-cleanup at 20% free space)
- Age-based cleanup of old TOC files

#### Path Helper Methods

##### `get_toc_file_path()`

```cpp
String get_toc_file_path() const
```

Get the full path to the TOC data file.

**Returns:** Path to `toc_data.txt`

##### `get_toc_meta_file_path()`

```cpp
String get_toc_meta_file_path() const
```

Get the full path to the TOC metadata file.

**Returns:** Path to `toc_meta.txt`

##### `get_cache_dir_path()`

```cpp
String get_cache_dir_path() const
```

Get the full path to the cache directory.

**Returns:** Path to cache directory

##### `get_cached_file_path()`

```cpp
String get_cached_file_path(const String& filename) const
```

Get the full path for a cached file.

**Parameters:**
- `filename` - Name of the file

**Returns:** Full path to cached file

### `google_drive_client` Class

Low-level Google Drive API client with streaming capabilities and advanced error handling.

#### Constructor

```cpp
google_drive_client::google_drive_client(const google_drive_client_config& config)
```

Create a new Google Drive client with specified configuration.

**Parameters:**
- `config` - Client configuration structure

#### Authentication Methods

##### `get_access_token()`

```cpp
photo_frame_error_t get_access_token()
```

Retrieve an access token for API authentication.

**Returns:** Error code indicating success or failure

**Features:**
- JWT-based service account authentication
- Automatic retry with exponential backoff
- Rate limiting compliance
- Comprehensive error handling

##### `set_access_token()`

```cpp
void set_access_token(const google_drive_access_token& token)
```

Set an existing access token.

**Parameters:**
- `token` - Access token structure

##### `is_token_expired()`

```cpp
bool is_token_expired(int marginSeconds = 60)
```

Check if current access token is expired or about to expire.

**Parameters:**
- `marginSeconds` - Safety margin before expiration

**Returns:** true if token needs refresh

#### File Listing Methods

##### `list_files_streaming()`

```cpp
size_t list_files_streaming(const char* folderId, sd_card& sdCard, const char* tocFilePath, int pageSize = 50)
```

List files in Google Drive folder with streaming directly to TOC file.

**Parameters:**
- `folderId` - Google Drive folder ID
- `sdCard` - Reference to SD card object
- `tocFilePath` - Path to TOC file
- `pageSize` - Files per API request

**Returns:** Total number of files written to TOC

**Features:**
- Memory-efficient streaming (no intermediate storage)
- Automatic pagination handling
- Direct disk writing as files are parsed
- Rate limiting and retry logic

##### `download_file()`

```cpp
photo_frame_error_t download_file(const String& fileId, fs::File* outFile)
```

Download a file by ID to specified file handle.

**Parameters:**
- `fileId` - Google Drive file ID
- `outFile` - Pointer to output file handle

**Returns:** Error code indicating success or failure

**Features:**
- Streaming download (no memory buffering)
- Chunked transfer encoding support
- Automatic token refresh
- Robust error handling

#### Security Methods

##### `set_root_ca_certificate()`

```cpp
void set_root_ca_certificate(const String& rootCA)
```

Set root CA certificate for SSL/TLS connections.

**Parameters:**
- `rootCA` - Certificate in PEM format

### Data Structures

#### `google_drive_file`

Represents a file from Google Drive.

```cpp
class google_drive_file {
public:
    String id;   // Unique file identifier
    String name; // Display name of file

    google_drive_file();
    google_drive_file(const String& id, const String& name);
};
```

#### `google_drive_json_config`

JSON-based configuration structure.

```cpp
typedef struct {
    String serviceAccountEmail;
    String privateKeyPem;
    String clientId;
    String folderId;
    String rootCaPath;
    int listPageSize;
    bool useInsecureTls;
    String localPath;
    unsigned long tocMaxAgeSeconds;
    int maxRequestsPerWindow;
    int rateLimitWindowSeconds;
    int minRequestDelayMs;
    int maxRetryAttempts;
    int backoffBaseDelayMs;
    int maxWaitTimeMs;
} google_drive_json_config;
```

#### `image_source_t`

Enumeration for tracking image sources.

```cpp
typedef enum image_source {
    IMAGE_SOURCE_CLOUD,      // Downloaded from Google Drive
    IMAGE_SOURCE_LOCAL_CACHE // Loaded from local cache
} image_source_t;
```

## Usage Examples

### Basic Setup (v0.7.1 Unified Configuration)

```cpp
#include "google_drive.h"
#include "sd_card.h"
#include "unified_config.h"

// Initialize SD card
photo_frame::sd_card sdCard;
sdCard.init();

// Load unified configuration
photo_frame::unified_config systemConfig;
auto configError = photo_frame::load_unified_config_with_fallback(sdCard, "/config.json", systemConfig);
if (configError != photo_frame::error_type::None) {
    Serial.println("Failed to load unified configuration");
    return;
}

// Initialize Google Drive from unified config
photo_frame::google_drive drive;
auto error = drive.initialize_from_unified_config(systemConfig.google_drive);
if (error != photo_frame::error_type::None) {
    Serial.println("Failed to initialize Google Drive");
    return;
}

// Create cache directories
drive.create_directories(sdCard);
```


### Retrieve File List

```cpp
// Get fresh TOC from Google Drive
size_t totalFiles = drive.retrieve_toc(sdCard, false);
if (totalFiles == 0) {
    Serial.println("No files found or error occurred");
    return;
}

Serial.print("Found ");
Serial.print(totalFiles);
Serial.println(" files");
```

### Download and Display Image

```cpp
// Select random image
size_t imageIndex = random(0, totalFiles);
photo_frame::photo_frame_error_t error;

// Get file info
auto selectedFile = drive.get_toc_file_by_index(sdCard, imageIndex, &error);
if (error != photo_frame::error_type::None) {
    Serial.println("Failed to get file info");
    return;
}

// Download file (or use cached version)
fs::File imageFile = drive.download_file(sdCard, selectedFile, &error);
if (error != photo_frame::error_type::None) {
    Serial.println("Failed to download file");
    return;
}

// Check image source
if (drive.get_last_image_source() == photo_frame::IMAGE_SOURCE_CLOUD) {
    Serial.println("Downloaded from cloud");
} else {
    Serial.println("Loaded from cache");
}

// Use imageFile for display...
imageFile.close();
```

### Cleanup and Maintenance

```cpp
// Clean up temporary files (daily maintenance)
uint32_t cleanedFiles = drive.cleanup_temporary_files(sdCard, false);
Serial.print("Cleaned up ");
Serial.print(cleanedFiles);
Serial.println(" temporary files");

// Force cleanup (if low on space)
if (lowDiskSpace) {
    cleanedFiles = drive.cleanup_temporary_files(sdCard, true);
    Serial.print("Force cleaned ");
    Serial.print(cleanedFiles);
    Serial.println(" files");
}
```

## Error Handling

### Error Types

The Google Drive API uses comprehensive error reporting with specific error codes:

- **Configuration Errors** (140-159): JSON parsing, validation failures
- **Authentication Errors** (40-49): OAuth token issues, JWT creation failures
- **Network Errors** (120-139): WiFi, HTTP transport issues
- **Storage Errors** (100-119): SD card access, file system problems
- **Google Drive API Errors** (50-69): API quotas, permissions, rate limiting

### Best Practices

```cpp
// Always check return values
photo_frame::photo_frame_error_t error;
size_t fileCount = drive.get_toc_file_count(sdCard, &error);
if (error != photo_frame::error_type::None) {
    Serial.print("TOC error: ");
    Serial.println(error.message);
    // Handle error appropriately
    return;
}

// Use error context for debugging
if (error.severity >= photo_frame::ERROR_SEVERITY_ERROR) {
    Serial.print("Critical error in ");
    Serial.print(error.source_file);
    Serial.print(":");
    Serial.println(error.source_line);
}
```

## Performance Characteristics

### Memory Usage

| Platform | TOC Size (350 files) | Memory Usage | Status |
|----------|---------------------|--------------|--------|
| ESP32-C6 | ~35KB | 38% stable | ✅ Recommended |
| ESP32-S3 | ~35KB | 30-35% | ✅ Excellent |
| ESP32-S3 + PSRAM | ~35KB | 25-30% | 🚀 Premium |
| Standard ESP32 | ~35KB | 45-50% | ✅ Good |

### Timing Benchmarks

- **TOC Retrieval**: 8-15 seconds (350 files)
- **File Download**: 2-5 seconds per file
- **Cache Access**: <1 second
- **Cleanup Operations**: 1-3 seconds

### Rate Limiting

The API includes built-in rate limiting to comply with Google Drive quotas:

- **Default Limits**: 100 requests per 100 seconds
- **Minimum Delay**: 500ms between requests
- **Exponential Backoff**: Automatic retry with increasing delays
- **Quota Protection**: Prevents API quota exhaustion

## Migration Guide

### From Legacy Configuration (v0.7.0 → v0.7.1)

**Unified Configuration Migration:**
```json
// OLD SYSTEM (separate files):
// - /wifi.txt                    ❌ No longer supported
// - /google_drive_config.json    ❌ No longer supported
// - /weather.json                ❌ No longer supported

// NEW UNIFIED SYSTEM:
// - /config.json                 ✅ Single configuration file

// Remove deprecated field:
"toc_filename": "toc.txt"  // ❌ No longer needed
```

**Update Method Calls:**
```cpp
// OLD API (v0.7.0) - REMOVED in v0.7.1:
// drive.initialize_from_json(sdCard, "/google_drive_config.json");  // ❌ Method removed
// drive.retrieve_toc(batteryMode);                                  // ❌ Old signature removed
// drive.get_toc_file_count(&error);                                 // ❌ Old signature removed

// NEW API (v0.7.1):
drive.initialize_from_unified_config(systemConfig.google_drive);     // ✅ Only method available
drive.retrieve_toc(sdCard, batteryMode);                             // ✅ Updated signature
drive.get_toc_file_count(sdCard, &error);                            // ✅ Updated signature
```

**Migration Benefits:**
- **Single File**: All configuration in one place (`/config.json`)
- **Faster Startup**: Single SD card read instead of 3 separate reads
- **Enhanced Validation**: Automatic value capping and fallback handling
- **Runtime Control**: Weather and other features controlled without recompilation
- **Better Error Handling**: Extended sleep on SD card failure

**Automatic Migration:**
- Old TOC files automatically removed
- Existing cache files remain valid
- Configuration validation handles deprecated fields gracefully
- **⚠️ Legacy methods have been removed**: Code must be updated to use unified configuration system

## Troubleshooting

### Common Issues

**"TOC file integrity check failed"**
- Solution: Clear cache directory, force refresh TOC

**"Failed to parse JSON response"**
- Check WiFi connection stability
- Verify Google Drive API quota
- Ensure proper TLS certificate configuration
- Verify unified `/config.json` syntax is valid

**"Configuration load failed"**
- Check `/config.json` file exists on SD card
- Verify JSON syntax using online validator
- Ensure all required google_drive configuration fields are present
- Check SD card file system integrity

**"Access token expired"**
- Tokens automatically refresh, check service account configuration
- Verify system time accuracy

**"Insufficient memory for Google Drive operations"**
- Should not occur with streaming architecture
- Check for memory leaks in other code

### Debug Features

```cpp
// Enable detailed logging
#define DEBUG_MODE 1

// Memory monitoring
logMemoryUsage("Before Google Drive operation");

// Check memory availability
if (!checkMemoryAvailable(4096)) {
    Serial.println("Low memory detected");
}
```

This API documentation provides comprehensive coverage of the enhanced Google Drive integration system, focusing on the new 2-file TOC architecture, streaming capabilities, and robust error handling that make large cloud collections accessible on all ESP32 platforms.