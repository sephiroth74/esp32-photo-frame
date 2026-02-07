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
 * prefs.putString("last_image", "image_123.pfr1");
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
     * @brief Get the timestamp of the last displayed image
     */
    time_t getLastImageTimestamp();

    /**
     * @brief Set the timestamp of the last displayed image
     */
    bool setLastImageTimestamp(time_t timestamp);

    /**
     * @brief Get the display rotation setting (0-3)
     *
     * Stored preference key: "display_rotation". Returns values 0-3 where
     * 0=0°, 1=90°, 2=180°, 3=270°. Defaults to 0 on missing/invalid data.
     */
    uint8_t getDisplayRotation();

    /**
     * @brief Persist the display rotation setting (0-3)
     *
     * @param rotation rotation value (clamped to 0-3)
     * @return true if successfully stored, false on error
     */
    bool setDisplayRotation(uint8_t rotation);

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
    bool isOpen = false; ///< Track if preferences are currently open
    bool isReadOnly = false; ///< Track if opened in read-only mode
};

} // namespace photo_frame

// Convenience macros for easy access
#define PREFS() photo_frame::PreferencesHelper::getInstance()

#endif // __PREFERENCES_HELPER_H__