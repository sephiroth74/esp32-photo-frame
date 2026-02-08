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

#ifndef __DISPLAY_MANAGER_H__
#define __DISPLAY_MANAGER_H__

#include "FS.h"
#include "battery_manager.h"
#include "binary_utils.h"
#include "config.h"
#include "display_driver.h"
#include "errors.h"
#include "geometry.h"
#include "types.h"

#ifndef ENABLE_WEBSERVER_DATAPROVIDER
#include "google_drive.h"
#endif // ENABLE_WEBSERVER_DATAPROVIDER

#include "image_buffer.h"
#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <RTClib.h>
#include <assets/icons/icons.h>
#include <memory> // For std::unique_ptr

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
 *     loadImageToBuffer(display.getBuffer(), imageFile, filename, DISP_WIDTH, DISP_HEIGHT);
 *     imageFile.close();
 *     sdCard.end();  // Close SD card before display operations
 *
 *     // Phase 2: Draw overlays on canvas
 *     display.drawOverlay();
 *     display.drawBatteryStatus(BatteryInfo);
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
     * @brief Get singleton instance of DisplayManager
     * @return Reference to the global DisplayManager instance
     *
     * The singleton instance should be initialized once at startup via:
     * - initBuffer() for Phase 1 (buffer allocation)
     * - initDisplay() for Phase 2 (hardware initialization)
     *
     * All code should use getInstance() to access the display, ensuring
     * only one DisplayManager instance exists and avoiding redundant
     * initialization.
     */
    static DisplayManager& getInstance();

    /**
     * @brief Constructor (use getInstance() instead)
     * @deprecated Use getInstance() for singleton access
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
     * @brief Draw an image file directly to the display buffer
     * @param imageFile PFR1BinaryFile wrapper containing image data and metadata
     */
    void drawImage(photo_frame::PFR1BinaryFile& imageFile);

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
    void clear(uint8_t color = DISPLAY_COLOR_WHITE);

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
     * @param BatteryInfo Battery information
     */
    void drawBatteryStatus(BatteryInfo BatteryInfo);

    /**
     * @brief Draw a side message on the display
     * @param gravity Position on screen (TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
     * TOP_CENTER)
     * @param message Message text to display
     * @param xOffset Optional X offset
     * @param yOffset Optional Y offset
     */
    void drawSideMessage(gravity_t gravity,
                         const char* message,
                         int32_t xOffset = 0,
                         int32_t yOffset = 0);

    /**
     * @brief Draw a side message for an error
     * @param gravity Position on screen (TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT,
     * TOP_CENTER)
     * @param error Error information
     * @param xOffset Optional X offset
     */
    void drawSideMessageError(gravity_t gravity,
                              photo_frame::photo_frame_error_t error,
                              int32_t xOffset = 0,
                              int32_t yOffset = 0);

    /**
     * @brief Set the image source for status reporting
     * @param source The source of the current image (CLOUD, LOCAL_CACHE, BLUETOOTH)
     */
    void setImageSource(ImageSource source) { image_source_ = source; }

    /**
     * @brief Get the current image source
     * @return The image source enum value
     */
    ImageSource getImageSource() const { return image_source_; }

    /**
     * @brief Draw image information
     * @param index Current image index
     * @param total_images Total number of images
     * @param image_source Source of the image
     */
    void drawImageInfo(uint32_t index, uint32_t total_images, ImageSource image_source);

    void drawImageInfo(const String& message, ImageSource image_source);

    /**
     * @brief Draw error message
     * @param error Error information
     * @param filename Optional filename to display (nullptr if not applicable)
     */
    void drawError(photo_frame_error_t error, const char* filename = nullptr);

    /**
     * @brief Draw centered message with icon on the canvas
     * @param canvas Canvas to draw on
     * @param icon Icon to display
     * @param title Title text (optional)
     * @param message Message text (optional)
     * @param icon_size Size of the icon bitmap to use
     */
    void drawCenteredMessageWithIcon(GFXcanvas8& canvas,
                                     icon_name_t icon,
                                     const String& title,
                                     const String& message,
                                     uint16_t icon_size);

    /**
     * @brief Draw error with details
     * @param errMsgLn1 First line of error
     * @param errMsgLn2 Second line of error
     * @param filename Filename that caused error
     * @param errorCode Error code
     */
    void drawErrorWithDetails(const String& errMsgLn1,
                              const String& errMsgLn2,
                              const char* filename,
                              uint16_t errorCode);

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
    ImageBuffer imageBuffer_;                      ///< Manages image buffer and canvas
    std::unique_ptr<DisplayDriver> displayDriver_; ///< Hardware display driver (smart pointer)
    bool initialized_;                             ///< Initialization state
    uint8_t rotation_;                             ///< Current rotation (0-3)
    ImageSource image_source_;                     ///< Current image source (for status reporting)

    /**
     * @brief Create appropriate display driver based on configuration
     * @return unique_ptr to display driver
     */
    std::unique_ptr<DisplayDriver> createDisplayDriver();
};

} // namespace photo_frame

#endif // __DISPLAY_MANAGER_H__