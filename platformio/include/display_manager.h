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

#ifndef __DISPLAY_MANAGER_H__
#define __DISPLAY_MANAGER_H__

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <RTClib.h>
#include <memory>  // For std::unique_ptr
#include "FS.h"
#include "config.h"
#include "image_buffer.h"
#include "display_driver.h"
#include "battery.h"
#include "errors.h"
#include "google_drive.h"

// Include the appropriate display library to get EPD_WIDTH and EPD_HEIGHT
#ifdef DISP_6C
#include "../lib/GDEP073E01/Display_EPD_GDEP073E01_W21.h"
#else
#include "../lib/GDEY075T7/Display_EPD_GDEY075T7_W21.h"
#endif

namespace photo_frame {

/**
 * @brief High-level display management class that encapsulates all rendering components
 *
 * This class provides a unified interface for display operations, managing:
 * - Image buffer allocation (PSRAM/heap)
 * - Canvas for drawing operations
 * - Display driver for hardware control
 * - Rendering and overlay operations
 *
 * Usage:
 * ```cpp
 * DisplayManager display;
 * if (display.init()) {
 *     // Phase 1: Load image from SD card directly into display buffer
 *     File imageFile = sdCard.open(filename, "r");
 *     renderer::load_image_to_buffer(display.getBuffer(), imageFile, filename, DISP_WIDTH, DISP_HEIGHT);
 *     imageFile.close();
 *     sdCard.end();  // Close SD card before display operations
 *
 *     // Phase 2: Draw overlays on canvas
 *     display.drawOverlay();
 *     display.drawBatteryStatus(battery_info);
 *
 *     // Phase 3: Render to physical display
 *     display.render();
 *     display.sleep();
 * }
 * ```
 */
class DisplayManager {
public:
    /**
     * @brief Constructor
     */
    DisplayManager();

    /**
     * @brief Destructor - automatically cleans up resources
     */
    ~DisplayManager();

    /**
     * @brief Initialize buffer only (Phase 1 - for SD card operations)
     * @param preferPsram Try to allocate buffer in PSRAM if available (default: true)
     * @return true if buffer allocation successful, false otherwise
     */
    bool initBuffer(bool preferPsram = true);

    /**
     * @brief Initialize display hardware (Phase 2 - after SD card closed)
     * @return true if display initialization successful, false otherwise
     * @note Must call initBuffer() first
     */
    bool initDisplay();

    /**
     * @brief Check if buffer is initialized
     * @return true if buffer is allocated and ready to use
     */
    bool isBufferInitialized() const { return imageBuffer_.isInitialized(); }

    /**
     * @brief Check if display hardware is initialized
     * @return true if display driver is initialized and ready to use
     */
    bool isDisplayInitialized() const { return displayDriver_ != nullptr; }

    /**
     * @brief Check if display manager is fully initialized (both buffer and hardware)
     * @return true if fully initialized and ready to use
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get the canvas for drawing operations
     * @return Reference to GFXcanvas8 (will crash if not initialized!)
     */
    GFXcanvas8& getCanvas() {
        if (!imageBuffer_.isInitialized()) {
            log_e("DisplayManager not initialized! Call init() first");
        }
        return imageBuffer_.getCanvas();
    }

    /**
     * @brief Get the raw image buffer
     * @return Pointer to buffer or nullptr if not initialized
     */
    uint8_t* getBuffer() { return imageBuffer_.getBuffer(); }

    /**
     * @brief Set display rotation (portrait/landscape mode)
     * @param rotation 0=landscape, 1=portrait CCW, 2=landscape flipped, 3=portrait CW
     */
    void setRotation(uint8_t rotation);

    /**
     * @brief Get current rotation
     * @return Current rotation value (0-3)
     */
    uint8_t getRotation() const;

    /**
     * @brief Check if display is in portrait mode
     * @return true if rotation is 1 or 3 (portrait)
     */
    bool isPortraitMode() const;

    // ========== Buffer Management Functions ==========

    /**
     * @brief Clear the buffer/canvas to a solid color
     * @param color Fill color (default: 0xFF for white)
     */
    void clear(uint8_t color = 0xFF);

    /**
     * @brief Fill buffer with image data
     * @param imageData Pointer to source image data
     * @param size Size of image data in bytes
     * @return true if successful, false if size doesn't match buffer
     */
    bool fillBuffer(const uint8_t* imageData, size_t size);

    /**
     * @brief Get buffer size
     * @return Size of the image buffer in bytes
     */
    size_t getBufferSize() const { return imageBuffer_.getSize(); }

    // ========== Overlay Drawing Functions ==========

    /**
     * @brief Draw standard overlay (status bar)
     */
    void drawOverlay();

    /**
     * @brief Draw last update time
     * @param lastUpdate DateTime of last update
     * @param refresh_seconds Refresh interval in seconds
     */
    void drawLastUpdate(const DateTime& lastUpdate, long refresh_seconds = 0);

    /**
     * @brief Draw battery status
     * @param battery_info Battery information
     */
    void drawBatteryStatus(battery_info_t battery_info);

    /**
     * @brief Draw image information
     * @param index Current image index
     * @param total_images Total number of images
     * @param image_source Source of the image
     */
    void drawImageInfo(uint32_t index, uint32_t total_images, image_source_t image_source);

    /**
     * @brief Draw error message
     * @param error Error information
     */
    void drawError(photo_frame_error_t error);

    /**
     * @brief Draw error with details
     * @param errMsgLn1 First line of error
     * @param errMsgLn2 Second line of error
     * @param filename Filename that caused error
     * @param errorCode Error code
     */
    void drawErrorWithDetails(const String& errMsgLn1, const String& errMsgLn2,
                              const char* filename, uint16_t errorCode);

    // ========== Display Control Functions ==========

    /**
     * @brief Render the buffer to the display
     * @return true if successful, false otherwise
     */
    bool render();

    /**
     * @brief Put display to sleep
     */
    void sleep();

    /**
     * @brief Power off the display
     */
    void powerOff();

    /**
     * @brief Hibernate the display
     */
    void hibernate();

    /**
     * @brief Refresh the display
     * @param partial_update Use partial update if available
     */
    void refresh(bool partial_update = false);

    /**
     * @brief Check if display supports partial updates
     * @return true if partial updates supported
     */
    bool hasPartialUpdate() const;

    /**
     * @brief Check if display supports fast partial updates
     * @return true if fast partial updates supported
     */
    bool hasFastPartialUpdate() const;

    /**
     * @brief Check if display supports color
     * @return true if color display
     */
    bool hasColor() const;

    /**
     * @brief Get display width (accounting for rotation)
     * @return Width in pixels
     */
    uint16_t getWidth() const;

    /**
     * @brief Get display height (accounting for rotation)
     * @return Height in pixels
     */
    uint16_t getHeight() const;

    /**
     * @brief Get native display width (from EPD_WIDTH, regardless of rotation)
     * @return Native width in pixels
     */
    static constexpr uint16_t getNativeWidth() { return EPD_WIDTH; }

    /**
     * @brief Get native display height (from EPD_HEIGHT, regardless of rotation)
     * @return Native height in pixels
     */
    static constexpr uint16_t getNativeHeight() { return EPD_HEIGHT; }

    /**
     * @brief Release all resources
     * Can be called manually before destructor
     */
    void release();

private:
    ImageBuffer imageBuffer_;                        ///< Manages image buffer and canvas
    std::unique_ptr<DisplayDriver> displayDriver_;   ///< Hardware display driver (smart pointer)
    bool initialized_;                               ///< Initialization state
    uint8_t rotation_;                              ///< Current rotation (0-3)

    /**
     * @brief Create appropriate display driver based on configuration
     * @return unique_ptr to display driver
     */
    std::unique_ptr<DisplayDriver> createDisplayDriver();
};

} // namespace photo_frame

#endif // __DISPLAY_MANAGER_H__