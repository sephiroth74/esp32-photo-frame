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


#ifndef WS_DISPLAY_UTILS_H
#define WS_DISPLAY_UTILS_H

#ifdef ENABLE_WEBSERVER_DATAPROVIDER

#include "display_manager.h"
#include "errors.h"
#include "littlefs_manager.h"
#include <string>

namespace photo_frame {
namespace ws_display_utils {

/**
 * @brief Load and display the current or default image from LittleFS
 * @param littleFs LittleFS manager instance
 * @param display Display manager instance
 * @param filename Filename of the image to load
 * @return Error code (None if successful)
 */
photo_frame_error_t drawImageFile(photo_frame::littlefs_manager::LittleFsManager &littleFs, photo_frame::DisplayManager &display,
                                  const char *filename);

/**
 * @brief Draw connection info box with AP name, QR code, and IP address
 * @param display Display manager instance
 * @param ssid AP SSID (e.g., "PhotoFrame-A1B2")
 * @param ipAddress IP address (e.g., "192.168.4.1")
 * @param deepLinkUrl Deep link URL for QR code (e.g.,
 * "photoframe://connect?ip=192.168.4.1&ssid=MyWiFi&port=8080&v=1&d=1&w=800&h=480")
 */
void drawConnectionInfoBox(photo_frame::DisplayManager &display, const std::string &ssid, const std::string &ipAddress,
                           const std::string &deepLinkUrl);

} // namespace ws_display_utils
} // namespace photo_frame

#endif // ENABLE_WEBSERVER_DATAPROVIDER

#endif // WS_DISPLAY_UTILS_H
