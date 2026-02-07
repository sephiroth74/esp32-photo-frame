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

#ifndef __IMAGE_BUFFER_H__
#define __IMAGE_BUFFER_H__

#include <Adafruit_GFX.h>
#include <Arduino.h>

// Forward declare to avoid circular dependency
extern uint16_t DISP_WIDTH;
extern uint16_t DISP_HEIGHT;

namespace photo_frame {

/**
 * @brief RAII class for managing image buffer and associated canvas
 *
 * This class automatically allocates an image buffer (preferably in PSRAM)
 * and creates a GFXcanvas8 that points to this buffer. The buffer is
 * automatically freed when the object is destroyed.
 *
 * Usage:
 * ```cpp
 * ImageBuffer imgBuffer;
 * if (imgBuffer.init()) {
 *     uint8_t* buffer = imgBuffer.getBuffer();
 *     GFXcanvas8& canvas = imgBuffer.getCanvas();
 *     // Use buffer and canvas...
 * }
 * // Buffer automatically freed when imgBuffer goes out of scope
 * ```
 */
class ImageBuffer {
public:
    /**
     * @brief Constructor - does not allocate memory yet
     */
    ImageBuffer();

    /**
     * @brief Destructor - automatically frees allocated memory
     */
    ~ImageBuffer();

    /**
     * @brief Initialize the buffer and canvas
     * @param width Display width (default: DISP_WIDTH)
     * @param height Display height (default: DISP_HEIGHT)
     * @param preferPsram Try to allocate in PSRAM if available (default: true)
     * @return true if initialization successful, false otherwise
     */
    bool init(uint16_t width = DISP_WIDTH, uint16_t height = DISP_HEIGHT, bool preferPsram = true);

    /**
     * @brief Check if buffer is initialized
     * @return true if buffer is allocated and ready
     */
    bool isInitialized() const { return buffer_ != nullptr; }

    /**
     * @brief Get pointer to the raw image buffer
     * @return Pointer to buffer or nullptr if not initialized
     */
    uint8_t* getBuffer() { return buffer_; }
    const uint8_t* getBuffer() const { return buffer_; }

    /**
     * @brief Get reference to the canvas
     * @return Reference to GFXcanvas8 (will crash if not initialized!)
     */
    GFXcanvas8& getCanvas()
    {
        if (!canvas_) {
            log_e("Canvas not initialized! Call init() first");
        }
        return *canvas_;
    }
    const GFXcanvas8& getCanvas() const
    {
        if (!canvas_) {
            log_e("Canvas not initialized! Call init() first");
        }
        return *canvas_;
    }

    /**
     * @brief Get pointer to the canvas (can be nullptr)
     * @return Pointer to GFXcanvas8 or nullptr if not initialized
     */
    GFXcanvas8* getCanvasPtr() { return canvas_; }
    const GFXcanvas8* getCanvasPtr() const { return canvas_; }

    /**
     * @brief Get buffer size in bytes
     * @return Size of allocated buffer
     */
    size_t getSize() const { return bufferSize_; }

    /**
     * @brief Get buffer width
     * @return Width in pixels
     */
    uint16_t getWidth() const { return width_; }

    /**
     * @brief Get buffer height
     * @return Height in pixels
     */
    uint16_t getHeight() const { return height_; }

    /**
     * @brief Check if buffer was allocated in PSRAM
     * @return true if in PSRAM, false if in regular heap
     */
    bool isInPsram() const { return inPsram_; }

    /**
     * @brief Clear the buffer (fill with white)
     * @param color Fill color (default: 0xFF for white)
     */
    void clear(uint8_t color = 0xFF);

    /**
     * @brief Release the buffer and canvas
     * Can be called manually before destructor
     */
    void release();

    // Disable copy constructor and assignment operator
    ImageBuffer(const ImageBuffer&) = delete;
    ImageBuffer& operator=(const ImageBuffer&) = delete;

    // Enable move semantics (C++11)
    ImageBuffer(ImageBuffer&& other) noexcept;
    ImageBuffer& operator=(ImageBuffer&& other) noexcept;

private:
    uint8_t* buffer_; ///< Raw image buffer
    GFXcanvas8* canvas_; ///< Canvas pointing to buffer
    size_t bufferSize_; ///< Size of buffer in bytes
    uint16_t width_; ///< Width in pixels
    uint16_t height_; ///< Height in pixels
    bool inPsram_; ///< Whether buffer is in PSRAM

    /**
     * @brief Internal method to make canvas point to our buffer
     * Uses the "hack" to access internal buffer pointer
     */
    void linkCanvasToBuffer();
};

} // namespace photo_frame

#endif // __IMAGE_BUFFER_H__