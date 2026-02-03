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

#ifndef WS_UTILS_H
#define WS_UTILS_H

#include "battery.h"
#include "binary_utils.h"
#include "display_manager.h"
#include "errors.h"
#include "littlefs_manager.h"
#include <Arduino.h>

namespace photo_frame {
namespace ws_utils {
/**
 * @brief Display battery critical error and sleep
 *
 * Shows error on display and enters deep sleep.
 *
 * @param battery_info Battery information to display
 * @param wakeup_reason Current wakeup reason
 * @param display_rotation Current display rotation
 */
void handleCriticalBattery(const battery_info_t& battery_info,
                           esp_sleep_wakeup_cause_t wakeup_reason,
                           uint8_t display_rotation);

/**
 * @brief Load and display a PFR1 image file from LittleFS
 * @param filename Name of the file to load (e.g., "current.pfr1")
 * @param littleFs LittleFS manager instance
 * @param wrapper PFR1 binary file wrapper
 * @return Error code (None if successful)
 */
photo_frame::photo_frame_error_t
loadLittleFsFile(const char* filename,
                 photo_frame::littlefs_manager::LittleFsManager& littleFs,
                 photo_frame::binary_utils::PFR1BinaryFile& wrapper);

} // namespace ws_utils
} // namespace photo_frame

#endif // WS_UTILS_H