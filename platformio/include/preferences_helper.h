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

#ifndef __PREFERENCES_HELPER_H__
#define __PREFERENCES_HELPER_H__

#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

namespace photo_frame {

/**
 * @brief Centralized preferences management helper class
 *
 * This class provides a centralized way to access ESP32 preferences
 * across the entire application. It ensures consistent namespace usage
 * and provides type-safe methods for common preference operations.
 *
 * Key features:
 * - Single instance shared across all modules
 * - Consistent namespace usage (PREFS_NAMESPACE from config.h)
 * - Automatic error handling with fallback values
 * - Type-safe getter/setter methods
 * - Proper resource management (automatic begin/end)
 *
 * Usage example:
 * @code
 * // Get singleton instance
 * auto& prefs = PreferencesHelper::getInstance();
 *
 * // Store values
 * prefs.putULong("last_cleanup", time(NULL));
 * prefs.putInt("refresh_interval", 3600);
 * prefs.putString("last_image", "image_123.bin");
 *
 * // Retrieve values with defaults
 * time_t last_cleanup = prefs.getULong("last_cleanup", 0);
 * int interval = prefs.getInt("refresh_interval", 1800);
 * String image = prefs.getString("last_image", "");
 * @endcode
 */
class PreferencesHelper {
  public:
    /**
     * @brief Get singleton instance of PreferencesHelper
     *
     * Returns the single instance of PreferencesHelper used throughout
     * the application. This ensures consistent namespace usage and
     * prevents multiple preferences instances.
     *
     * @return Reference to the singleton PreferencesHelper instance
     */
    static PreferencesHelper& getInstance();

    // Delete copy constructor and assignment operator to enforce singleton
    PreferencesHelper(const PreferencesHelper&)            = delete;
    PreferencesHelper& operator=(const PreferencesHelper&) = delete;

    // ========================================
    // APPLICATION-SPECIFIC METHODS
    // ========================================

    /**
     * @brief Get the timestamp of the last cleanup operation
     *
     * This method encapsulates the "last_cleanup" preference key
     * and provides a type-safe way to retrieve the cleanup timestamp.
     *
     * @return Unix timestamp of last cleanup, or 0 if never performed
     */
    time_t getLastCleanup();

    /**
     * @brief Set the timestamp of the last cleanup operation
     *
     * This method encapsulates the "last_cleanup" preference key
     * and provides a type-safe way to store the cleanup timestamp.
     *
     * @param timestamp Unix timestamp of the cleanup operation
     * @return true if successfully stored, false on error
     */
    bool setLastCleanup(time_t timestamp);

    /**
     * @brief Get the display portrait mode setting
     *
     * This method retrieves the stored portrait_mode preference
     * which determines if the display should be in portrait orientation.
     *
     * @return true for portrait, false for landscape (default)
     */
    bool getPortraitMode();

    /**
     * @brief Set the display portrait mode setting
     *
     * This method stores the portrait_mode preference
     * which determines if the display should be in portrait orientation.
     *
     * @param portrait_mode true for portrait, false for landscape
     * @return true if successfully stored, false on error
     */
    bool setPortraitMode(bool portrait_mode);

    /**
     * @brief Get the last displayed image index
     *
     * This method retrieves the index of the last displayed image.
     *
     * @return Image index, or 0 if not set
     */
    uint32_t getImageIndex();

    /**
     * @brief Set the last displayed image index
     *
     * This method stores the index of the last displayed image.
     *
     * @param index The image index to store
     * @return true if successfully stored, false on error
     */
    bool setImageIndex(uint32_t index);

#ifdef ENABLE_BT_IMAGE
    // ========================================
    // BLUETOOTH MODE PREFERENCES
    // ========================================

    /**
     * @brief Get display rotation for Bluetooth mode
     *
     * Returns the saved rotation value (0-3) for the display.
     * 0=0° (landscape), 1=90° (portrait), 2=180° (inverted landscape), 3=270° (inverted portrait)
     *
     * @return Rotation value (0-3), defaults to 0
     */
    uint8_t getBtRotation();

    /**
     * @brief Set display rotation for Bluetooth mode
     *
     * Stores the rotation value for the display.
     *
     * @param rotation Rotation value (0-3)
     * @return true if successfully stored, false on error
     */
    bool setBtRotation(uint8_t rotation);

    /**
     * @brief Check if this is the first boot in BT mode
     *
     * Returns true if no BT image has been received yet.
     *
     * @return true if first boot, false otherwise
     */
    bool getBtFirstBoot();

    /**
     * @brief Set first boot flag for BT mode
     *
     * @param is_first true if this is first boot
     * @return true if successfully stored, false on error
     */
    bool setBtFirstBoot(bool is_first);

    /**
     * @brief Check if a valid BT image is available
     *
     * Returns true if a fallback image exists on SD card.
     *
     * @return true if image available, false otherwise
     */
    bool getBtImageAvailable();

    /**
     * @brief Set image available flag for BT mode
     *
     * @param available true if image is available
     * @return true if successfully stored, false on error
     */
    bool setBtImageAvailable(bool available);

    /**
     * @brief Get last BT error code
     *
     * Returns the error code from the last BT operation.
     *
     * @return Error code, or 0 if no error
     */
    uint16_t getBtLastError();

    /**
     * @brief Set last BT error code
     *
     * @param error_code Error code to store
     * @return true if successfully stored, false on error
     */
    bool setBtLastError(uint16_t error_code);

    /**
     * @brief Get BT retry count
     *
     * Returns the number of retries attempted for current operation.
     *
     * @return Retry count, or 0 if none
     */
    uint8_t getBtRetryCount();

    /**
     * @brief Set BT retry count
     *
     * @param count Retry count to store
     * @return true if successfully stored, false on error
     */
    bool setBtRetryCount(uint8_t count);

    /**
     * @brief Clear all Bluetooth mode preferences
     *
     * Resets all BT-related preferences to defaults.
     * Used during reset/initialization.
     *
     * @return true if successfully cleared, false on error
     */
    bool clearBtPreferences();
#endif // ENABLE_BT_IMAGE

  private:
    /**
     * @brief Private constructor for singleton pattern
     */
    PreferencesHelper() = default;

    /**
     * @brief Private destructor
     */
    ~PreferencesHelper() = default;

    /**
     * @brief Open preferences for reading
     *
     * @return true if successfully opened, false on error
     */
    bool beginRead();

    /**
     * @brief Open preferences for writing
     *
     * @return true if successfully opened, false on error
     */
    bool beginWrite();

    /**
     * @brief Close preferences
     */
    void end();

    /**
     * @brief Store an unsigned long value in preferences
     *
     * @param key The preference key name
     * @param value The value to store
     * @return true if successfully stored, false on error
     */
    bool putULong(const char* key, uint32_t value);

    /**
     * @brief Retrieve an unsigned long value from preferences
     *
     * @param key The preference key name
     * @param defaultValue Default value if key doesn't exist
     * @return The stored value or defaultValue if not found
     */
    uint32_t getULong(const char* key, uint32_t defaultValue = 0);

    Preferences preferences; ///< ESP32 Preferences instance
    bool isOpen     = false; ///< Track if preferences are currently open
    bool isReadOnly = false; ///< Track if opened in read-only mode
};

} // namespace photo_frame

// Convenience macros for easy access
#define PREFS() photo_frame::PreferencesHelper::getInstance()

#endif // __PREFERENCES_HELPER_H__