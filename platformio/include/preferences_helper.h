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
#include <Preferences.h>
#include <Arduino.h>

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
 * prefs.putULong("ota_last_check", time(NULL));
 * prefs.putInt("refresh_interval", 3600);
 * prefs.putString("last_image", "image_123.bin");
 *
 * // Retrieve values with defaults
 * time_t last_check = prefs.getULong("ota_last_check", 0);
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
    PreferencesHelper(const PreferencesHelper&) = delete;
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
     * @brief Get the timestamp of the last OTA update check
     *
     * This method encapsulates the "ota_last_check" preference key
     * and provides a type-safe way to retrieve the OTA check timestamp.
     *
     * @return Unix timestamp of last OTA check, or 0 if never performed
     */
    time_t getOtaLastCheck();

    /**
     * @brief Set the timestamp of the last OTA update check
     *
     * This method encapsulates the "ota_last_check" preference key
     * and provides a type-safe way to store the OTA check timestamp.
     *
     * @param timestamp Unix timestamp of the OTA check operation
     * @return true if successfully stored, false on error
     */
    bool setOtaLastCheck(time_t timestamp);

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

    Preferences preferences;          ///< ESP32 Preferences instance
    bool isOpen = false;             ///< Track if preferences are currently open
    bool isReadOnly = false;         ///< Track if opened in read-only mode
};

} // namespace photo_frame

// Convenience macros for easy access
#define PREFS() photo_frame::PreferencesHelper::getInstance()

#endif // __PREFERENCES_HELPER_H__