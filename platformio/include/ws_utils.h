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
     * @param BatteryInfo Battery information to display
     * @param wakeup_reason Current wakeup reason
     * @param display_rotation Current display rotation
     */
    void handleCriticalBattery(const BatteryInfo& BatteryInfo,
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
        photo_frame::PFR1BinaryFile& wrapper);

} // namespace ws_utils
} // namespace photo_frame

#endif // WS_UTILS_H