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

#ifndef COMMON_MAIN_H
#define COMMON_MAIN_H

#include "battery_manager.h"
#include "binary_utils.h"
#include "config.h"
#include "display_manager.h"
#include "errors.h"
#include "types.h"

#ifndef ENABLE_WEBSERVER_DATAPROVIDER
#include "google_drive.h"
#endif // ENABLE_WEBSERVER_DATAPROVIDER

#include <Arduino.h>

// ============================================================================
// REFRESH DELAY STRUCTURE
// ============================================================================
typedef struct {
  long refresh_seconds;
  uint64_t get_refresh_microseconds() const { return (uint64_t)refresh_seconds * MICROSECONDS_IN_SECOND; };
} refresh_delay_t;

// ============================================================================
// GLOBAL OBJECTS (shared across main files)
// ============================================================================
// Note: DisplayManager is now a singleton - use DisplayManager::getInstance()
extern unsigned long startupTime;

// ============================================================================
// COMMON HARDWARE INITIALIZATION
// ============================================================================

/**
 * @brief Initialize hardware components (Serial, RGB LED, board utilities)
 * @return true if initialization successful, false otherwise
 */
bool initializeHardware();

/**
 * @brief Initialize the PSRAM image buffer only (Phase 1)
 *
 * Allocates the image buffer in PSRAM (if available) but does NOT initialize
 * the display hardware. This allows SD card operations to use the buffer
 * without SPI conflicts.
 *
 * @note This function must be called before any image loading operations
 * @return true if buffer allocation successful, false if failed
 */
bool initializeImageBuffer();

/**
 * @brief Initialize the display hardware (Phase 2)
 *
 * Initializes the display hardware and SPI. Must be called after SD card
 * operations are complete to avoid SPI conflicts.
 *
 * @note Must call init_image_buffer() first
 * @return true if display initialization successful, false if failed
 */
bool initializeDisplayHardware();

/**
 * @brief Cleanup the image buffer
 *
 * The ImageBuffer class automatically handles cleanup through its destructor,
 * but this function can be called explicitly if needed.
 */
void cleanupImageBuffer();

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

/**
 * @brief Read and validate battery level, handle critical battery states
 * @param batteryInfo Reference to store battery information
 * @param wakeup_reason Current wakeup reason for power management decisions
 * @return Error state after battery check
 */
photo_frame::photo_frame_error_t setupBatteryAndPower(photo_frame::BatteryInfo &batteryInfo,
                                                      esp_sleep_wakeup_cause_t wakeup_reason);

#ifndef ENABLE_WEBSERVER_DATAPROVIDER

/**
 * @brief Calculate the wakeup delay based on battery level and current time
 *
 * @param batteryInfo The battery information structure
 * @param now The current time
 * @return The calculated wakeup delay
 */
refresh_delay_t calculateWakeupDelay(photo_frame::BatteryInfo &batteryInfo, DateTime &now);

#endif // ENABLE_WEBSERVER_DATAPROVIDER

#ifndef ENABLE_WEBSERVER_DATAPROVIDER
/**
 * @brief Handle final cleanup and prepare for deep sleep
 * @param batteryInfo Battery information for sleep calculations
 * @param now Current DateTime for sleep timing
 * @param wakeup_reason Wakeup reason for sleep decisions
 * @param refresh_delay Pre-calculated refresh delay to avoid re-reading
 * potentiometer
 */
void finalizeAndEnterDeepSleep(photo_frame::BatteryInfo &batteryInfo, DateTime &now, esp_sleep_wakeup_cause_t wakeup_reason,
                               const refresh_delay_t &refresh_delay);

#endif // ENABLE_WEBSERVER_DATAPROVIDER

// ============================================================================
// IMAGE RENDERING
// ============================================================================

#if !defined(ENABLE_WEBSERVER_DATAPROVIDER)

/**
 * @brief Handle image rendering with full/partial display modes and error
 * handling
 * @param file The validated image file to render (temporary file in LittleFS)
 * @param original_filename Original filename from SD card (used for format
 * detection and error reporting)
 * @param current_error Current error state
 * @param now Current DateTime for info display
 * @param refresh_delay Refresh delay info
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param drive Google Drive instance
 * @param batteryInfo Battery information
 * @return Updated error state after rendering attempt
 */
photo_frame::photo_frame_error_t renderImage(const photo_frame::PFR1BinaryFile &image_file, const char *original_filename,
                                             photo_frame::photo_frame_error_t current_error, const DateTime &now,
                                             const refresh_delay_t &refresh_delay, uint32_t image_index, uint32_t total_files,
                                             photo_frame::GoogleDrive &drive, const photo_frame::BatteryInfo &batteryInfo);

#endif // !ENABLE_WEBSERVER_DATAPROVIDER

#endif // COMMON_MAIN_H
