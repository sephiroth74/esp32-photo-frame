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

#ifndef WS_DISPLAY_UTILS_H
#define WS_DISPLAY_UTILS_H

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
 * @return Error code (None if successful)
 */
photo_frame_error_t
loadCurrentOrDefaultImage(photo_frame::littlefs_manager::LittleFsManager& littleFs,
                          photo_frame::DisplayManager& display);

/**
 * @brief Draw connection info box with AP name, QR code, and IP address
 * @param display Display manager instance
 * @param ssid AP SSID (e.g., "PhotoFrame-A1B2")
 * @param ipAddress IP address (e.g., "192.168.4.1")
 * @param wsUrl WebSocket URL for QR code (e.g., "ws://192.168.4.1")
 */
void drawConnectionInfoBox(photo_frame::DisplayManager& display,
                           const std::string& ssid,
                           const std::string& ipAddress,
                           const std::string& wsUrl);

} // namespace ws_display_utils
} // namespace photo_frame

#endif // WS_DISPLAY_UTILS_H
