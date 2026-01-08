# Dynamic Configuration TODO

## Overview
Implementing dynamic configuration for image sources and display orientation from config.json instead of compile-time constants.

## Configuration Changes Added to config.json

### 1. Google Drive Config - Added "enabled" field
```json
"google_drive_config": {
  "enabled": false,  // NEW: Controls whether to use Google Drive
  ...
}
```

### 2. SD Card Config - NEW SECTION
```json
"sd_card_config": {
  "enabled": true,
  "images_directory": "/6c/portrait/bin"  // Directory on SD card for images
}
```

### 3. Board Config - Added "portrait_mode" field
```json
"board_config": {
  "portrait_mode": true,  // NEW: Dynamic display orientation (replaces compile-time constant)
  ...
}
```

## Implementation Tasks

### ✅ Completed
- [x] Analyzed new config.json structure

### 📝 TODO

#### 1. Update Config Structures (unified_config.h)
- [ ] Add `bool enabled` to GoogleDriveConfig struct
- [ ] Create new SDCardConfig struct with:
  - `bool enabled`
  - `String images_directory`
- [ ] Add `bool portrait_mode` to BoardConfig struct
- [ ] Update UnifiedConfig struct to include SDCardConfig

#### 2. Update Config Parser (unified_config.cpp)
- [ ] Parse `google_drive_config.enabled` field
- [ ] Parse new `sd_card_config` section
- [ ] Parse `board_config.portrait_mode` field
- [ ] Set defaults for backward compatibility

#### 3. Implement Config Validation
- [ ] Validate at least one source is enabled (Google Drive OR SD Card)
- [ ] Return error if both are disabled
- [ ] Return error if SD card is enabled but directory is empty/invalid
- [ ] Add validation function in unified_config.cpp

#### 4. Update Main Logic (main.cpp)
- [ ] Check google_drive_config.enabled before Google Drive operations
- [ ] Implement SD card-only mode when Google Drive is disabled
- [ ] Use sd_card_config.images_directory for local image source
- [ ] Add logic to list and cycle through local images

#### 5. Replace Compile-Time Portrait Mode
- [ ] Remove PORTRAIT_MODE constant from config.h/board configs
- [ ] Use config.board_config.portrait_mode instead
- [ ] Update all references to PORTRAIT_MODE constant
- [ ] Update display initialization to use dynamic value

#### 6. Update Error Handling
- [ ] Add new error codes for invalid config combinations
- [ ] Display appropriate error messages on screen
- [ ] Log detailed error information

#### 7. Test Different Configurations
- [ ] Test Google Drive only (GD enabled, SD disabled)
- [ ] Test SD card only (GD disabled, SD enabled)
- [ ] Test both enabled (should work, prefer Google Drive)
- [ ] Test both disabled (should show error)
- [ ] Test portrait_mode true/false

## Files to Modify

1. **include/unified_config.h** - Add new struct fields
2. **src/unified_config.cpp** - Parse new fields and validate
3. **src/main.cpp** - Implement source selection logic
4. **src/sd_card.cpp** - Add function to list images from directory
5. **include/config.h** - Remove PORTRAIT_MODE constant
6. **include/config/*.h** - Remove PORTRAIT_MODE from board configs
7. **src/renderer.cpp** - Use dynamic portrait_mode
8. **src/errors.cpp** - Add new error codes

## Validation Rules

1. **At least one source must be enabled:**
   - If `google_drive_config.enabled == false` AND `sd_card_config.enabled == false` → ERROR
   - If `google_drive_config.enabled == false` AND `sd_card_config` is missing → ERROR

2. **SD Card validation:**
   - If `sd_card_config.enabled == true` but `images_directory` is empty → ERROR
   - Directory should exist on SD card (check at runtime)

3. **Portrait mode:**
   - Must be a boolean value
   - Defaults to false if not specified

## Example Usage Scenarios

### Scenario 1: SD Card Only
```json
"google_drive_config": { "enabled": false, ... },
"sd_card_config": { "enabled": true, "images_directory": "/6c/portrait/bin" }
```
→ App reads images from `/6c/portrait/bin` on SD card

### Scenario 2: Google Drive Only
```json
"google_drive_config": { "enabled": true, ... },
"sd_card_config": { "enabled": false }
```
→ App uses Google Drive as before

### Scenario 3: Invalid Config
```json
"google_drive_config": { "enabled": false, ... },
"sd_card_config": { "enabled": false }
```
→ ERROR: No image source enabled

## Notes
- Backward compatibility: If fields are missing, use sensible defaults
- Performance: SD card mode should be faster (no network delays)
- Power consumption: SD card mode uses less power (no WiFi needed for images)