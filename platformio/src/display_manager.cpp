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

#include "display_manager.h"
#include "canvas_renderer.h"
#include "config.h"
#include "display_driver_6c.h"

namespace photo_frame {

// Static singleton instance
static DisplayManager* g_display_instance = nullptr;

DisplayManager& DisplayManager::getInstance() {
    if (g_display_instance == nullptr) {
        g_display_instance = new DisplayManager();
        log_i("[DisplayManager] singleton created");
    }
    return *g_display_instance;
}

DisplayManager::DisplayManager() :
    displayDriver_(nullptr) // unique_ptr starts as nullptr
    ,
    initialized_(false),
    rotation_(0),
    image_source_(IMAGE_SOURCE_NONE) {
    log_d("[DisplayManager] constructor");
}

DisplayManager::~DisplayManager() {
    log_d("[DisplayManager] destructor");
    release();
}

bool DisplayManager::initBuffer(bool preferPsram) {
    log_i("[DisplayManager] Initializing buffer (Phase 1)...");

    // check if already initialized
    if (imageBuffer_.isInitialized()) {
        log_w("[DisplayManager] Image buffer already initialized");
        return true;
    }

    // Release any existing resources
    release();

    // Initialize the image buffer with display dimensions
    if (!imageBuffer_.init(EPD_WIDTH, EPD_HEIGHT, preferPsram)) {
        log_e("[DisplayManager] Failed to initialize image buffer");
        return false;
    }

    log_d("[DisplayManager] Image buffer initialized (%ux%u = %u bytes, %s)",
          imageBuffer_.getWidth(),
          imageBuffer_.getHeight(),
          imageBuffer_.getSize(),
          imageBuffer_.isInPsram() ? "PSRAM" : "heap");

    // Set default rotation (landscape) - for buffer operations
    setRotation(0);

    log_v("[DisplayManager] buffer initialized successfully (Phase 1 complete)");
    return true;
}

bool DisplayManager::initDisplay() {
    log_i("[DisplayManager] Initializing display hardware (Phase 2)...");

    if (!imageBuffer_.isInitialized()) {
        log_e("[DisplayManager] Buffer not initialized! Call initBuffer() first");
        imageBuffer_.getCanvas().setRotation(rotation_);
        return false;
    }

    if (displayDriver_) {
        log_w("[DisplayManager] Display driver already initialized");
        return true;
    }

    // Create and initialize the display driver
    displayDriver_ = createDisplayDriver();
    if (!displayDriver_) {
        log_e("[DisplayManager] Failed to create display driver");
        return false;
    }

    if (!displayDriver_->init()) {
        log_e("[DisplayManager] Failed to initialize display driver");
        displayDriver_.reset(); // Smart pointer automatically deletes
        return false;
    }

    initialized_ = true;
    log_v("[DisplayManager] Display driver initialized");
    return true;
}

std::unique_ptr<DisplayDriver> DisplayManager::createDisplayDriver() {
#ifdef DISP_6C
    log_i("[DisplayManager] Creating 6-color display driver");
    return std::unique_ptr<DisplayDriver>(new DisplayDriver6C(
        EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN));
#else
    log_i("[DisplayManager] Creating B/W display driver");
    return std::unique_ptr<DisplayDriver>(new DisplayDriverBW(
        EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN, EPD_SCK_PIN, EPD_MOSI_PIN));
#endif
}

void DisplayManager::drawImage(photo_frame::PFR1BinaryFile& imageFile) {
    log_d("[DisplayManager] Drawing image from PFR1BinaryFile wrapper");

    if (!initialized_) {
        log_e("[DisplayManager] Cannot draw image - display not initialized");
        return;
    }

    // Get image data and size from the wrapper
    const uint8_t* imageData = imageFile.getPayload();
    size_t imageSize         = imageFile.getPayloadSize();
    uint8_t rotation         = imageFile.getRotation();

    if (!imageData || imageSize == 0) {
        log_e("[DisplayManager] Invalid image data in PFR1BinaryFile");
        return;
    }

    // update the canvas rotation to match image metadata
    setRotation(rotation);

    // Fill the buffer with the image data
    if (!fillBuffer(imageData, imageSize)) {
        log_e("[DisplayManager] Failed to fill buffer with image data");
        return;
    }
}

void DisplayManager::setRotation(uint8_t rotation) {
    if (rotation > 3) {
        log_w("[DisplayManager] Invalid rotation %u, using 0", rotation);
        rotation = 0;
    }

    rotation_ = rotation;

    if (imageBuffer_.isInitialized()) {
        imageBuffer_.getCanvas().setRotation(rotation);
        log_d("[DisplayManager] Canvas rotation set to %u", rotation);
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
    log_d("[DisplayManager] Clearing buffer with color 0x%02X", color);
    if (imageBuffer_.isInitialized()) {
        imageBuffer_.clear(color);
    } else {
        log_w("[DisplayManager] Cannot clear - image buffer not initialized");
    }
}

bool DisplayManager::fillBuffer(const uint8_t* imageData, size_t size) {
    log_d("[DisplayManager] Filling buffer with image data (%u bytes)", size);

    if (!initialized_) {
        log_w("[DisplayManager] not initialized");
        return false;
    }

    if (size != imageBuffer_.getSize()) {
        log_e("[DisplayManager] Image size mismatch: expected %u, got %u",
              imageBuffer_.getSize(),
              size);
        return false;
    }

    memcpy(imageBuffer_.getBuffer(), imageData, size);
    return true;
}

void DisplayManager::drawOverlay() {
    // Only need buffer to be initialized for drawing to canvas
    log_d("[DisplayManager] Drawing overlay on canvas");
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawOverlay(imageBuffer_.getCanvas());
}

void DisplayManager::drawSideMessage(gravity_t gravity,
                                     const char* message,
                                     int32_t xOffset,
                                     int32_t yOffset) {
    log_d("[DisplayManager] Drawing side message on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawSideMessage(imageBuffer_.getCanvas(), gravity, message, xOffset, yOffset);
}

void DisplayManager::drawSideMessageError(gravity_t gravity,
                                          photo_frame_error_t error,
                                          int32_t xOffset,
                                          int32_t yOffset) {
    log_d("[DisplayManager] Drawing side message error on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;

    const char* errorMsg = error.message ? error.message : TXT_UNKNOWN_ERROR;
    photo_frame::drawSideMessageWithIcon(
        imageBuffer_.getCanvas(), gravity, icon_name::warning_icon, errorMsg, xOffset, yOffset);
}

void DisplayManager::drawLastUpdate(const DateTime& lastUpdate, long refresh_seconds) {
    log_d("[DisplayManager] Drawing last update time on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    if (!lastUpdate.isValid()) {
        log_w("Invalid last update time, skipping drawLastUpdate");
        return;
    }
    photo_frame::drawLastUpdate(imageBuffer_.getCanvas(), lastUpdate, refresh_seconds);
}

void DisplayManager::drawBatteryStatus(BatteryInfo BatteryInfo) {
    log_d("[DisplayManager] Drawing battery status on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawBatteryStatus(imageBuffer_.getCanvas(), BatteryInfo);
}

void DisplayManager::drawImageInfo(uint32_t index,
                                   uint32_t total_images,
                                   ImageSource image_source) {
    log_d("[DisplayManager] Drawing image info on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawImageInfo(imageBuffer_.getCanvas(), index, total_images, image_source);
}

void DisplayManager::drawImageInfo(const String& message, ImageSource image_source) {
    log_d("[DisplayManager] Drawing image info message on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawImageInfo(imageBuffer_.getCanvas(), message, image_source);
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

void DisplayManager::drawCenteredMessageWithIcon(GFXcanvas8& canvas,
                                                 icon_name_t icon,
                                                 const String& title,
                                                 const String& message,
                                                 uint16_t icon_size) {
    log_d("[DisplayManager] Drawing centered message with icon on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawCenteredMessageWithIcon(canvas, icon, title, message, icon_size);
}

void DisplayManager::drawErrorWithDetails(const String& errMsgLn1,
                                          const String& errMsgLn2,
                                          const char* filename,
                                          uint16_t errorCode) {
    log_d("[DisplayManager] Drawing error with details on canvas");
    // Only need buffer to be initialized for drawing to canvas
    if (!imageBuffer_.isInitialized())
        return;
    photo_frame::drawErrorWithDetails(
        imageBuffer_.getCanvas(), errMsgLn1, errMsgLn2, filename, errorCode);
}

bool DisplayManager::render() {
    log_d("[DisplayManager] Rendering buffer to display");
    if (!initialized_ || !displayDriver_) {
        log_e("DisplayManager not initialized");
        return false;
    }

    return displayDriver_->picDisplay(imageBuffer_.getBuffer());
}

void DisplayManager::sleep() {
    log_d("[DisplayManager] Putting display to sleep");
    if (displayDriver_) {
        displayDriver_->sleep();
    }
}

void DisplayManager::powerOff() {
    log_d("[DisplayManager] Powering off display");
    if (displayDriver_) {
        displayDriver_->power_off();
    }
}

void DisplayManager::hibernate() {
    log_d("[DisplayManager] Putting display to hibernate");
    if (displayDriver_) {
        displayDriver_->hibernate();
    }
}

void DisplayManager::refresh(bool partial_update) {
    log_d("[DisplayManager] Refreshing display (partial_update=%s)",
          partial_update ? "true" : "false");
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
    log_i("Releasing DisplayManager resources");

    // Smart pointer automatically handles deletion
    displayDriver_.reset();

    imageBuffer_.release();
    initialized_ = false;
}

} // namespace photo_frame