# SD Card Configuration Documentation

## Overview

The ESP32 Photo Frame supports using the SD card as the primary image source, providing a completely offline operation mode without requiring WiFi or Google Drive. This feature was introduced in v0.13.0 as an alternative to cloud storage.

## Configuration

### Enabling SD Card Mode

To use the SD card as your image source, configure the following in `/config.json`:

```json
{
  "sd_card_config": {
    "enabled": true,
    "images_directory": "/images",
    "use_toc_cache": true,
    "toc_max_age_seconds": 86400
  },
  "google_drive_config": {
    "enabled": false
  }
}
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | true | Enable SD card as image source |
| `images_directory` | string | "/images" | Path to directory containing processed images |
| `use_toc_cache` | boolean | true | Enable Table of Contents caching for faster startup |
| `toc_max_age_seconds` | integer | 86400 | How long to cache the TOC before rebuilding (seconds) |

## Image Storage

### Directory Structure

Place your processed binary images (.bin files) in the configured directory on the SD card:

```
SD Card Root
├── config.json
├── images/
│   ├── photo1.bin
│   ├── photo2.bin
│   ├── photo3.bin
│   └── ...
└── certs/
    └── google_root_ca.pem (optional, only for Google Drive mode)
```

### Image Format

- **Format**: Binary (.bin) files only
- **Resolution**: Match your display (e.g., 800x480 for 7.5" display)
- **Color Mode**: Black/White or 6-color depending on your display
- **Processing**: Use the Rust processor, Android app, or Flutter app to create binary files

## Performance

### With TOC Caching Enabled

- **First startup**: 2-3 seconds to build TOC for 500+ files
- **Subsequent startups**: <100ms to validate cached TOC
- **Image selection**: Near-instantaneous
- **Battery impact**: Minimal SD card access

### Without TOC Caching

- **Every startup**: 30+ seconds to iterate directory
- **Image selection**: 1-2 seconds per selection
- **Battery impact**: Higher due to frequent SD card access

## TOC (Table of Contents) System

The TOC system creates two cache files on the SD card:

- `sd_toc_data.txt`: Contains list of all image files
- `sd_toc_meta.txt`: Contains metadata (directory path, file count, timestamp)

### TOC Rebuilding

The TOC is automatically rebuilt when:
- TOC files don't exist
- Directory path changes in configuration
- File extension filter changes
- Reset button is pressed (wakeup reason undefined)
- TOC files are corrupted

### TOC Persistence

- TOC remains valid across deep sleep cycles
- EXT1 wakeup (button press) does NOT trigger rebuild
- Timer wakeup does NOT trigger rebuild

## Priority and Conflicts

### Image Source Priority

If both SD card and Google Drive are enabled:
1. SD card takes precedence
2. Google Drive is ignored
3. Warning logged about conflicting configuration

### Recommended Configurations

**Offline Mode** (SD card only):
```json
{
  "sd_card_config": {
    "enabled": true,
    "images_directory": "/images"
  },
  "google_drive_config": {
    "enabled": false
  }
}
```

**Cloud Mode** (Google Drive only):
```json
{
  "sd_card_config": {
    "enabled": false
  },
  "google_drive_config": {
    "enabled": true,
    // ... Google Drive settings
  }
}
```

## WiFi Requirements

- **SD Card Mode**: No WiFi required for image display
- **Optional WiFi**: Only needed for NTP time synchronization
- **Offline Operation**: Fully functional without network connection

## Advantages of SD Card Mode

1. **No Internet Required**: Complete offline operation
2. **Faster Image Access**: No download delays
3. **Predictable Performance**: No network variability
4. **Lower Power Usage**: No WiFi radio usage
5. **Privacy**: Images stay local
6. **Simple Setup**: No API keys or authentication

## Limitations

1. **Manual Updates**: Images must be manually copied to SD card
2. **Storage Capacity**: Limited by SD card size
3. **No Remote Management**: Cannot update images remotely
4. **Single Directory**: All images must be in one directory

## Troubleshooting

### Images Not Displaying

1. Check that `sd_card_config.enabled` is `true`
2. Verify images are in the configured directory
3. Ensure images are in binary (.bin) format
4. Check SD card is properly formatted (FAT32)

### TOC Not Working

1. Delete `sd_toc_data.txt` and `sd_toc_meta.txt` to force rebuild
2. Check SD card has write permissions
3. Verify sufficient free space on SD card
4. Enable debug logging to see TOC operations

### Slow Performance

1. Enable `use_toc_cache` if disabled
2. Check SD card speed (Class 10 or better recommended)
3. Reduce number of files in directory if excessive (>1000)

## Migration from Google Drive

To switch from Google Drive to SD card mode:

1. Process your images using the Rust processor or apps
2. Copy all .bin files to `/images` on SD card
3. Update `config.json` as shown above
4. Restart the photo frame

The cached Google Drive images in `/gdrive` can be deleted to free space.