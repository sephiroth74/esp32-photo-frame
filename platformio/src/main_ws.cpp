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

#ifdef ENABLE_WEBSERVER_DATAPROVIDER

#include "main_ws.h"
#include "board_util.h"
#include "config.h"
#include "display_manager.h"
#include "littlefs_manager.h"
#include "main_common.h"
#include "preferences_helper.h"
#include "rgb_status.h"
#include "types.h"
#include "ws_ap_manager.h"
#include "ws_display_utils.h"
#include "ws_server.h"
#include "ws_types.h"
#include "ws_utils.h"
#include <Arduino.h>
#include <memory>
#include <sys/time.h>
#include <time.h>

using namespace photo_frame::ws;

/// Flag to track if load_image is currently executing
volatile bool g_isLoadingImage = false;

// Flag to indicate if an image update occurred
volatile bool g_imageUpdated         = false;

static bool g_disconnectOnLastClient = false;

/// Timeout monitoring state variables
static WSServer* g_wsServer                                       = nullptr;
static photo_frame::littlefs_manager::LittleFsManager* g_littleFs = nullptr;
static photo_frame::DisplayManager* g_display                     = nullptr;
static unsigned long g_serverStartMs                              = 0;
static unsigned long g_lastCheckMs                                = 0;
static uint32_t g_timeout_ms                                      = 0;
static uint8_t g_display_rotation                                 = DEFAULT_ORIENTATION;
static photo_frame::BatteryInfo g_battery_info;
static bool g_clientConnected = false;

void performFactoryReset() {
    log_i("[WS] Perform factory reset: clearing preferences and resetting state");
    auto& prefs  = photo_frame::PreferencesHelper::getInstance();
    bool success = true;

    // TODO: Add any webserver-specific preferences to clear here
    success &= prefs.setLastImageTimestamp(0);
    success &= prefs.setDisplayRotation(DEFAULT_ORIENTATION); // reset to default orientation

    if (success) {
        log_i("[WS] ✓ All WS preferences cleared");
    } else {
        log_w("[WS] ⚠ Some preferences failed to clear");
    }
}

void shutdown(photo_frame::littlefs_manager::LittleFsManager* littleFs,
              photo_frame::DisplayManager* display,
              unsigned long delay_ms = 0) {
    log_i("[WS] Shutting down (delay %lu ms)", delay_ms);

    if (delay_ms > 0) {
        delay(delay_ms);
    }

    // TODO: Add any webserver-specific shutdown steps here

    if (littleFs) {
        littleFs->release();
    }
    if (display) {
        display->powerOff();
        display->release();
    }
    photo_frame::board_utils::displayPowerOff();
    photo_frame::board_utils::enterDeepSleep(ESP_SLEEP_WAKEUP_EXT0, 0);
}

DateTime updateDateTime(time_t timestamp) {
    log_i("[WS] Updating DateTime with timestamp: %u", timestamp);
    struct tm timeinfo;

    if (timestamp > 0) {
        setenv("TZ", TIMEZONE, 1);
        tzset();

        time_t timestamp_time = (time_t)timestamp;
        localtime_r(&timestamp_time, &timeinfo);

        struct timeval tv;
        tv.tv_sec  = timestamp;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }

    return DateTime(timeinfo.tm_year + 1900,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_mday,
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec);
}

DateTime getCurrentDateTime() {
    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    return DateTime(timeinfo.tm_year + 1900,
                    timeinfo.tm_mon + 1,
                    timeinfo.tm_mday,
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec);
}

/**
 * @brief Load and display the received image file
 * @param filename Filename of the image in LittleFS
 * @param orientation Display orientation (0-3)
 * @param timestamp Unix timestamp of the image (for overlay)
 * @param battery_info Current battery information (for overlay)
 * @param isNewUpload Flag indicating if the image is a new upload
 */
void displayReceivedFile(const char* filename,
                         uint8_t orientation,
                         uint32_t timestamp,
                         photo_frame::BatteryInfo& battery_info,
                         bool isNewUpload) {
    g_isLoadingImage = true;

    log_i("[WS] Loading image %s (orientation=%u, timestamp=%u)", filename, orientation, timestamp);

    if (!g_littleFs || !g_display) {
        log_e("[WS] LittleFS or Display not initialized");
        g_isLoadingImage = false;
        return;
    }

    // Retrieve preferences instance
    auto& prefs = photo_frame::PreferencesHelper::getInstance();

    // Create a wrapper for loading the PFR1 file into the display buffer
    photo_frame::PFR1BinaryFile wrapper(g_display->getWidth(), g_display->getHeight());

    // Convert timestamp to DateTime for overlay display
    DateTime image_time = updateDateTime(timestamp);

    // Update global rotation state
    g_display_rotation = orientation;

    // Save orientation and timestamp preference
    prefs.setDisplayRotation(orientation);
    prefs.setLastImageTimestamp(timestamp);

    auto error = photo_frame::ws_utils::loadLittleFsFile(filename, *g_littleFs, wrapper);
    g_display->clear();

    if (error != photo_frame::error_type::None) {
        error.log_detailed();
        g_display->setRotation(orientation);
        g_display->drawError(error, filename);
        g_display->render();
        g_isLoadingImage = false;
        return;
    }

    if (isNewUpload) {
        g_display->setImageSource(photo_frame::ImageSource::IMAGE_SOURCE_WEBSOCKET);
    } else {
        g_display->setImageSource(photo_frame::ImageSource::IMAGE_SOURCE_LOCAL_CACHE);
    }

    g_display->drawImage(wrapper);
    g_display->setRotation(orientation);
    g_display->drawOverlay();
    g_display->drawLastUpdate(image_time, 0); // Pass 0 for refresh seconds to skip wake-up time
    g_display->drawImageInfo(photo_frame::getImageSourceString(g_display->getImageSource()),
                             g_display->getImageSource());
    g_display->drawBatteryStatus(battery_info);
    g_display->render();

    g_isLoadingImage = false;
    g_imageUpdated   = true;
}

void onWsClientError(const WSEvent& event) {
    log_w("[WS] Client error: %s", event.message.c_str());
}

void onWsClientConnected(const WSEvent& event) {
    log_i("[WS] Client %u connected (active clients: %u)", event.clientId, event.clientsCount);
    g_clientConnected = true;
    log_d("[WS] Timeout monitoring disabled while clients are connected");
}

void onWsClientDisconnected(const WSEvent& event) {
    log_i("[WS] WebSocket client %u disconnected (active clients: %u)",
          event.clientId,
          event.clientsCount);
    g_clientConnected = (event.clientsCount > 0);

    if (!g_disconnectOnLastClient)
        return;

    // Check if an upload or image loading is in progress
    if (g_wsServer && g_wsServer->isUploadActive()) {
        log_w("[WS] Client disconnected but upload session is still active, waiting...");
        return;
    }

    if (g_isLoadingImage) {
        log_w("[WS] Client disconnected but image is being loaded, waiting...");
        return;
    }

    // Client disconnected and no active operations - go to deep sleep
    log_i("[WS] Client disconnected, no active operations - entering deep sleep");

    if (!g_imageUpdated) {
        // If no image update occurred, re-display current or default image
        log_d("[WS] No image update occurred, re-displaying current/default image");

        char filename[64];
        bool isNewUpload = false;
        if (g_littleFs->file_exists(WS_CURRENT_IMAGE_FILENAME)) {
            snprintf(filename, sizeof(filename), WS_CURRENT_IMAGE_FILENAME);
        } else {
            g_display_rotation = DEFAULT_ORIENTATION;
            isNewUpload        = false;
            snprintf(filename, sizeof(filename), WS_DEFAULT_IMAGE_FILENAME);
        }

        time_t timestamp = photo_frame::PreferencesHelper::getInstance().getLastImageTimestamp();
        displayReceivedFile(filename, g_display_rotation, timestamp, g_battery_info, isNewUpload);
    } else {
        log_v("[WS] Image was updated during session, no need to re-display");
    }
    shutdown(g_littleFs, g_display, 100);
}

void onWsImageReceived(const WSEvent& event) {
    log_i("[WS] Image received: %s (timestamp: %u, orientation: %u)",
          event.filepath.c_str(),
          event.timestamp,
          event.orientation);
    displayReceivedFile(
        event.filepath.c_str(), event.orientation, event.timestamp, g_battery_info, true);
}

void onWsShutDownRequested(const WSEvent& event) {
    log_i("[WS] Shutdown request received via WebSocket");

    // Check if an upload or image loading is in progress
    if (g_wsServer && g_wsServer->isUploadActive()) {
        log_w("[WS] Cannot shutdown: upload session is active");
        return;
    }

    if (g_isLoadingImage) {
        log_w("[WS] Cannot shutdown: image is being loaded");
        return;
    }

    log_d("[WS] Entering deep sleep in 100ms...");
    shutdown(g_littleFs, g_display, 100);
}

void main_webserver_setup() {
    Serial.begin(115200);
    delay(5000);

    // Initialize display power control (if configured)
    photo_frame::board_utils::initDisplayPower();
    auto& prefs = photo_frame::PreferencesHelper::getInstance();

    // Get singleton references - we'll store pointers to them in globals for loop() access
    g_littleFs = &photo_frame::littlefs_manager::LittleFsManager::getInstance();
    g_display  = &photo_frame::DisplayManager::getInstance();

    // Initialize DateTime with current time (or RTC time if available)
    updateDateTime(prefs.getLastImageTimestamp());

    // Get wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::getWakeupReason();
    photo_frame::board_utils::printWakeUpReason(wakeup_reason);

    bool is_first_boot = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;
    log_d("[WS] Is first boot: %s", is_first_boot ? "Yes" : "No");

    // Initialize hardware
    if (!initializeHardware()) {
        log_e("[WS] CRITICAL! Failed to initialize hardware!");
        shutdown(g_littleFs, g_display, 0);
        return;
    }

    photo_frame::board_utils::displayPowerOff();

    // Check for factory reset button press (5 second long press on WAKEUP_PIN)
    // This must be checked early, before any other operations
    log_d("[WS] Checking for factory reset button...");
    if (wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED) {
        log_d("[WS] Factory reset triggered!");
        performFactoryReset();

        if (g_littleFs->init()) {
            g_littleFs->delete_file(WS_CURRENT_IMAGE_FILENAME);
        }

        is_first_boot = true; // After reset, treat as first boot
    }
    log_d("[WS] No factory reset requested");

    g_display_rotation = prefs.getDisplayRotation();
    log_d("[WS] Display rotation from preferences: %u", g_display_rotation);

    // Check battery status
    photo_frame::photo_frame_error_t error = setupBatteryAndPower(g_battery_info, wakeup_reason);
    log_d("[WS] Battery: %.1f%%, %.1f mV", g_battery_info.percent, g_battery_info.millivolts);

    // Provide current runtime info to BoardInfo for GET_CONFIG
    BoardInfo::setBatteryInfo(g_battery_info);
    BoardInfo::setDisplayRotation(g_display_rotation);

    if (error == photo_frame::error_type::BatteryLevelCritical) {
        log_e("[BT] Battery is critical, showing error and sleeping");
        photo_frame::ws_utils::handleCriticalBattery(
            g_battery_info, wakeup_reason, g_display_rotation);
        return;
    }

    // Determine timeout based on boot type
    uint32_t timeout_ms = is_first_boot ? WS_FIRST_BOOT_TIMEOUT_MS : WS_LISTEN_TIMEOUT_MS;

    log_d("[WS] Starting image wait: %s",
          is_first_boot ? "First Boot Timeout" : "Subsequent Wakeup Timeout");
    log_d("[WS] Timeout set to %u ms", timeout_ms);

    // ========================================================================
    // Initialize display ONCE at the beginning (before WS operations)
    // ========================================================================
    log_i("[WS] Initializing display system...");

    // Phase 1: Initialize buffer
    if (!initializeImageBuffer()) {
        log_e("[WS] Failed to initialize display buffer");
        shutdown(g_littleFs, g_display, 10000);
        return;
    }

    // ========================================================================
    // Initialize WiFi Access Point
    // ========================================================================
    log_i("[WS] Initializing WiFi Access Point...");

    // Phase 2: Start WiFi AP
    photo_frame::ws::WSAPManager apManager;
    if (!apManager.begin()) {
        log_e("[WS] Failed to start WiFi AP - will display error");
        // TODO: Display error on screen indicating AP initialization failure
        error = photo_frame::error_type::WifiConnectionFailed;
    } else {
        log_i("[WS] WiFi AP started: SSID=%s, IP=%s",
              apManager.getSSID().c_str(),
              apManager.getIP().c_str());
    }

    // ========================================================================
    // Initialize LittleFS and load image (if AP started successfully)
    // ========================================================================
    error = g_littleFs->init() ? error : photo_frame::error_type::LittleFSInitFailed;

    // Phase 3: Initialize hardware
    photo_frame::board_utils::displayPowerOn();

    if (!initializeDisplayHardware()) {
        log_e("[WS] Failed to initialize display hardware");
        shutdown(g_littleFs, g_display, 10000);
        return;
    }

    delay(300);
    g_display->setRotation(g_display_rotation);

    // ========================================================================
    // Load and display image with connection info
    // ========================================================================
    log_d("[WS] Loading and displaying image...");

    if (error == photo_frame::error_type::None) {

        // Load current or default image
        error = photo_frame::ws_display_utils::drawImageFile(
            *g_littleFs, *g_display, WS_CURRENT_IMAGE_FILENAME);

        if (error != photo_frame::error_type::None) {
            log_w("[WS] Failed to load current image, trying default image");
            error = photo_frame::ws_display_utils::drawImageFile(
                *g_littleFs, *g_display, WS_DEFAULT_IMAGE_FILENAME);

            if (error != photo_frame::error_type::None) {
                log_w("[WS] Failed to load default image, showing blank screen");
                g_display->clear(DISPLAY_COLOR_WHITE);
            } else {
                g_display->setImageSource(photo_frame::ImageSource::IMAGE_SOURCE_LOCAL_CACHE);
            }
        } else {
            g_display->setImageSource(photo_frame::ImageSource::IMAGE_SOURCE_WEBSOCKET);
        }
        error = photo_frame::error_type::None;
    } else {
        // just show the error and shut down
        log_w("[WS] Skipping image load due to previous error: %d", error.code);
        g_display->clear(DISPLAY_COLOR_WHITE);
        g_display->drawError(error);
        g_display->render();
        shutdown(g_littleFs, g_display, 10000);
        return;
    }

    // Draw connection info box (QR code, SSID, IP)
    // Create deep link URL for Flutter app:
    // photoframe://connect?ip=...&ssid=...&port=...&v=1&d=1&w=800&h=480
    std::string deepLinkUrl =
        "photoframe://connect?ip=" + apManager.getIP() + "&ssid=" + apManager.getSSID() +
        "&port=" + std::to_string(WS_PORT) + "&v=" + std::to_string(PFR1_VERSION) +
#ifdef DISP_6C
        "&d=1" +
#else
        "&d=0" +
#endif
        "&w=" + std::to_string(EPD_WIDTH) + "&h=" + std::to_string(EPD_HEIGHT);

    photo_frame::ws_display_utils::drawConnectionInfoBox(
        *g_display, apManager.getSSID(), apManager.getIP(), deepLinkUrl);

    g_display->drawOverlay();
    g_display->drawLastUpdate(getCurrentDateTime(), 0);
    g_display->drawBatteryStatus(g_battery_info);
    g_display->drawImageInfo(photo_frame::getImageSourceString(g_display->getImageSource()),
                             g_display->getImageSource());

    // Render to display
    if (!g_display->render()) {
        log_e("[WS] Failed to render display");
    } else {
        log_d("[WS] Display rendered successfully");
    }

    log_d("[WS] Setup complete - waiting for WebSocket connections");

    // Verify WiFi AP is still active before starting WebSocket
    log_d("[WS] Verifying WiFi AP status...");
    log_d("[WS] AP SSID: %s", apManager.getSSID().c_str());
    log_d("[WS] AP IP: %s", apManager.getIP().c_str());
    log_d("[WS] AP Running: %s",
          apManager.isClientConnected() ? "Yes (client connected)" : "Yes (no clients yet)");

    // Give WiFi AP time to fully stabilize before starting WebSocket
    log_v("[WS] Waiting 1 second for WiFi AP to stabilize...");
    delay(1000);

    // Start WebSocket server for GET_CONFIG testing
    log_d("[WS] Creating WebSocket server on port %u...", WS_PORT);
    static auto wsServer = std::make_unique<WSServer>(WS_PORT, [](const WSEvent& event) {
        switch (event.type) {
        case WSEventType::ERROR:               onWsClientError(event); break;
        case WSEventType::CLIENT_CONNECTED:    onWsClientConnected(event); break;
        case WSEventType::CLIENT_DISCONNECTED: onWsClientDisconnected(event); break;
        case WSEventType::IMAGE_RECEIVED:      onWsImageReceived(event); break;
        case WSEventType::SHUTDOWN_REQUEST:    onWsShutDownRequested(event); break;
        default:                               break;
        }
    });

    if (!wsServer->begin()) {
        log_e("[WS] Failed to start WebSocket server");
    } else {
        log_i("[WS] WebSocket server listening on port %u", WS_PORT);
    }

    // Initialize global state for timeout monitoring in loop()
    g_wsServer      = wsServer.get();
    g_serverStartMs = millis();
    g_lastCheckMs   = millis();
    g_timeout_ms    = timeout_ms;

    log_i("[WS] Setup complete - timeout monitoring will run in loop()");
    log_i("[WS] Timeout: %u ms (%u minutes)", timeout_ms, timeout_ms / 60000);

    photo_frame::board_utils::checkHeapHealth("[WS] End of main_webserver_setup");
}

void main_webserver_loop() {
    // WebSocket server runs in its own FreeRTOS task
    // This loop monitors timeout and handles shutdown
    delay(100);
    yield();

    // Safety check - ensure globals are initialized
    if (!g_wsServer || !g_littleFs || !g_display) {
        return;
    }

    unsigned long now = millis();

    // Check timeout every second
    if (now - g_lastCheckMs >= 1000) {
        g_lastCheckMs = now;

        // Skip timeout check if any client is connected
        if (g_clientConnected) {
            // Log status every 30 seconds while clients are connected
            if ((now - g_serverStartMs) % 30000 < 1000) {
                log_d("[WS] Clients connected - timeout disabled");
            }
            return;
        }

        // Get last activity time from WebSocket server
        unsigned long lastActivityMs = g_wsServer->getLastActivityMs();
        unsigned long timeSinceActivity;

        // If no activity yet, use server start time
        if (lastActivityMs == 0) {
            timeSinceActivity = now - g_serverStartMs;
        } else {
            timeSinceActivity = now - lastActivityMs;
        }

        // Check if timeout exceeded
        if (timeSinceActivity >= g_timeout_ms) {
            // Don't shutdown if image is being loaded
            if (g_isLoadingImage) {
                log_d("[WS] Timeout reached but image is loading, waiting...");
                return;
            }

            // Don't shutdown if upload is active
            if (g_wsServer->isUploadActive()) {
                log_d("[WS] Timeout reached but upload is active, waiting...");
                return;
            }

            log_d("[WS] Timeout reached after %u ms of inactivity", timeSinceActivity);
            log_d("[WS] No activity detected, initiating shutdown");

            if (!g_imageUpdated) {
                // If no image update occurred, re-display current or default image
                log_d("[WS] No image update occurred, re-displaying current/default image");

                char filename[64];
                bool isNewUpload = false;
                if (g_littleFs->file_exists(WS_CURRENT_IMAGE_FILENAME)) {
                    snprintf(filename, sizeof(filename), WS_CURRENT_IMAGE_FILENAME);
                } else {
                    g_display_rotation = DEFAULT_ORIENTATION;
                    isNewUpload        = false;
                    snprintf(filename, sizeof(filename), WS_DEFAULT_IMAGE_FILENAME);
                }

                time_t timestamp =
                    photo_frame::PreferencesHelper::getInstance().getLastImageTimestamp();
                displayReceivedFile(
                    filename, g_display_rotation, timestamp, g_battery_info, isNewUpload);
            } else {
                log_v("[WS] Image was updated during session, no need to re-display");
            }
            shutdown(g_littleFs, g_display, 100);
            return;
        }

        // Log status every 30 seconds
        if ((now - g_serverStartMs) % 30000 < 1000) {
            log_d("[WS] Active - last activity %u seconds ago (timeout in %u seconds)",
                  timeSinceActivity / 1000,
                  (g_timeout_ms - timeSinceActivity) / 1000);
        }
    }
}

#endif // ENABLE_WEBSERVER_DATAPROVIDER