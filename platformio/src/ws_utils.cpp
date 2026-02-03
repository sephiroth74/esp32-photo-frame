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

#ifdef ENABLE_WEBSERVER_DATAPROVIDER

#include "ws_utils.h"
#include "binary_utils.h"
#include "board_util.h"
#include "bt_protocol.h"
#include "config.h"
#include "display_manager.h"
#include "esp32/spiram.h"
#include "littlefs_manager.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include FONT_HEADER

namespace photo_frame {
namespace ws_utils {

void handleCriticalBattery(const battery_info_t& battery_info,
                           esp_sleep_wakeup_cause_t wakeup_reason,
                           uint8_t display_rotation) {
    log_e("[BT] Handling critical battery state: %.1f%%", battery_info.percent);

    RGB_SET_STATE(BATTERY_LOW);

    // Use singleton DisplayManager instance
    auto& display = photo_frame::DisplayManager::getInstance();

    if (!display.initBuffer(true)) {
        log_e("[BT] Failed to init buffer for error display");
        board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    board_utils::display_power_on();

    if (!display.initDisplay()) {
        log_e("[BT] Failed to init display for error display");
        board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0);
        return;
    }

    delay(300);

    // Draw battery critical error
    display.setRotation(display_rotation);
    display.clear(DISPLAY_COLOR_WHITE);
    display.drawError(error_type::BatteryLevelCritical, nullptr);
    display.render();

    delay(2000);

    // Power off and sleep indefinitely (only wake via GPIO1)
    display.powerOff();
    board_utils::display_power_off();

    log_i("[BT] Battery critical - entering indefinite sleep (wake via GPIO1 only)");
    board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_EXT0, 0); // 0 = indefinite sleep
}

photo_frame::photo_frame_error_t
loadLittleFsFile(const char* filename,
                 photo_frame::littlefs_manager::LittleFsManager& littleFs,
                 photo_frame::binary_utils::PFR1BinaryFile& wrapper) {
    log_i("[WS-Utils] Loading %s image from LittleFS", filename);

    // Try to open file
    File file = littleFs.open_file(filename, "r");
    if (!file) {
        return photo_frame::error_type::LittleFsFileNotFound;
    }

    // Load and validate file into wrapper
    auto validationError = photo_frame::binary_utils::validatePFR1File(file, wrapper);
    file.close();

    if (validationError != photo_frame::error_type::None) {
        log_e("[WS-Utils] %s PFR1 validation failed: %s", filename, validationError.message);
        return validationError;
    }
    return photo_frame::error_type::None;
}

} // namespace ws_utils
} // namespace photo_frame

#endif // ENABLE_WEBSERVER_DATAPROVIDER