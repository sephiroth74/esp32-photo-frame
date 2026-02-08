// ESP32 Photo Frame
// Copyright (C) 2025 Alessandro Crugnola
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// Include display library headers to get EPD_WIDTH and EPD_HEIGHT
// The pins are taken from config.h (EPD_BUSY_PIN, EPD_RST_PIN, EPD_DC_PIN, EPD_CS_PIN)
#ifdef DISP_6C
#include <Display_EPD_GDEP073E01_W21.h>
#else
#include <Display_EPD_GDEY075T7_W21.h>
#endif

/**
 * PFR1 Format Constants
 *
 * Centralized configuration for PFR1 binary format:
 * Header (21 bytes) + Payload (width * height) + CRC32 (4 bytes)
 *
 * Display dimensions are automatically imported from the display library
 * (EPD_WIDTH and EPD_HEIGHT), ensuring consistency across the entire project.
 */

// Standard display dimensions from display library
// EPD_WIDTH and EPD_HEIGHT are defined in the display-specific headers:
// - GDEY075T7 (B/W): 800x480
// - GDEP073E01 (6C): 800x480
#define PFR1_DEFAULT_DISPLAY_WIDTH  EPD_WIDTH
#define PFR1_DEFAULT_DISPLAY_HEIGHT EPD_HEIGHT

// PFR1 format structure sizes
#define PFR1_HEADER_SIZE 21
#define PFR1_CRC32_SIZE  4
#define PFR1_MAGIC       0x50465231 // 'PFR1' in little-endian
#define PFR1_VERSION     1

/**
 * Calculate maximum image buffer size for a given display dimension
 * Used for both binary_utils and BT protocol validation
 *
 * Formula: header_size + (width * height) + crc32_size
 * Example: 21 + (800 * 480) + 4 = 384,025 bytes (~375 KB)
 */
#define PFR1_MAX_IMAGE_SIZE_FOR(width, height)                                                     \
    (PFR1_HEADER_SIZE + ((uint32_t)(width) * (uint32_t)(height)) + PFR1_CRC32_SIZE)

/**
 * Default maximum image size (for 800x480 displays)
 * = 21 + (800 * 480) + 4 = 384,025 bytes
 */
#define PFR1_MAX_IMAGE_SIZE_DEFAULT                                                                \
    PFR1_MAX_IMAGE_SIZE_FOR(PFR1_DEFAULT_DISPLAY_WIDTH, PFR1_DEFAULT_DISPLAY_HEIGHT)

/**
 * Absolute maximum image size (safety limit for BT transfers)
 * This should accommodate any reasonable display size
 * Set to 1MB to allow flexibility for different configurations
 */
#define PFR1_MAX_IMAGE_SIZE_ABSOLUTE                                                               \
    PFR1_MAX_IMAGE_SIZE_DEFAULT + PFR1_MAX_IMAGE_SIZE_DEFAULT // 768,050 bytes (~750 KB)
                                                              /**
                                                               * ============================================================================
                                                               * USAGE GUIDE
                                                               * ============================================================================
                                                               *
                                                               * This file centralizes PFR1 format configuration to ensure consistency
                                                               *
                                                               * . For Binary Utilities (binary_utils.cpp):
                                                               *    buffer_size = PFR1_HEADER_SIZE + (width * height) + PFR1_CRC32_SIZE
                                                               *
                                                               * . To add a new display dimension:
                                                               *    - Update PFR1_DEFAULT_DISPLAY_WIDTH and PFR1_DEFAULT_DISPLAY_HEIGHT
                                                               *    - PFR1_MAX_IMAGE_SIZE_DEFAULT will auto-calculate
                                                               *    - All code using the macros will automatically adapt
                                                               *
                                                               * . Calculate size for custom dimensions:
                                                               *    size = PFR1_MAX_IMAGE_SIZE_FOR(custom_width, custom_height)
                                                               *
                                                               * Note: The macro uses (uint32_t) casts to prevent integer overflow
                                                               * when multiplying width * height for large displays.
                                                               * ============================================================================
                                                               */