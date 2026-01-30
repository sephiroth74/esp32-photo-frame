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

#ifndef COMMON_MAIN_H
#define COMMON_MAIN_H

#include "battery.h"
#include "binary_utils.h"
#include "config.h"
#include "display_manager.h"
#include "errors.h"
#include "google_drive.h"
#include <Arduino.h>

// ============================================================================
// REFRESH DELAY STRUCTURE
// ============================================================================
typedef struct {
    long refresh_seconds;
    uint64_t refresh_microseconds;
} refresh_delay_t;

// ============================================================================
// GLOBAL OBJECTS (shared across main files)
// ============================================================================
// Note: DisplayManager is now a singleton - use DisplayManager::getInstance()
extern uint8_t display_rotation; // 0-3 rotation applied to display/canvas
extern unsigned long startupTime;
extern photo_frame::BatteryReader battery_reader;

// ============================================================================
// COMMON HARDWARE INITIALIZATION
// ============================================================================

/**
 * @brief Initialize hardware components (Serial, RGB LED, board utilities)
 * @return true if initialization successful, false otherwise
 */
bool initialize_hardware();

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
bool init_image_buffer();

/**
 * @brief Initialize the display hardware (Phase 2)
 *
 * Initializes the display hardware and SPI. Must be called after SD card
 * operations are complete to avoid SPI conflicts.
 *
 * @note Must call init_image_buffer() first
 * @return true if display initialization successful, false if failed
 */
bool init_display_hardware();

/**
 * @brief Cleanup the image buffer
 *
 * The ImageBuffer class automatically handles cleanup through its destructor,
 * but this function can be called explicitly if needed.
 */
void cleanup_image_buffer();

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

/**
 * @brief Read and validate battery level, handle critical battery states
 * @param battery_info Reference to store battery information
 * @param wakeup_reason Current wakeup reason for power management decisions
 * @return Error state after battery check
 */
photo_frame::photo_frame_error_t setup_battery_and_power(photo_frame::battery_info_t& battery_info,
                                                         esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Calculate the wakeup delay based on battery level and current time
 *
 * @param battery_info The battery information structure
 * @param now The current time
 * @return The calculated wakeup delay
 */
refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now);

/**
 * @brief Handle final cleanup and prepare for deep sleep
 * @param battery_info Battery information for sleep calculations
 * @param now Current DateTime for sleep timing
 * @param wakeup_reason Wakeup reason for sleep decisions
 * @param refresh_delay Pre-calculated refresh delay to avoid re-reading potentiometer
 */
void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
                              DateTime& now,
                              esp_sleep_wakeup_cause_t wakeup_reason,
                              const refresh_delay_t& refresh_delay);

// ============================================================================
// IMAGE RENDERING
// ============================================================================

/**
 * @brief Handle image rendering with full/partial display modes and error handling
 * @param file The validated image file to render (temporary file in LittleFS)
 * @param original_filename Original filename from SD card (used for format detection and error
 * reporting)
 * @param current_error Current error state
 * @param now Current DateTime for info display
 * @param refresh_delay Refresh delay info
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param drive Google Drive instance
 * @param battery_info Battery information
 * @return Updated error state after rendering attempt
 */
photo_frame::photo_frame_error_t
render_image(const photo_frame::binary_utils::PFR1BinaryFile& image_file,
             const char* original_filename,
             photo_frame::photo_frame_error_t current_error,
             const DateTime& now,
             const refresh_delay_t& refresh_delay,
             uint32_t image_index,
             uint32_t total_files,
             photo_frame::GoogleDrive& drive,
             const photo_frame::battery_info_t& battery_info);

#endif // COMMON_MAIN_H
