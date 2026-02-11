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

#ifdef ENABLE_WEBSERVER_DATAPROVIDER

#include "ws_utils.h"
#include "binary_utils.h"
#include "board_util.h"
#include "config.h"
#include "display_manager.h"
#include "esp32/spiram.h"
#include "littlefs_manager.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include FONT_HEADER

namespace photo_frame {
namespace ws_utils {

void handleCriticalBattery(const BatteryInfo &BatteryInfo, esp_sleep_wakeup_cause_t wakeup_reason, uint8_t display_rotation) {
  log_e("[BT] Handling critical battery state: %.1f%%", BatteryInfo.percent);

  RGB_SET_STATE(BATTERY_LOW);

  // Use singleton DisplayManager instance
  auto &display = photo_frame::DisplayManager::getInstance();

  if (!display.initBuffer(true)) {
    log_e("[BT] Failed to init buffer for error display");
    board_utils::enterDeepSleep(ESP_SLEEP_WAKEUP_EXT0, 0);
    return;
  }

  board_utils::displayPowerOn();

  if (!display.initDisplay()) {
    log_e("[BT] Failed to init display for error display");
    board_utils::enterDeepSleep(ESP_SLEEP_WAKEUP_EXT0, 0);
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
  board_utils::displayPowerOff();

  log_w("[BT] Battery critical - entering indefinite sleep (wake via GPIO1 "
        "only)");
  board_utils::enterDeepSleep(ESP_SLEEP_WAKEUP_EXT0, 0); // 0 = indefinite sleep
}

photo_frame::photo_frame_error_t loadLittleFsFile(const char *filename, photo_frame::littlefs_manager::LittleFsManager &littleFs,
                                                  photo_frame::PFR1BinaryFile &wrapper) {
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