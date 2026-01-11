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

#include "display_manager.h"
#include "canvas_renderer.h"
#include "config.h"
#include "display_driver_6c.h"
#include "display_driver_bw.h"
#include "renderer.h"

namespace photo_frame {

DisplayManager::DisplayManager() :
    displayDriver_(nullptr) // unique_ptr starts as nullptr
    ,
    initialized_(false),
    rotation_(0) {
    log_d("DisplayManager constructor");
}

DisplayManager::~DisplayManager() {
    log_d("DisplayManager destructor");
    release();
}

bool DisplayManager::initBuffer(bool preferPsram) {
    log_i("Initializing DisplayManager buffer (Phase 1)...");

    // Release any existing resources
    release();

    // Initialize the image buffer with display dimensions
    if (!imageBuffer_.init(EPD_WIDTH, EPD_HEIGHT, preferPsram)) {
        log_e("Failed to initialize image buffer");
        return false;
    }

    log_i("Image buffer initialized (%ux%u = %u bytes, %s)",
          imageBuffer_.getWidth(),
          imageBuffer_.getHeight(),
          imageBuffer_.getSize(),
          imageBuffer_.isInPsram() ? "PSRAM" : "heap");

    // Set default rotation (landscape) - for buffer operations
    setRotation(0);

    log_i("DisplayManager buffer initialized successfully (Phase 1 complete)");
    return true;
}

bool DisplayManager::initDisplay() {
    log_i("Initializing DisplayManager display hardware (Phase 2)...");

    if (!imageBuffer_.isInitialized()) {
        log_e("Buffer not initialized! Call initBuffer() first");
        return false;
    }

    if (displayDriver_) {
        log_w("Display driver already initialized");
        return true;
    }

    // Create and initialize the display driver
    displayDriver_ = createDisplayDriver();
    if (!displayDriver_) {
        log_e("Failed to create display driver");
        return false;
    }

    if (!displayDriver_->init()) {
        log_e("Failed to initialize display driver");
        displayDriver_.reset(); // Smart pointer automatically deletes
        return false;
    }

    log_i("Display driver initialized");

    initialized_ = true;
    log_i("DisplayManager fully initialized (Phase 2 complete)");

    return true;
}

std::unique_ptr<DisplayDriver> DisplayManager::createDisplayDriver() {
#ifdef DISP_6C
    log_i("Creating 6-color display driver");
    return std::unique_ptr<DisplayDriver>(new DisplayDriver6C(
        EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN));
#else
    log_i("Creating B/W display driver");
    return std::unique_ptr<DisplayDriver>(new DisplayDriverBW(
        EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN));
#endif
}

void DisplayManager::setRotation(uint8_t rotation) {
    if (rotation > 3) {
        log_w("Invalid rotation %u, using 0", rotation);
        rotation = 0;
    }

    rotation_ = rotation;

    if (imageBuffer_.isInitialized()) {
        imageBuffer_.getCanvas().setRotation(rotation);
        log_d("Canvas rotation set to %u", rotation);
    }
}

uint8_t DisplayManager::getRotation() const {
    if (imageBuffer_.isInitialized()) {
        return imageBuffer_.getCanvas().getRotation();
    }
    return rotation_;
}

bool DisplayManager::isPortraitMode() const {
    uint8_t rot = getRotation();
    return (rot == 1 || rot == 3);
}

uint16_t DisplayManager::getWidth() const {
    if (!imageBuffer_.isInitialized()) {
        return isPortraitMode() ? EPD_HEIGHT : EPD_WIDTH;
    }
    return imageBuffer_.getCanvas().width();
}

uint16_t DisplayManager::getHeight() const {
    if (!imageBuffer_.isInitialized()) {
        return isPortraitMode() ? EPD_WIDTH : EPD_HEIGHT;
    }
    return imageBuffer_.getCanvas().height();
}

void DisplayManager::clear(uint8_t color) {
    if (imageBuffer_.isInitialized()) {
        imageBuffer_.clear(color);
    }
}

bool DisplayManager::fillBuffer(const uint8_t* imageData, size_t size) {
    if (!initialized_) {
        log_e("DisplayManager not initialized");
        return false;
    }

    if (size != imageBuffer_.getSize()) {
        log_e("Image size mismatch: expected %u, got %u", imageBuffer_.getSize(), size);
        return false;
    }

    memcpy(imageBuffer_.getBuffer(), imageData, size);
    return true;
}

void DisplayManager::drawOverlay() {
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawOverlay(imageBuffer_.getCanvas());
}

void DisplayManager::drawLastUpdate(const DateTime& lastUpdate, long refresh_seconds) {
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawLastUpdate(imageBuffer_.getCanvas(), lastUpdate, refresh_seconds);
}

void DisplayManager::drawBatteryStatus(battery_info_t battery_info) {
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawBatteryStatus(imageBuffer_.getCanvas(), battery_info);
}

void DisplayManager::drawImageInfo(uint32_t index,
                                   uint32_t total_images,
                                   image_source_t image_source) {
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawImageInfo(imageBuffer_.getCanvas(), index, total_images, image_source);
}

void DisplayManager::drawError(photo_frame_error_t error, const char* filename) {
    log_w("draw_error. code=%d, category%d, filename=%s",
          error.code,
          error.category,
          filename ? filename : "N/A");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized()) {
        log_e("Cannot draw error - image buffer not initialized");
        return;
    }
    photo_frame::drawError(imageBuffer_.getCanvas(), error, filename);
}

void DisplayManager::drawErrorWithDetails(const String& errMsgLn1,
                                          const String& errMsgLn2,
                                          const char* filename,
                                          uint16_t errorCode) {
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawErrorWithDetails(
        imageBuffer_.getCanvas(), errMsgLn1, errMsgLn2, filename, errorCode);
}

bool DisplayManager::render() {
    if (!initialized_ || !displayDriver_) {
        log_e("DisplayManager not initialized");
        return false;
    }

    return displayDriver_->picDisplay(imageBuffer_.getBuffer());
}

void DisplayManager::sleep() {
    if (displayDriver_) {
        displayDriver_->sleep();
    }
}

void DisplayManager::powerOff() {
    if (displayDriver_) {
        displayDriver_->power_off();
    }
}

void DisplayManager::hibernate() {
    if (displayDriver_) {
        displayDriver_->hibernate();
    }
}

void DisplayManager::refresh(bool partial_update) {
    if (displayDriver_) {
        displayDriver_->refresh(partial_update);
    }
}

bool DisplayManager::hasPartialUpdate() const {
    if (displayDriver_) {
        return displayDriver_->has_partial_update();
    }
    return false;
}

bool DisplayManager::hasFastPartialUpdate() const {
    if (displayDriver_) {
        return displayDriver_->has_fast_partial_update();
    }
    return false;
}

bool DisplayManager::hasColor() const {
    if (displayDriver_) {
        return displayDriver_->has_color();
    }
#ifdef DISP_6C
    return true;
#else
    return false;
#endif
}

void DisplayManager::release() {
    log_d("Releasing DisplayManager resources");

    // Smart pointer automatically handles deletion
    displayDriver_.reset();

    imageBuffer_.release();
    initialized_ = false;
}

} // namespace photo_frame