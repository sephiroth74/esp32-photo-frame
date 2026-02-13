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

#include "ws_display_utils.h"
#include "config.h"
#include "main_common.h"
#include "ws_utils.h"
#include <Arduino.h>
#include <qrcode.h>
#include FONT_HEADER

namespace photo_frame {
namespace ws_display_utils {

photo_frame_error_t drawImageFile(photo_frame::littlefs_manager::LittleFsManager &littleFs, photo_frame::DisplayManager &display,
                                  const char *filename) {

  log_i("[WS-Display] Attempting to load %s from LittleFS", filename);

  // Try loading current image first
  photo_frame::PFR1BinaryFile wrapper(display.getWidth(), display.getHeight());
  photo_frame_error_t error = photo_frame::ws_utils::loadLittleFsFile(filename, littleFs, wrapper);

  if (error != photo_frame::error_type::None) {
    log_w("[WS-Display] %s not found", filename);
    wrapper.reset();
    return error;
  } else {
    log_d("[WS-Display] Loaded current image successfully");
    log_d("[WS-Display] Successfully loaded %s (PFR1) width=%u height=%u "
          "rotation=%u",
          filename, wrapper.header.getWidth(), wrapper.header.getHeight(), wrapper.header.getRotation());
  }
  display.drawImage(wrapper); // will update rotation as well
  return photo_frame::error_type::None;
}

void drawConnectionInfoBox(photo_frame::DisplayManager &display, const std::string &ssid, const std::string &ipAddress,
                           const std::string &deepLinkUrl) {

  log_i("[WS-Display] Drawing connection info box");

  // Get display canvas
  GFXcanvas8 &canvas = display.getCanvas();

  // Define box dimensions and position (bottom-left corner)
  const int16_t boxBottom = canvas.height() - 10;
  const int16_t boxWidth = 250;
  const int16_t boxX = 10;
  const int16_t qrSize = 148;

  // Draw IP address at the bottom (centered and larger)
  canvas.setFont(&FONT_10pt8b);
  int16_t ipX, ipY;
  uint16_t ipW, ipH;
  canvas.getTextBounds(ipAddress.c_str(), 0, 0, &ipX, &ipY, &ipW, &ipH);
  int16_t ipCenterX = boxX + (boxWidth - ipW) / 2;
  int16_t ipCenterY = boxBottom - 20;
  log_d("[WS-Display] IP position at (%d,%d)", ipCenterX, ipCenterY);

  // Draw SSID at the top (over the IP address)
  canvas.setFont(&FONT_12pt8b);
  canvas.setTextColor(DISPLAY_COLOR_BLACK);
  int16_t ssidX, ssidY;
  uint16_t ssidW, ssidH;
  canvas.getTextBounds(ssid.c_str(), 0, 0, &ssidX, &ssidY, &ssidW, &ssidH);
  int16_t ssidCenterX = boxX + (boxWidth - ssidW) / 2;
  int16_t ssidCenterY = ipCenterY - ssidH - 8;

  log_d("[WS-Display] SSID position at (%d,%d)", ssidCenterX, ssidCenterY);

  // canvas.setCursor(ssidCenterX, boxY + boxHeight - 64);
  // canvas.print(ssid.c_str());

  // Generate QR code for deep link URL and place it above the SSID
  // Note: Using version 5 for larger QR code capacity needed for deep link URL
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(5)];
  qrcode_initText(&qrcode, qrcodeData, 5, ECC_LOW, deepLinkUrl.c_str());

  const int moduleSize = qrSize / qrcode.size;
  log_d("[WS-Display] QR code size: %u modules, module size: %d pixels", qrcode.size, moduleSize);

  const int qrFinalSize = moduleSize * qrcode.size;
  const int qrX = boxX + (boxWidth / 2) - (qrFinalSize / 2);
  const int qrY = ssidCenterY - qrFinalSize - 22;

  log_d("[WS-Display] QR code position at (%d,%d)", qrX, qrY);

  // Draw QR code on canvas
  log_d("[WS-Display] Generated QR code with size %u", qrcode.size);

  // now draw the box
  const int16_t boxTop = qrY - 20;
  const int16_t boxHeight = boxBottom - boxTop;
  const int16_t boxY = boxBottom - boxHeight;

  log_d("[WS-Display] Drawing connection info box at (%d,%d) size %dx%d", boxX, boxY, boxWidth, boxHeight);

  // Draw white background box
  canvas.fillRect(boxX, boxY, boxWidth, boxHeight, DISPLAY_COLOR_WHITE);
  canvas.fillRect(boxX + 2, boxY + 2, boxWidth - 4, boxHeight - 4, DISPLAY_COLOR_BLACK);
  canvas.fillRect(boxX + 4, boxY + 4, boxWidth - 8, boxHeight - 8, DISPLAY_COLOR_WHITE);
  // canvas.drawRect(boxX + 6, boxY + 6, boxWidth - 12, boxHeight - 12,
  // DISPLAY_COLOR_BLACK);

  log_d("[WS-Display] module size %d  ", moduleSize);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        canvas.fillRect(qrX + x * moduleSize, qrY + y * moduleSize, moduleSize, moduleSize, DISPLAY_COLOR_BLACK);
      }
    }
  }

  // Draw IP
  canvas.setFont(&FONT_10pt8b);
  canvas.setCursor(ipCenterX, ipCenterY);
  canvas.print(ipAddress.c_str());

  // Draw SSID
  canvas.setFont(&FONT_12pt8b);
  canvas.setCursor(ssidCenterX, ssidCenterY);
  canvas.print(ssid.c_str());

  log_i("[WS-Display] Connection info box drawn successfully");
}

} // namespace ws_display_utils
} // namespace photo_frame

#endif // ENABLE_WEBSERVER_DATAPROVIDER