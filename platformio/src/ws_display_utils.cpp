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

#include "ws_display_utils.h"
#include "config.h"
#include "main_common.h"
#include "ws_utils.h"
#include <Arduino.h>
#include <qrcode.h>
#include FONT_HEADER

namespace photo_frame {
namespace ws_display_utils {

photo_frame_error_t
loadCurrentOrDefaultImage(photo_frame::littlefs_manager::LittleFsManager& littleFs,
                          photo_frame::DisplayManager& display) {

    log_i("[WS-Display] Attempting to load %s from LittleFS", WS_CURRENT_IMAGE_FILENAME);

    // Try loading current image first
    photo_frame::binary_utils::PFR1BinaryFile wrapper(display.getWidth(), display.getHeight());
    photo_frame_error_t error =
        photo_frame::ws_utils::loadLittleFsFile(WS_CURRENT_IMAGE_FILENAME, littleFs, wrapper);

    if (error != photo_frame::error_type::None) {
        log_w("[WS-Display] %s not found, trying default image", WS_CURRENT_IMAGE_FILENAME);

        // Fallback to default image
        error =
            photo_frame::ws_utils::loadLittleFsFile(WS_DEFAULT_IMAGE_FILENAME, littleFs, wrapper);

        if (error != photo_frame::error_type::None) {
            log_e("[WS-Display] %s not found either", WS_DEFAULT_IMAGE_FILENAME);
            return error;
        }

        log_d("[WS-Display] Loaded default image successfully");
        log_d("[WS-Display] Successfully loaded %s (PFR1) width=%u height=%u rotation=%u",
              WS_DEFAULT_IMAGE_FILENAME,
              wrapper.header.width,
              wrapper.header.height,
              wrapper.header.rotation);
    } else {
        log_d("[WS-Display] Loaded current image successfully");
        log_d("[WS-Display] Successfully loaded %s (PFR1) width=%u height=%u rotation=%u",
              WS_CURRENT_IMAGE_FILENAME,
              wrapper.header.width,
              wrapper.header.height,
              wrapper.header.rotation);
    }

    // Copy payload into display buffer
    memcpy(display.getBuffer(), wrapper.getPayload(), wrapper.header.payload_len);

    // Apply rotation from header (0-3)
    display.setRotation(wrapper.header.rotation % 4);
    return photo_frame::error_type::None;
}

void drawConnectionInfoBox(photo_frame::DisplayManager& display,
                           const std::string& ssid,
                           const std::string& ipAddress,
                           const std::string& wsUrl) {

    log_i("[WS-Display] Drawing connection info box");

    // Get display canvas
    GFXcanvas8& canvas = display.getCanvas();

    // Define box dimensions and position (bottom-left corner)
    const int16_t boxWidth  = 250;
    const int16_t boxHeight = 200;
    const int16_t boxX      = 10;
    const int16_t boxY      = canvas.height() - boxHeight - 10;

    // Draw white background box
    canvas.fillRect(boxX, boxY, boxWidth, boxHeight, DISPLAY_COLOR_WHITE);
    canvas.drawRect(boxX, boxY, boxWidth, boxHeight, DISPLAY_COLOR_BLACK);

    // Draw SSID at the top (centered)
    canvas.setFont(&FONT_14pt8b);
    canvas.setTextColor(DISPLAY_COLOR_BLACK);
    int16_t ssidX, ssidY;
    uint16_t ssidW, ssidH;
    canvas.getTextBounds(ssid.c_str(), 0, 0, &ssidX, &ssidY, &ssidW, &ssidH);
    int16_t ssidCenterX = boxX + (boxWidth - ssidW) / 2;
    canvas.setCursor(ssidCenterX, boxY + 34);
    canvas.print(ssid.c_str());

    // Generate and draw QR code
    const int qrSize = 100;
    const int qrX    = boxX + (boxWidth - qrSize) / 2;
    const int qrY    = boxY + 56;

    // Create QR code
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, wsUrl.c_str());

    // Draw QR code on canvas
    const int moduleSize = qrSize / qrcode.size;
    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                canvas.fillRect(qrX + x * moduleSize,
                                qrY + y * moduleSize,
                                moduleSize,
                                moduleSize,
                                DISPLAY_COLOR_BLACK);
            }
        }
    }

    // Draw IP address at the bottom (centered and larger)
    canvas.setFont(&FONT_10pt8b);
    int16_t ipX, ipY;
    uint16_t ipW, ipH;
    canvas.getTextBounds(wsUrl.c_str(), 0, 0, &ipX, &ipY, &ipW, &ipH);
    int16_t ipCenterX = boxX + (boxWidth - ipW) / 2;
    canvas.setCursor(ipCenterX, boxY + boxHeight - 24);
    canvas.print(wsUrl.c_str());

    log_i("[WS-Display] Connection info box drawn successfully");
}

} // namespace ws_display_utils
} // namespace photo_frame
