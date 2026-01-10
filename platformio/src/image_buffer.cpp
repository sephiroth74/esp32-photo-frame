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

#include "image_buffer.h"
#include <esp_heap_caps.h>

namespace photo_frame {

ImageBuffer::ImageBuffer()
    : buffer_(nullptr)
    , canvas_(nullptr)
    , bufferSize_(0)
    , width_(0)
    , height_(0)
    , inPsram_(false)
{
}

ImageBuffer::~ImageBuffer()
{
    release();
}

bool ImageBuffer::init(uint16_t width, uint16_t height, bool preferPsram)
{
    // Release any existing buffer
    release();

    width_ = width;
    height_ = height;
    bufferSize_ = (size_t)width * height;

    log_i("Initializing image buffer (%ux%u = %u bytes)...", width, height, bufferSize_);

    // Try to allocate in PSRAM if requested and available
    if (preferPsram) {
#if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
        buffer_ = (uint8_t*)heap_caps_malloc(bufferSize_, MALLOC_CAP_SPIRAM);
        if (buffer_) {
            inPsram_ = true;
            log_i("Successfully allocated %u bytes in PSRAM", bufferSize_);
            log_d("PSRAM free after allocation: %u bytes", ESP.getFreePsram());
        } else {
            log_w("Failed to allocate in PSRAM, falling back to regular heap");
        }
#endif
    }

    // Fall back to regular heap if PSRAM allocation failed or not requested
    if (!buffer_) {
        buffer_ = (uint8_t*)malloc(bufferSize_);
        inPsram_ = false;
        if (buffer_) {
            log_i("Allocated %u bytes in regular heap", bufferSize_);
        }
    }

    if (!buffer_) {
        log_e("CRITICAL: Failed to allocate image buffer!");
        log_e("Required: %u bytes, Free heap: %u bytes", bufferSize_, ESP.getFreeHeap());
        return false;
    }

    log_d("Image buffer allocated at address: %p", buffer_);

    // Create canvas that will use our buffer
    canvas_ = new GFXcanvas8(width_, height_, false);  // false = don't allocate internal buffer
    if (!canvas_) {
        log_e("Failed to create canvas!");
        free(buffer_);
        buffer_ = nullptr;
        return false;
    }

    // Link canvas to our buffer
    linkCanvasToBuffer();

    // Clear the buffer to white
    clear();

    return true;
}

void ImageBuffer::linkCanvasToBuffer()
{
    if (!canvas_ || !buffer_) {
        return;
    }

    // Hack to access the internal buffer pointer of GFXcanvas8
    // The buffer pointer is the first member after the base class
    uint8_t** canvasBufferPtr = (uint8_t**)((uint8_t*)canvas_ + sizeof(Adafruit_GFX));
    *canvasBufferPtr = buffer_;

    log_d("Canvas linked to buffer at %p", buffer_);
}

void ImageBuffer::clear(uint8_t color)
{
    if (buffer_) {
        memset(buffer_, color, bufferSize_);
    }
}

void ImageBuffer::release()
{
    if (canvas_) {
        delete canvas_;
        canvas_ = nullptr;
    }

    if (buffer_) {
        log_d("Releasing image buffer at %p (%u bytes, %s)",
              buffer_, bufferSize_, inPsram_ ? "PSRAM" : "heap");
        free(buffer_);
        buffer_ = nullptr;
        bufferSize_ = 0;
        width_ = 0;
        height_ = 0;
        inPsram_ = false;
    }
}

// Move constructor
ImageBuffer::ImageBuffer(ImageBuffer&& other) noexcept
    : buffer_(other.buffer_)
    , canvas_(other.canvas_)
    , bufferSize_(other.bufferSize_)
    , width_(other.width_)
    , height_(other.height_)
    , inPsram_(other.inPsram_)
{
    // Clear the other object
    other.buffer_ = nullptr;
    other.canvas_ = nullptr;
    other.bufferSize_ = 0;
    other.width_ = 0;
    other.height_ = 0;
    other.inPsram_ = false;
}

// Move assignment operator
ImageBuffer& ImageBuffer::operator=(ImageBuffer&& other) noexcept
{
    if (this != &other) {
        // Release our current resources
        release();

        // Take ownership of other's resources
        buffer_ = other.buffer_;
        canvas_ = other.canvas_;
        bufferSize_ = other.bufferSize_;
        width_ = other.width_;
        height_ = other.height_;
        inPsram_ = other.inPsram_;

        // Clear the other object
        other.buffer_ = nullptr;
        other.canvas_ = nullptr;
        other.bufferSize_ = 0;
        other.width_ = 0;
        other.height_ = 0;
        other.inPsram_ = false;
    }
    return *this;
}

} // namespace photo_frame