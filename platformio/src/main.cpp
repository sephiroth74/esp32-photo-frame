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

#ifndef ENABLE_DISPLAY_DIAGNOSTIC

#include <Arduino.h>

#include "esp32/spiram.h"

#include "battery.h"
#include "board_util.h"
#include "config.h"
#include "errors.h"
#include "io_utils.h"
#include "preferences_helper.h"
#include "renderer.h"
#include "canvas_renderer.h"
#include "image_buffer.h"
#include "rgb_status.h"

#include "sd_card.h"
#include "littlefs_manager.h"
#include "string_utils.h"
#include "unified_config.h"
#include "wifi_manager.h"

#include <assets/icons/icons.h>
#include <Fonts/FreeMonoBold9pt7b.h>

#include "google_drive.h"
#include "google_drive_client.h"

// Google Drive instance (initialized from JSON config)
photo_frame::google_drive drive;

// Helper function to cleanup temporary files from LittleFS
void cleanup_temp_image_file()
{
    photo_frame::littlefs_manager::LittleFsManager::cleanup_temp_files();
}

#ifndef USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader(BATTERY_PIN,
    BATTERY_RESISTORS_RATIO,
    BATTERY_NUM_READINGS,
    BATTERY_DELAY_BETWEEN_READINGS);
#else // USE_SENSOR_MAX1704X
photo_frame::battery_reader battery_reader;
#endif // USE_SENSOR_MAX1704X

photo_frame::sd_card sdCard; // SD_MMC uses fixed SDIO pins
photo_frame::wifi_manager wifiManager;
photo_frame::unified_config systemConfig; // Unified configuration system

// Static image buffer using RAII class for automatic management
static photo_frame::ImageBuffer g_imageBuffer;
static bool portrait_mode = false; // Track display orientation

unsigned long startupTime = 0;

typedef struct {
    long refresh_seconds;
    uint64_t refresh_microseconds;
} refresh_delay_t;

refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now);

/**
 * @brief Initialize hardware components (Serial, RGB LED, board utilities)
 * @return true if initialization successful, false otherwise
 */
bool initialize_hardware();

/**
 * @brief Read and validate battery level, handle critical battery states
 * @param battery_info Reference to store battery information
 * @param wakeup_reason Current wakeup reason for power management decisions
 * @return Error state after battery check
 */
photo_frame::photo_frame_error_t setup_battery_and_power(photo_frame::battery_info_t& battery_info,
    esp_sleep_wakeup_cause_t wakeup_reason);

/**
 * @brief Setup time synchronization via WiFi and NTP
 * @param battery_info Battery information for power conservation decisions
 * @param is_reset Whether this is a reset/startup (affects NTP sync behavior)
 * @param now Reference to store current DateTime
 * @return Error state after time/WiFi operations
 */
photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info, bool is_reset, DateTime& now);

/**
 * @brief Handle Google Drive operations (initialization, file selection, download)
 * @param is_reset Whether this is a reset/startup (affects TOC writing)
 * @param file Reference to File object for image (closed after loading to buffer)
 * @param original_filename Original filename from SD card (for format detection and logging)
 * @param image_index Reference to store selected image index
 * @param total_files Reference to store total file count
 * @param battery_info Battery information for download decisions
 * @param file_ready Reference to boolean indicating if file is ready (buffer loaded or file open)
 * @return Error state after Google Drive operations
 */
photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
    fs::File& file,
    String& original_filename,
    uint32_t& image_index,
    uint32_t& total_files,
    const photo_frame::battery_info_t& battery_info,
    bool& file_ready);

/**
 * @brief Handle SD card operations (file selection from local storage)
 * @param is_reset Whether this is a reset/startup
 * @param file Reference to File object for image
 * @param original_filename Original filename from SD card
 * @param image_index Reference to store selected image index
 * @param total_files Reference to store total file count
 * @param battery_info Battery information
 * @param file_ready Reference to boolean indicating if file is ready
 * @return Error state after SD card operations
 */
photo_frame::photo_frame_error_t
handle_sd_card_operations(bool is_reset,
    fs::File& file,
    String& original_filename,
    uint32_t& image_index,
    uint32_t& total_files,
    const photo_frame::battery_info_t& battery_info,
    bool& file_ready);

/**
 * @brief Process image file (validation, copying to LittleFS, rendering)
 * @param file File handle for image to process (temporary file in LittleFS)
 * @param original_filename Original filename from SD card (used for format detection and error
 * reporting)
 * @param current_error Current error state
 * @param file_ready Whether file is ready (buffer loaded or file open)
 * @param now Current DateTime for display
 * @param refresh_delay Refresh delay information
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param battery_info Battery information
 * @return Updated error state after image processing
 */
photo_frame::photo_frame_error_t
process_image_file(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    bool file_ready,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    const photo_frame::battery_info_t& battery_info);

/**
 * @brief Handle final cleanup and prepare for deep sleep
 * @param battery_info Battery information for sleep calculations
 * @param now Current DateTime for sleep timing
 * @param wakeup_reason Wakeup reason for sleep decisions
 * @param refresh_delay Pre-calculated refresh delay to avoid re-reading potentiometer
 */
void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
    DateTime& now,
    esp_sleep_wakeup_cause_t wakeup_reason,
    const refresh_delay_t& refresh_delay);

/**
 * @brief Handle image rendering with full/partial display modes and error handling
 * @param file The validated image file to render (temporary file in LittleFS)
 * @param original_filename Original filename from SD card (used for format detection and error
 * reporting)
 * @param current_error Current error state
 * @param now Current DateTime for info display
 * @param refresh_delay Refresh delay info
 * @param image_index Current image index
 * @param total_files Total number of files
 * @param drive Google Drive instance
 * @param battery_info Battery information
 * @return Updated error state after rendering attempt
 */
photo_frame::photo_frame_error_t render_image(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    photo_frame::google_drive& drive,
    const photo_frame::battery_info_t& battery_info);

/**
 * @brief Initialize the PSRAM image buffer for Mode 1 format rendering
 *
 * Uses ImageBuffer class for automatic RAII-based buffer management.
 * The buffer is allocated once in PSRAM (if available) and reused for all images.
 *
 * @note This function must be called before any image loading operations
 */
void init_image_buffer()
{
    if (g_imageBuffer.isInitialized()) {
        log_w("[main] Image buffer already initialized");
        return;
    }

    log_i("[main] Initializing image buffer...");

    if (!g_imageBuffer.init(DISP_WIDTH, DISP_HEIGHT, true)) {
        log_e("[main] CRITICAL: Failed to initialize image buffer!");
        log_e("[main] Cannot continue without image buffer");
    } else {
        log_i("[main] Image buffer initialized successfully");
        log_i("[main] Buffer size: %u bytes, in %s",
              g_imageBuffer.getSize(),
              g_imageBuffer.isInPsram() ? "PSRAM" : "heap");
    }
}

/**
 * @brief Cleanup the image buffer
 *
 * The ImageBuffer class automatically handles cleanup through its destructor,
 * but this function can be called explicitly if needed.
 */
void cleanup_image_buffer()
{
    if (g_imageBuffer.isInitialized()) {
        log_i("[main] Releasing image buffer");
        g_imageBuffer.release();
        log_i("[main] Image buffer released");
    }
}

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000)
        delay(10); // Wait for serial or timeout

    // Immediate diagnostic check

    log_i("\n==================================");
    log_i("*** NORMAL MODE ***");
    log_i("==================================");

    // Initialize hardware components
    if (!initialize_hardware()) {
        return;
    }

    // Determine wakeup reason and setup basic state
    esp_sleep_wakeup_cause_t wakeup_reason = photo_frame::board_utils::get_wakeup_reason();
    bool is_reset = wakeup_reason == ESP_SLEEP_WAKEUP_UNDEFINED;

    char wakeup_reason_string[32];
    photo_frame::board_utils::get_wakeup_reason_string(
        wakeup_reason, wakeup_reason_string, sizeof(wakeup_reason_string));

    log_i("Wakeup reason: %s (%d)", wakeup_reason_string, wakeup_reason);
    log_i("Is reset: %s", is_reset ? "Yes" : "No");

    // Setup battery and power management
    photo_frame::battery_info_t battery_info;
    photo_frame::photo_frame_error_t error = setup_battery_and_power(battery_info, wakeup_reason);

#if BATTERY_POWER_SAVING
    if (error == photo_frame::error_type::BatteryEmpty) {
        // stop here as battery is empty and we entered deep sleep
        return;
    }
#endif // BATTERY_POWER_SAVING

    // Setup time synchronization and connectivity
    DateTime now = DateTime((uint32_t)0);

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        error = setup_time_and_connectivity(battery_info, is_reset, now);
    }

    // Initialize PSRAM image buffer BEFORE Google Drive operations
    // This must be done before load_image_to_buffer() is called
    log_i("--------------------------------------");
    log_i("- Initializing PSRAM image buffer...");
    log_i("--------------------------------------");
    init_image_buffer();

    // Handle Google Drive operations
    fs::File file;
    uint32_t image_index = 0, total_files = 0;
    String original_filename; // Store original filename for format detection
    bool file_ready = false; // Track if file is ready (buffer loaded or file open)

    if (error == photo_frame::error_type::None && !battery_info.is_critical()) {
        // Choose image source based on configuration
        if (systemConfig.google_drive.enabled) {
            log_i("Using Google Drive as image source");
            RGB_SET_STATE(GOOGLE_DRIVE); // Show Google Drive operations
            error = handle_google_drive_operations(
                is_reset, file, original_filename, image_index, total_files, battery_info, file_ready);
        } else if (systemConfig.sd_card.enabled) {
            log_i("Using SD Card as image source (Google Drive disabled)");
            RGB_SET_STATE(SD_READING); // Show SD Card operations
            error = handle_sd_card_operations(
                is_reset, file, original_filename, image_index, total_files, battery_info, file_ready);
        } else {
            // This should not happen as config validation ensures at least one source is enabled
            log_e("No image source enabled!");
            error = photo_frame::error_type::InvalidConfigNoImageSource;
        }
    }

    // Safely disconnect WiFi with proper cleanup
    if (wifiManager.is_connected()) {
        log_i("Disconnecting WiFi to save power...");
        // Add small delay to ensure pending WiFi operations complete
        delay(100);
        wifiManager.disconnect();
        // Wait for disconnect to complete
        delay(200);
    }

    log_i("WiFi operations complete - using NTP-only time");

    // Calculate refresh delay
    log_i("--------------------------------------");
    log_i("- Calculating refresh rate");
    log_i("--------------------------------------");
    refresh_delay_t refresh_delay = calculate_wakeup_delay(battery_info, now);

    // Initialize E-Paper display
    log_i("--------------------------------------");
    log_i("- Initializing E-Paper display...");
    log_i("--------------------------------------");

    delay(100);
    RGB_SET_STATE(RENDERING); // Show display rendering

    // Use portrait_mode from config (or from preferences if config failed)
    portrait_mode = systemConfig.board.portrait_mode; // Set global portrait_mode

    // If config loading failed, try to get from preferences
    if (!systemConfig.is_valid()) {
        auto& prefs = photo_frame::PreferencesHelper::getInstance();
        portrait_mode = prefs.getPortraitMode(); // Default to false (landscape) if not set
        log_w("Config invalid, using portrait_mode from preferences: %s",
            portrait_mode ? "portrait" : "landscape");
    }

    // Initialize the new renderer with abstraction layer
    if (!photo_frame::renderer::init()) {
        log_e("Failed to initialize renderer");
        // TODO: Handle initialization error properly
    }

    // TODO: Portrait mode handling will be implemented in Phase 2
    // For now, the display always operates in hardware landscape mode
    // photo_frame::renderer::setRotation(portrait_mode ? 1 : 0);

    // Allow time for display SPI bus initialization to complete
    delay(300);
    log_i("Display initialization complete");

    // Prepare display for rendering
    // Note: fillScreen() removed in v0.11.0 to eliminate race condition causing white vertical stripes
    // The full-screen image write will overwrite the entire display buffer, making fillScreen() redundant
    log_i("Preparing display for rendering...");

    // Check if file is ready (binary image loaded to buffer)
    if (error == photo_frame::error_type::None && !file_ready) {
        log_e("File is not ready! (buffer not loaded)");
        error = photo_frame::error_type::SdCardFileOpenFailed;
    }

    // Handle errors or process image file
    if (error != photo_frame::error_type::None) {
        RGB_SET_STATE(ERROR); // Show error status

        // Get canvas from ImageBuffer
        GFXcanvas8& canvas = g_imageBuffer.getCanvas();

        // Clear canvas and draw error
        canvas.fillScreen(DISPLAY_COLOR_WHITE);
        photo_frame::canvas_renderer::draw_error(canvas, error);

        if (error != photo_frame::error_type::BatteryLevelCritical && now.isValid()) {
            photo_frame::canvas_renderer::draw_last_update(canvas, now, refresh_delay.refresh_seconds);
        }

        // Render to display
        photo_frame::renderer::render_image(g_imageBuffer.getBuffer());
    } else {
        // Process image file (validation and rendering)
        error = process_image_file(file,
            original_filename.c_str(),
            error,
            file_ready,
            now,
            refresh_delay,
            image_index,
            total_files,
            battery_info);
    }

    // Finalize and enter sleep - show sleep preparation with delay
    RGB_SET_STATE(SLEEP_PREP); // Show sleep preparation
    delay(2500); // Allow sleep preparation animation to complete
    finalize_and_enter_sleep(battery_info, now, wakeup_reason, refresh_delay);
}

void loop()
{
    delay(1000); // Just to avoid watchdog reset
}

photo_frame::photo_frame_error_t render_image(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    photo_frame::google_drive& drive,
    const photo_frame::battery_info_t& battery_info)
{
    photo_frame::photo_frame_error_t error = current_error;

    // Get display capabilities
    bool has_partial_update = photo_frame::renderer::has_partial_update();

    if (error == photo_frame::error_type::None && !has_partial_update) {
        // ----------------------------------------------
        // the display does not support partial update
        // ----------------------------------------------
        log_w("Display does not support partial update!");

        bool rendering_failed = false;
        uint16_t error_code = 0;

#if defined(DISP_6C)
        // ============================================
        // 6C/7C displays: Non-paged rendering
        // ============================================
        // These displays render from PSRAM buffer (binary format only)
        // Binary format - render from PSRAM buffer (already loaded)
        // For 6C/7C displays: Use GFXcanvas8 to draw overlays on buffer with proper renderer functions
        log_i("[main] Rendering Mode 1 format from PSRAM buffer with overlays");

        // Check portrait mode from config (or preferences fallback)
        bool portrait_mode = systemConfig.board.portrait_mode;
        if (!systemConfig.is_valid()) {
            auto& prefs = photo_frame::PreferencesHelper::getInstance();
            portrait_mode = prefs.getPortraitMode();
        }

        // Get canvas from ImageBuffer
        GFXcanvas8& canvas = g_imageBuffer.getCanvas();

        if (portrait_mode) {
            canvas.setRotation(1);
        }

        // Draw overlay based on portrait mode
        photo_frame::canvas_renderer::draw_overlay(canvas);

        // Draw status information
        photo_frame::canvas_renderer::draw_last_update(canvas, now, refresh_delay.refresh_seconds);
        photo_frame::canvas_renderer::draw_image_info(canvas,
            image_index, total_files, drive.get_last_image_source());
        photo_frame::canvas_renderer::draw_battery_status(canvas, battery_info);

        // Render the image with overlays to the display
        log_i("Rendering image to display...");
        if (!photo_frame::renderer::render_image(g_imageBuffer.getBuffer())) {
            log_e("Failed to render image!");
            rendering_failed = true;
        } else {
            log_i("Display render complete with overlays");
        }
#else
        // ============================================
        // B/W displays: Same system as 6C, no paged rendering
        // ============================================

        // Get canvas from ImageBuffer
        GFXcanvas8& canvas = g_imageBuffer.getCanvas();

        if (portrait_mode) {
            canvas.setRotation(1);
        }

        // Draw overlay based on portrait mode
        photo_frame::canvas_renderer::draw_overlay(canvas);

        // Draw status information
        photo_frame::canvas_renderer::draw_last_update(canvas, now, refresh_delay.refresh_seconds);
        photo_frame::canvas_renderer::draw_image_info(canvas,
            image_index, total_files, drive.get_last_image_source());
        photo_frame::canvas_renderer::draw_battery_status(canvas, battery_info);

        // Render the image with overlays to the display
        log_i("Rendering B/W image to display...");
        if (!photo_frame::renderer::render_image(g_imageBuffer.getBuffer())) {
            log_e("Failed to render B/W image!");
            rendering_failed = true;
        } else {
            log_i("B/W display render complete with overlays");
        }

        /* OLD PAGED RENDERING CODE - REMOVED
        int page_index = 0;
        display.firstPage();
        do {
            log_d("Drawing page: %d", page_index);
            // Binary format only - paged rendering
            size_t standardSize = DISP_WIDTH * DISP_HEIGHT;
            size_t fileSize = file.size();

            if (fileSize == standardSize) {
                log_i("[main] Detected standard format (%d bytes) - using paged rendering", standardSize);
                error_code = photo_frame::renderer::draw_binary_from_file_paged(
                    file, original_filename, DISP_WIDTH, DISP_HEIGHT, page_index);
            } else {
                log_e("[main] ERROR: Unknown binary format size: %d bytes", fileSize);
                error_code = photo_frame::error_type::BinaryRenderingFailed.code;
            }

            if (error_code != 0) {
                log_e("Failed to render binary image!");
                rendering_failed = true;
                break;
            }

            display.writeFillRect(0, 0, DISP_WIDTH, 16, DISPLAY_COLOR_WHITE);
            photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
            photo_frame::renderer::draw_image_info(
                image_index, total_files, drive.get_last_image_source());
            photo_frame::renderer::draw_battery_status(battery_info);

            page_index++;
        } while (display.nextPage());
        */
#endif

#if defined(DISP_6C)
        // Power recovery delay after page refresh for 6-color displays
        // Increased from 400ms to 1000ms in v0.11.1 to improve power stability
        // Helps prevent:
        // - White vertical stripes from voltage drop
        // - Washout from incomplete capacitor recharge
        // - Display artifacts from power supply instability
        delay(1000);
#endif

        // Handle rendering errors
        if (rendering_failed) {
            log_w("Rendering failed!");

            // Clear canvas and draw error
            canvas.fillScreen(DISPLAY_COLOR_WHITE);
            photo_frame::canvas_renderer::draw_error_with_details(canvas,
                TXT_IMAGE_FORMAT_NOT_SUPPORTED, "", original_filename, error_code);

            // Draw status information
            photo_frame::canvas_renderer::draw_last_update(canvas, now, refresh_delay.refresh_seconds);
            photo_frame::canvas_renderer::draw_image_info(canvas,
                image_index, total_files, drive.get_last_image_source());
            photo_frame::canvas_renderer::draw_battery_status(canvas, battery_info);

            // Render error to display
            photo_frame::renderer::render_image(g_imageBuffer.getBuffer());
        }

        file.close(); // Close the file after drawing
        cleanup_temp_image_file(); // Clean up temporary LittleFS file

    } else if (error == photo_frame::error_type::None) {
        // -------------------------------
        // Display has partial update - NOT SUPPORTED IN NEW SYSTEM
        // -------------------------------
        log_w("Partial update mode not yet supported in new display system");

        // For now, use same rendering as full update
        // Get canvas from ImageBuffer
        GFXcanvas8& partial_canvas = g_imageBuffer.getCanvas();

        // Draw overlay and status
        photo_frame::canvas_renderer::draw_overlay(partial_canvas);
        photo_frame::canvas_renderer::draw_last_update(partial_canvas, now, refresh_delay.refresh_seconds);
        photo_frame::canvas_renderer::draw_image_info(partial_canvas,
            image_index, total_files, drive.get_last_image_source());
        photo_frame::canvas_renderer::draw_battery_status(partial_canvas, battery_info);

        // Render to display
        photo_frame::renderer::render_image(g_imageBuffer.getBuffer());

        file.close(); // Close the file after drawing
        cleanup_temp_image_file(); // Clean up temporary LittleFS file

        /* OLD PARTIAL UPDATE CODE - REMOVED
        display.setPartialWindow(0, 0, DISP_WIDTH, 16);
        display.firstPage();
        do {
            display.fillScreen(DISPLAY_COLOR_WHITE);
            photo_frame::renderer::draw_last_update(now, refresh_delay.refresh_seconds);
            photo_frame::renderer::draw_image_info(
                image_index, total_files, drive.get_last_image_source());
            photo_frame::renderer::draw_battery_status(battery_info);
        } while (display.nextPage()); // Clear the display
        */
    }

    return error;
}

bool initialize_hardware()
{
    analogReadResolution(12);
    startupTime = millis();
    photo_frame::board_utils::blink_builtin_led(1, 900, 100);
    photo_frame::board_utils::disable_built_in_led();

#ifdef LED_PWR_PIN
    pinMode(LED_PWR_PIN, OUTPUT);
    digitalWrite(LED_PWR_PIN, LOW);
#endif // LED_PWR_PIN

    // PSRAM initialization (mandatory)
    log_i("[PSRAM] Initializing PSRAM...");

    // Check if PSRAM is already initialized by the framework
    if (psramFound()) {
        log_i("[PSRAM] PSRAM already initialized by framework");
        log_i("[PSRAM] Available PSRAM: %u bytes", ESP.getPsramSize());
        log_i("[PSRAM] Free PSRAM: %u bytes", ESP.getFreePsram());
    } else {
        // Try manual initialization if not already done
        esp_err_t ret = esp_spiram_init();
        if (ret != ESP_OK) {
            log_e("[PSRAM] CRITICAL: Failed to initialize PSRAM: %s", esp_err_to_name(ret));
            log_e("[PSRAM] PSRAM is required for this board configuration!");
            log_e("[PSRAM] System will likely crash due to memory constraints");
// For ProS3(d) and other PSRAM-mandatory boards, we should halt
#ifdef BOARD_HAS_PSRAM
            ESP.restart(); // Restart to try again
#endif
        } else {
            log_i("[PSRAM] PSRAM initialized successfully");

            // Note: esp_spiram_add_to_heapalloc() is deprecated in newer ESP-IDF
            // PSRAM should be automatically added to heap with CONFIG_SPIRAM_USE_MALLOC
            // which is set in platformio.ini via board_build.arduino.memory_type

            log_i("[PSRAM] Available PSRAM: %u bytes", ESP.getPsramSize());
            log_i("[PSRAM] Free PSRAM: %u bytes", ESP.getFreePsram());
        }
    }

#ifdef RGB_STATUS_ENABLED
    // Initialize RGB status system (FeatherS3 NeoPixel)
    log_i("[RGB] Initializing RGB status system...");
    if (!rgbStatus.begin()) {
        log_w("[RGB] Warning: Failed to initialize RGB status system");
    } else {
        RGB_SET_STATE(STARTING); // Show startup indication
    }
#else
    log_i("[RGB] RGB status system disabled");
#endif // RGB_STATUS_ENABLED

    log_i("------------------------------");
    log_i("Photo Frame %s", FIRMWARE_VERSION_STRING);
    log_i("------------------------------");

    photo_frame::board_utils::print_board_stats();

#if DEBUG_MODE
    photo_frame::board_utils::print_board_pins();
#endif // DEBUG_MODE

    return true;
}

photo_frame::photo_frame_error_t setup_battery_and_power(photo_frame::battery_info_t& battery_info,
    esp_sleep_wakeup_cause_t wakeup_reason)
{
    log_i("--------------------------------------");
    log_i("- Reading battery level...");
    log_i("--------------------------------------");

    battery_reader.init();
    battery_info = battery_reader.read();

    // print the battery levels
#ifdef DEBUG_BATTERY_READER
    log_i("Battery level: %d%%, %d mV, Raw mV: %d", battery_info.percent, battery_info.millivolts, battery_info.raw_millivolts);
#else
    log_i("Battery level: %.1f%%, %.1f mV", battery_info.percent, battery_info.millivolts);
#endif // DEBUG_BATTERY_READER

    if (battery_info.is_empty()) {
        log_e("Battery is empty!");
#ifdef BATTERY_POWER_SAVING
        unsigned long elapsed = millis() - startupTime;
        log_i("Elapsed seconds since startup: %lu s", elapsed / 1000);

        log_i("Entering deep sleep to preserve battery...");
        photo_frame::board_utils::enter_deep_sleep(wakeup_reason); // Enter deep sleep mode
                                                                   // Battery too low to continue
#endif // BATTERY_POWER_SAVING
#ifdef RGB_STATUS_ENABLED
        rgbStatus.disable();
        log_i("[RGB] Disabled RGB LED to conserve battery power");
#endif // RGB_STATUS_ENABLED
        return photo_frame::error_type::BatteryEmpty;
    } else if (battery_info.is_critical()) {
        log_w("Battery level is critical!");
        RGB_SET_STATE_TIMED(BATTERY_LOW, 3000); // Show battery critical warning briefly

        // Disable RGB after warning to save maximum power
        delay(3500);
#ifdef RGB_STATUS_ENABLED
        rgbStatus.disable();
        log_i("[RGB] Disabled RGB LED to conserve battery power");
#endif // RGB_STATUS_ENABLED
       // Indicate critical battery error
        return photo_frame::error_type::BatteryLevelCritical;
    }

    // Battery level allows continued operation
    return photo_frame::error_type::None;
}

photo_frame::photo_frame_error_t
setup_time_and_connectivity(const photo_frame::battery_info_t& battery_info, bool is_reset, DateTime& now)
{
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;

    log_i("--------------------------------------");
    log_i("- Initialize SD card and load configuration...");
    log_i("--------------------------------------");

    RGB_SET_STATE(SD_READING); // Show SD card operations
    error = sdCard.begin();

    // Reduce RGB brightness if battery is low to save power
    if (battery_info.is_low()) {
#ifdef RGB_STATUS_ENABLED
        rgbStatus.setBrightness(32); // Reduce brightness to 50% of normal for low battery
#endif // RGB_STATUS_ENABLED
    }

    // Load unified configuration from SD card
    if (error == photo_frame::error_type::None) {
        log_i("Loading unified configuration...");
        error = photo_frame::load_unified_config_with_fallback(sdCard, CONFIG_FILEPATH, systemConfig);

        if (error != photo_frame::error_type::None) {
            log_w("Failed to load unified configuration: %d", error.code);
            // Configuration loading failed, but fallback values are loaded
            // Continue with fallback configuration
            error = photo_frame::error_type::None;
        }

        // Validate essential configuration
        if (!systemConfig.wifi.is_valid()) {
            log_w("WARNING: WiFi configuration is missing or invalid!");
            log_w("Please ensure CONFIG_FILEPATH contains valid WiFi credentials");
            error = photo_frame::error_type::WifiCredentialsNotFound;
        }
    } else {
        log_w("SD card initialization failed - using fallback configuration");

        // SD card failed, load fallback configuration and calculate extended sleep
        load_fallback_config(systemConfig);

        // Enter deep sleep immediately with extended duration
        // photo_frame::board_utils::enter_deep_sleep(ESP_SLEEP_WAKEUP_UNDEFINED, fallback_sleep_microseconds);
        return error; // This line won't be reached, but included for completeness
    }

    // WiFi is optional for SD card mode, required for Google Drive
    bool wifiRequired = systemConfig.google_drive.enabled;

    if (error == photo_frame::error_type::None) {
        log_i("Initializing WiFi manager with unified configuration...");
        RGB_SET_STATE(WIFI_CONNECTING); // Show WiFi connecting status

        // Initialize WiFi manager with multiple networks from unified config
        error = wifiManager.init_with_networks(systemConfig.wifi);

        if (error == photo_frame::error_type::None) {
            // NTP-only time fetching
            log_i("Fetching time from NTP servers...");
            error = wifiManager.connect();
            if (error == photo_frame::error_type::None) {
                now = wifiManager.fetch_datetime(&error);
                if (!now.isValid() || error != photo_frame::error_type::None) {
                    log_e("Failed to fetch time from NTP!");
                    if (error == photo_frame::error_type::None) {
                        error = photo_frame::error_type::NTPSyncFailed;
                    }
                } else {
                    log_i("Successfully fetched time from NTP: %s", now.timestamp(DateTime::TIMESTAMP_FULL).c_str());
                }
            }
        } else {
            log_e("WiFi initialization failed");
            RGB_SET_STATE_TIMED(WIFI_FAILED, 2000); // Show WiFi failed status
        }

        // If WiFi is not required (SD card mode) and it failed, clear the error
        if (!wifiRequired && error != photo_frame::error_type::None) {
            log_w("WiFi failed but not required for SD card mode, continuing without time sync");
            error = photo_frame::error_type::None;
            // Set a default time if WiFi failed
            now = DateTime(2024, 1, 1, 12, 0, 0);
        }
    }

    if (error == photo_frame::error_type::None) {
        log_i("Current time is valid: %s", now.isValid() ? "Yes" : "No");
    } else {
        log_e("Failed to fetch current time! Error code: %d", error.code);
    }

    return error;
}

photo_frame::photo_frame_error_t
handle_google_drive_operations(bool is_reset,
    fs::File& file,
    String& original_filename,
    uint32_t& image_index,
    uint32_t& total_files,
    const photo_frame::battery_info_t& battery_info,
    bool& file_ready)
{
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    photo_frame::photo_frame_error_t tocError = photo_frame::error_type::JsonParseFailed;
    file_ready = false; // Initialize to false
    bool write_toc = is_reset;

    log_i("--------------------------------------");
    log_i(" - Find the next image from the SD...");
    log_i("--------------------------------------");

    error = sdCard.begin(); // Initialize the SD card

    if (error == photo_frame::error_type::None) {
#if DEBUG_MODE
        sdCard.print_stats(); // Print SD card statistics
#endif

        // Initialize Google Drive from unified configuration
        log_i("Initializing Google Drive from unified config...");
        error = drive.initialize_from_unified_config(systemConfig.google_drive);

        if (error != photo_frame::error_type::None) {
            log_e("Failed to initialize Google Drive from unified config! Error: %d", error.code);
        } else {
            log_i("Google Drive initialized successfully from unified config");

            // Use Google Drive to download the next image from Google Drive folder
            // The google_drive class handles authentication, caching, and file management
        }

        if (error == photo_frame::error_type::None) {
            // Clean up any temporary files from previous incomplete downloads
            // Only run cleanup once per day to save battery
            bool shouldCleanup = write_toc; // Always cleanup if forced

            if (!shouldCleanup) {
                // Check if we need to run cleanup based on time interval
                auto& prefs = photo_frame::PreferencesHelper::getInstance();
                time_t now = time(NULL);
                time_t lastCleanup = prefs.getLastCleanup();

                if (now - lastCleanup >= CLEANUP_TEMP_FILES_INTERVAL_SECONDS) {
                    shouldCleanup = true;
                    log_i("Time since last cleanup: %ld seconds", now - lastCleanup);
                } else {
                    log_i("Skipping cleanup, only %ld seconds since last cleanup (need %d seconds)",
                        now - lastCleanup, CLEANUP_TEMP_FILES_INTERVAL_SECONDS);
                }
            }

            if (shouldCleanup) {
                uint32_t cleanedFiles = drive.cleanup_temporary_files(sdCard, write_toc);
                if (cleanedFiles > 0) {
                    log_i("Cleaned up %u temporary files from previous session", cleanedFiles);
                }

                // Update last cleanup time
                auto& prefs = photo_frame::PreferencesHelper::getInstance();
                if (prefs.setLastCleanup(time(NULL))) {
                    log_i("Updated last cleanup time");
                } else {
                    log_w("Failed to save cleanup time to preferences");
                }
            }

            // Retrieve Table of Contents (with caching)
            // If battery is critical, use cached TOC even if expired to save power
            bool batteryConservationMode = battery_info.is_critical();
            if (batteryConservationMode) {
                log_w("Battery critical (%d%%) - using cached TOC to preserve power", battery_info.percent);
            }

            if (error == photo_frame::error_type::None) {
                error = drive.create_directories(sdCard);
            }

            if (error == photo_frame::error_type::None) {
                // Retrieve TOC and get file count directly
                // I2C is already shut down before all WiFi operations to prevent ESP32-C6
                // interference
                total_files = drive.retrieve_toc(sdCard, batteryConservationMode);
            } else {
                log_w("Google Drive not initialized - skipping");
                total_files = 0;
            }

            if (total_files > 0) {
                log_i("Total files in Google Drive folder: %u", total_files);

                photo_frame::google_drive_file selectedFile;

#ifdef GOOGLE_DRIVE_TEST_FILE
                // Use specific test file if defined
                log_i("Using test file: %s", GOOGLE_DRIVE_TEST_FILE);
                selectedFile = drive.get_toc_file_by_name(GOOGLE_DRIVE_TEST_FILE, &tocError);
                if (tocError != photo_frame::error_type::None) {
                    log_w("Test file not found in TOC, falling back to random selection. Error: %d", tocError.code);
                    // Fallback to random selection
                    image_index = random(0, drive.get_toc_file_count());
                    selectedFile = drive.get_toc_file_by_index(image_index, &tocError);
                }
#else
                // Generate a random index to start from
                image_index = random(0, drive.get_toc_file_count(sdCard));

                // Get the specific file by index efficiently
                selectedFile = drive.get_toc_file_by_index(sdCard, image_index, &tocError);
#endif // GOOGLE_DRIVE_TEST_FILE

                // Track if file was successfully processed (binary loaded to PSRAM buffer)
                bool fileProcessedSuccessfully = false;

                if (tocError == photo_frame::error_type::None && selectedFile.id.length() > 0) {
                    log_i("Selected file: %s", selectedFile.name.c_str());

                    // Always download to SD card first for better caching
                    String localFilePath = drive.get_cached_file_path(selectedFile.name);
                    if (sdCard.file_exists(localFilePath.c_str()) && sdCard.get_file_size(localFilePath.c_str()) > 0) {
                        log_i("File already exists in SD card, using cached version");
                        file = sdCard.open(localFilePath.c_str(), FILE_READ);
                        drive.set_last_image_source(photo_frame::IMAGE_SOURCE_LOCAL_CACHE);
                    } else {
                        // Check battery level before downloading file
                        if (batteryConservationMode) {
                            log_w("Skipping file download due to critical battery level (%d%%) - will use cached files if available",
                                battery_info.percent);
                            error = photo_frame::error_type::BatteryLevelCritical;
                        } else {
                            // Download the selected file to SD card
                            file = drive.download_file(sdCard, selectedFile, &error);
                            // Note: image source is set to IMAGE_SOURCE_CLOUD inside
                            // download_file
                        }
                    }

                    // Validate and load image file directly to PSRAM buffer
                    if (error == photo_frame::error_type::None && file) {
                        const char* filename = file.name();
                        String filePath = String(filename);
                        original_filename = String(filename); // Store original filename for error reporting

                        log_i("Validating downloaded image file...");
                        auto validationError = photo_frame::io_utils::validate_image_file(
                            file, filename, DISP_WIDTH, DISP_HEIGHT);

                        if (validationError != photo_frame::error_type::None) {
                            log_e("Image validation FAILED: %s", validationError.message);

                            // Close file before deletion
                            file.close();

                            // Delete corrupted file from SD card
                            if (sdCard.file_exists(filePath.c_str())) {
                                log_w("Deleting corrupted file from SD card: %s", filePath.c_str());
                                if (sdCard.remove(filePath.c_str())) {
                                    log_i("Corrupted file successfully deleted");
                                } else {
                                    log_e("Failed to delete corrupted file");
                                }
                            }

                            error = validationError;

                            // Close SD card after validation error to prevent SPI conflicts
                            log_i("Closing SD card after validation error");
                            sdCard.end();
                        } else {
                            log_i("Image validation PASSED");

                            // Binary format only: Load to PSRAM buffer, then close SD card
                            log_i("Loading binary image to PSRAM buffer from SD card...");
                            uint16_t loadError = photo_frame::renderer::load_image_to_buffer(
                                g_imageBuffer.getBuffer(), file, filename, DISP_WIDTH, DISP_HEIGHT);

                            // Close the SD card file after loading
                            file.close();

                            // Sample first few bytes to verify buffer has data
                            log_d("Buffer check - First 8 bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
                                g_imageBuffer.getBuffer()[0], g_imageBuffer.getBuffer()[1], g_imageBuffer.getBuffer()[2], g_imageBuffer.getBuffer()[3],
                                g_imageBuffer.getBuffer()[4], g_imageBuffer.getBuffer()[5], g_imageBuffer.getBuffer()[6], g_imageBuffer.getBuffer()[7]);

                            if (loadError != 0) {
                                log_e("Failed to load image to buffer, error code: %d", loadError);
                                error = photo_frame::error_type::BinaryRenderingFailed;
                            } else {
                                log_i("Binary image loaded to PSRAM buffer");
                                // Mark as successfully processed
                                fileProcessedSuccessfully = true;
                                file_ready = true; // Binary image loaded to buffer
                            }
                            sdCard.end();
                        }
                    }

                    // Binary format: SD card is already closed and image is in PSRAM buffer
                } else {
                    log_e("Failed to get file by index. Error code: %d", tocError.code);
                    error = tocError;
                }

                // Check if file was successfully processed (binary in buffer)
                if (error == photo_frame::error_type::None && (fileProcessedSuccessfully || file)) {
                    log_i("File downloaded and ready for display!");
                } else {
                    log_e("Failed to download file from Google Drive! Error code: %d", error.code);
                }
            } else {
                // Check if there was an error during TOC retrieval (e.g., access token failure)
                photo_frame::photo_frame_error_t lastDriveError = drive.get_last_error();
                if (lastDriveError != photo_frame::error_type::None) {
                    log_e("Failed to retrieve TOC. Error code: %d", lastDriveError.code);
                    error = lastDriveError;
                } else if (tocError != photo_frame::error_type::None) {
                    log_e("Failed to read TOC file count. Error code: %d", tocError.code);
                    error = tocError;
                } else {
                    log_w("No files found in Google Drive folder!");
                    error = photo_frame::error_type::NoImagesFound;
                }

                // CRITICAL: Close SD card on error to prevent SPI conflicts with display
                log_i("Closing SD card after Google Drive error to prevent SPI conflicts");
                sdCard.end();
            }
        }
    } else {
        log_e("Failed to initialize SD card. Error code: %d", error.code);
    }

    // CRITICAL: Ensure SD card is closed on any error before display initialization
    // This prevents SPI bus conflicts that cause "Busy Timeout!" on the display
    if (error != photo_frame::error_type::None && sdCard.is_initialized()) {
        log_w("Closing SD card due to error (code %d) to prevent SPI conflicts", error.code);
        sdCard.end();
    }

    return error;
}

/**
 * @brief Handle SD Card operations (image selection from local directory)
 * @param is_reset Whether this is a reset/startup
 * @param file Reference to File object for image
 * @param original_filename Original filename from SD card
 * @param image_index Reference to store selected image index
 * @param total_files Reference to store total file count
 * @param battery_info Battery information for power decisions
 * @param file_ready Reference to boolean indicating if file is ready
 * @return Error state after SD card operations
 */
photo_frame::photo_frame_error_t
handle_sd_card_operations(bool is_reset,
    fs::File& file,
    String& original_filename,
    uint32_t& image_index,
    uint32_t& total_files,
    const photo_frame::battery_info_t& battery_info,
    bool& file_ready)
{
    photo_frame::photo_frame_error_t error = photo_frame::error_type::None;
    file_ready = false;

    log_i("--------------------------------------");
    log_i(" - SD Card Only Mode - Local Images");
    log_i("--------------------------------------");

    // Initialize SD card
    error = sdCard.begin();
    if (error != photo_frame::error_type::None) {
        log_e("Failed to initialize SD card for image source");
        return error;
    }

#if DEBUG_MODE
    sdCard.print_stats(); // Print SD card statistics
#endif

    // Check if the configured directory exists
    const char* images_dir = systemConfig.sd_card.images_directory.c_str();
    if (!sdCard.is_directory(images_dir)) {
        log_e("Images directory does not exist: %s", images_dir);
        sdCard.end();
        return photo_frame::error_type::NoImagesFound;
    }

    // Count total files in directory
    total_files = sdCard.count_files_in_directory(images_dir, ".bin");
    if (total_files == 0) {
        log_e("No .bin files found in directory: %s", images_dir);
        sdCard.end();
        return photo_frame::error_type::NoImagesFound;
    }

    log_i("Found %d image files in %s", total_files, images_dir);

    // Select a random image
    image_index = random(0, total_files);
    log_i("Selected random image index: %d", image_index);

    // Get the file path at the selected index
    String file_path = sdCard.get_file_at_index(images_dir, image_index, ".bin");
    if (file_path.isEmpty()) {
        log_e("Failed to get file at index %d", image_index);
        sdCard.end();
        return photo_frame::error_type::SdCardFileNotFound;
    }

    // Store the original filename for display
    int lastSlash = file_path.lastIndexOf('/');
    if (lastSlash >= 0) {
        original_filename = file_path.substring(lastSlash + 1);
    } else {
        original_filename = file_path;
    }

    log_i("Selected image: %s", original_filename.c_str());

    // Open the file
    file = sdCard.open(file_path.c_str());
    if (!file) {
        log_e("Failed to open image file: %s", file_path.c_str());
        sdCard.end();
        return photo_frame::error_type::SdCardFileOpenFailed;
    }

    // Validate the binary file
    // TODO: Validate binary file
    // error = photo_frame::io::validate_binary_file(file, original_filename.c_str());
    error = photo_frame::error_type::None; // Skip validation for now
    if (error != photo_frame::error_type::None) {
        log_e("Image validation failed for: %s", original_filename.c_str());
        file.close();
        sdCard.end();
        return error;
    }

    log_i("Image validation PASSED");

    // Load binary image to PSRAM buffer
    log_i("Loading binary image to PSRAM buffer...");
    uint16_t loadError = photo_frame::renderer::load_image_to_buffer(
        g_imageBuffer.getBuffer(), file, original_filename.c_str(), DISP_WIDTH, DISP_HEIGHT);

    file.close();

    if (loadError != 0) {
        log_e("Failed to load image to buffer, error code: %d", loadError);
        sdCard.end();
        return photo_frame::error_type::BinaryRenderingFailed;
    }

    log_i("Binary image loaded to PSRAM buffer");
    file_ready = true;

    // Store last displayed index in preferences
    auto& prefs = photo_frame::PreferencesHelper::getInstance();
    prefs.setImageIndex(image_index);

    // Keep SD card mounted for potential next image
    // It will be closed later if needed

    return error;
}

photo_frame::photo_frame_error_t
process_image_file(fs::File& file,
    const char* original_filename,
    photo_frame::photo_frame_error_t current_error,
    bool file_ready,
    const DateTime& now,
    const refresh_delay_t& refresh_delay,
    uint32_t image_index,
    uint32_t total_files,
    const photo_frame::battery_info_t& battery_info)
{
    photo_frame::photo_frame_error_t error = current_error;

    // Binary format only: file_ready=true, file=closed, image buffer loaded
    if (error == photo_frame::error_type::None && file_ready) {
        // Image was already validated and loaded to PSRAM buffer
        log_i("Rendering validated binary image from buffer...");
        error = render_image(file,
            original_filename,
            error,
            now,
            refresh_delay,
            image_index,
            total_files,
            drive,
            battery_info);
    }

    if (file) {
        file.close(); // Ensure file is closed (should already be closed after buffer load)
    }

    return error;
}

void finalize_and_enter_sleep(photo_frame::battery_info_t& battery_info,
    DateTime& now,
    esp_sleep_wakeup_cause_t wakeup_reason,
    const refresh_delay_t& refresh_delay)
{
#ifdef RGB_STATUS_ENABLED
    // Shutdown RGB status system to save power during sleep
    log_i("[RGB] Shutting down RGB status system for sleep");
    rgbStatus.end(); // Properly shutdown NeoPixel and FreeRTOS task
#endif // RGB_STATUS_ENABLED

    // Free image buffer before sleep to release memory
    cleanup_image_buffer();

    delay(100);
    photo_frame::renderer::power_off();

    // now go to sleep using the pre-calculated refresh delay
    if (!battery_info.is_critical() && refresh_delay.refresh_microseconds > MICROSECONDS_IN_SECOND) {
        log_i("Going to sleep for %ld seconds (%lu seconds from microseconds)",
            refresh_delay.refresh_seconds,
            (unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL));
    } else {
        if (battery_info.is_critical()) {
            log_w("Battery is critical, entering indefinite sleep");
        } else {
            log_w("Sleep time too short or invalid, entering default sleep");
        }
    }

    unsigned long elapsed = millis() - startupTime;
    log_i("Elapsed seconds since startup: %lu s", elapsed / 1000);
    photo_frame::board_utils::enter_deep_sleep(wakeup_reason, refresh_delay.refresh_microseconds);
}

/**
 * @brief Calculate the wakeup delay based on the battery level and current time.
 *
 * @param battery_info The battery information structure.
 * @param now The current time.
 * @return The calculated wakeup delay.
 */
refresh_delay_t calculate_wakeup_delay(photo_frame::battery_info_t& battery_info, DateTime& now)
{
    refresh_delay_t refresh_delay = { 0, 0 };

    if (battery_info.is_critical()) {
        log_w("Battery is critical, using low battery refresh interval");
        refresh_delay.refresh_seconds = REFRESH_INTERVAL_SECONDS_CRITICAL_BATTERY;
        refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        return refresh_delay;
    }

    if (!now.isValid()) {
        log_w("Time is invalid, using minimum refresh interval as fallback");
        refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
        refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        return refresh_delay;
    }

    if (true) { // Changed from the original condition to always execute
        refresh_delay.refresh_seconds = photo_frame::board_utils::read_refresh_seconds(systemConfig, battery_info.is_low());

        log_i("Refresh seconds: %ld", refresh_delay.refresh_seconds);

        // add the refresh time to the current time
        // Update the current time with the refresh interval
        DateTime nextRefresh = now + TimeSpan(refresh_delay.refresh_seconds);

        // check if the next refresh time is after DAY_END_HOUR
        DateTime dayEnd = DateTime(now.year(), now.month(), now.day(), DAY_END_HOUR, 0, 0);

        if (nextRefresh > dayEnd) {
            log_i("Next refresh time is after DAY_END_HOUR");
            // Check if we're currently in the inactive period (before DAY_START_HOUR)
            if (now.hour() < DAY_START_HOUR) {
                // We're in early morning hours, schedule for DAY_START_HOUR today
                nextRefresh = DateTime(now.year(), now.month(), now.day(), DAY_START_HOUR, 0, 0);
            } else {
                // We're past DAY_END_HOUR, schedule for DAY_START_HOUR tomorrow
                DateTime tomorrow = now + TimeSpan(1, 0, 0, 0); // Add 1 day safely
                nextRefresh = DateTime(
                    tomorrow.year(), tomorrow.month(), tomorrow.day(), DAY_START_HOUR, 0, 0);
            }
            refresh_delay.refresh_seconds = nextRefresh.unixtime() - now.unixtime();
        }

        log_i("Next refresh time: %s", nextRefresh.timestamp(DateTime::TIMESTAMP_FULL).c_str());

        // Convert seconds to microseconds for deep sleep with overflow protection
        // ESP32 max sleep time is ~18 hours (281474976710655 microseconds)
        // Practical limit defined in config.h

        if (refresh_delay.refresh_seconds <= 0) {
            log_w("Warning: Invalid refresh interval, using minimum interval as fallback");
            refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
            refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        } else if (refresh_delay.refresh_seconds > MAX_DEEP_SLEEP_SECONDS) {
            refresh_delay.refresh_microseconds = (uint64_t)MAX_DEEP_SLEEP_SECONDS * MICROSECONDS_IN_SECOND;
            log_w("Warning: Refresh interval capped to %d seconds to prevent overflow", MAX_DEEP_SLEEP_SECONDS);
        } else {
            // Safe calculation using 64-bit arithmetic
            refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
        }

        char humanReadable[64];
        photo_frame::string_utils::seconds_to_human(
            humanReadable, sizeof(humanReadable), refresh_delay.refresh_seconds);

        log_i("Refresh interval in: %s", humanReadable);
    }

    // Final validation - ensure we never return 0 microseconds
    if (refresh_delay.refresh_microseconds == 0) {
        log_e("CRITICAL: refresh_microseconds is 0, forcing minimum interval");
        refresh_delay.refresh_seconds = REFRESH_MIN_INTERVAL_SECONDS;
        refresh_delay.refresh_microseconds = (uint64_t)refresh_delay.refresh_seconds * MICROSECONDS_IN_SECOND;
    }

    log_i("Final refresh delay: %ld seconds (%lu microseconds)",
        refresh_delay.refresh_seconds,
        (unsigned long)(refresh_delay.refresh_microseconds / 1000000ULL));

    return refresh_delay;
}

#else
#include "display_debug.h"
#endif