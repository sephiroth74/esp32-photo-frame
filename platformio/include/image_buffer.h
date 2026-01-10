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

#ifndef __IMAGE_BUFFER_H__
#define __IMAGE_BUFFER_H__

#include <Arduino.h>
#include <Adafruit_GFX.h>

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
    GFXcanvas8& getCanvas() {
        if (!canvas_) {
            log_e("Canvas not initialized! Call init() first");
        }
        return *canvas_;
    }
    const GFXcanvas8& getCanvas() const {
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
    uint8_t* buffer_;       ///< Raw image buffer
    GFXcanvas8* canvas_;    ///< Canvas pointing to buffer
    size_t bufferSize_;     ///< Size of buffer in bytes
    uint16_t width_;        ///< Width in pixels
    uint16_t height_;       ///< Height in pixels
    bool inPsram_;          ///< Whether buffer is in PSRAM

    /**
     * @brief Internal method to make canvas point to our buffer
     * Uses the "hack" to access internal buffer pointer
     */
    void linkCanvasToBuffer();
};

} // namespace photo_frame

#endif // __IMAGE_BUFFER_H__