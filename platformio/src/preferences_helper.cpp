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

#include "preferences_helper.h"

namespace photo_frame {

PreferencesHelper& PreferencesHelper::getInstance() {
    static PreferencesHelper instance;
    return instance;
}

bool PreferencesHelper::beginRead() {
    if (isOpen) {
        end(); // Close if already open
    }

    if (preferences.begin(PREFS_NAMESPACE, true)) { // true = read-only
        isOpen     = true;
        isReadOnly = true;
        return true;
    }
    return false;
}

bool PreferencesHelper::beginWrite() {
    if (isOpen) {
        end(); // Close if already open
    }

    if (preferences.begin(PREFS_NAMESPACE, false)) { // false = read-write
        isOpen     = true;
        isReadOnly = false;
        return true;
    }
    return false;
}

void PreferencesHelper::end() {
    if (isOpen) {
        preferences.end();
        isOpen     = false;
        isReadOnly = false;
    }
}

bool PreferencesHelper::putULong(const char* key, uint32_t value) {
    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putULong(key, value);
    end();

    return written > 0;
}

uint32_t PreferencesHelper::getULong(const char* key, uint32_t defaultValue) {
    if (!beginRead()) {
        return defaultValue;
    }

    uint32_t value = preferences.getULong(key, defaultValue);
    end();

    return value;
}

// ========================================
// APPLICATION-SPECIFIC METHODS
// ========================================

time_t PreferencesHelper::getLastCleanup() { return getULong("last_cleanup", 0); }

bool PreferencesHelper::setLastCleanup(time_t timestamp) {
    return putULong("last_cleanup", timestamp);
}

uint8_t PreferencesHelper::getDisplayRotation() {
    if (!beginRead()) {
        return 0; // Default to 0° if can't read
    }

    uint8_t value = preferences.getUChar("display_rotation", 0);
    end();

    if (value > 3) {
        log_w("Invalid display_rotation %u found in preferences, resetting to 0", value);
        value = 0;
    }

    return value;
}

bool PreferencesHelper::setDisplayRotation(uint8_t rotation) {
    uint8_t clamped = rotation % 4; // Ensure 0-3

    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putUChar("display_rotation", clamped);
    end();

    return written > 0;
}

uint32_t PreferencesHelper::getImageIndex() {
    return getULong("image_index", 0); // Default to 0 if not set
}

bool PreferencesHelper::setImageIndex(uint32_t index) { return putULong("image_index", index); }

#ifdef ENABLE_BT_IMAGE
// ========================================
// BLUETOOTH MODE METHODS
// ========================================

uint8_t PreferencesHelper::getBtRotation() {
    if (!beginRead()) {
        return 0; // Default to 0° (landscape)
    }

    uint8_t value = preferences.getUChar("bt_rotation", 0);
    end();

    // Validate range
    if (value > 3) {
        log_w("[Prefs] Invalid bt_rotation value: %d, using 0", value);
        return 0;
    }

    return value;
}

bool PreferencesHelper::setBtRotation(uint8_t rotation) {
    // Validate input
    if (rotation > 3) {
        log_e("[Prefs] Invalid rotation value: %d (must be 0-3)", rotation);
        return false;
    }

    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putUChar("bt_rotation", rotation);
    end();

    return written > 0;
}

bool PreferencesHelper::getBtFirstBoot() {
    if (!beginRead()) {
        return true; // Default to first boot if can't read
    }

    bool value = preferences.getBool("bt_first_boot", true);
    end();

    return value;
}

bool PreferencesHelper::setBtFirstBoot(bool is_first) {
    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putBool("bt_first_boot", is_first);
    end();

    return written > 0;
}

bool PreferencesHelper::getBtImageAvailable() {
    if (!beginRead()) {
        return false; // Default to no image available
    }

    bool value = preferences.getBool("bt_img_avail", false);
    end();

    return value;
}

bool PreferencesHelper::setBtImageAvailable(bool available) {
    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putBool("bt_img_avail", available);
    end();

    return written > 0;
}

uint16_t PreferencesHelper::getBtLastError() {
    if (!beginRead()) {
        return 0;
    }

    uint16_t value = preferences.getUShort("bt_last_err", 0);
    end();

    return value;
}

bool PreferencesHelper::setBtLastError(uint16_t error_code) {
    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putUShort("bt_last_err", error_code);
    end();

    return written > 0;
}

uint8_t PreferencesHelper::getBtRetryCount() {
    if (!beginRead()) {
        return 0;
    }

    uint8_t value = preferences.getUChar("bt_retry_cnt", 0);
    end();

    return value;
}

bool PreferencesHelper::setBtRetryCount(uint8_t count) {
    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putUChar("bt_retry_cnt", count);
    end();

    return written > 0;
}

bool PreferencesHelper::clearBtPreferences() {
    if (!beginWrite()) {
        return false;
    }

    log_i("[Prefs] Clearing all BT preferences");

    preferences.remove("bt_rotation");
    preferences.remove("bt_first_boot");
    preferences.remove("bt_img_avail");
    preferences.remove("bt_last_err");
    preferences.remove("bt_retry_cnt");

    end();

    return true;
}
#endif // ENABLE_BT_IMAGE

} // namespace photo_frame