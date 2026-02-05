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
        return DEFAULT_ORIENTATION;
    }

    uint8_t value = preferences.getUChar("disp_rotation", DEFAULT_ORIENTATION);
    end();

    if (value > 3) {
        log_w("Invalid display_rotation %u found in preferences, resetting to 0", value);
        value = DEFAULT_ORIENTATION;
    }

    return value;
}

bool PreferencesHelper::setDisplayRotation(uint8_t rotation) {
    uint8_t clamped = rotation % 4; // Ensure 0-3

    if (!beginWrite()) {
        return false;
    }

    size_t written = preferences.putUChar("disp_rotation", clamped);
    end();

    return written > 0;
}

uint32_t PreferencesHelper::getImageIndex() {
    return getULong("image_index", 0); // Default to 0 if not set
}

bool PreferencesHelper::setImageIndex(uint32_t index) { return putULong("image_index", index); }

time_t PreferencesHelper::getLastImageTimestamp() {
    return static_cast<time_t>(getULong("last_image_ts", 0));
}

bool PreferencesHelper::setLastImageTimestamp(time_t timestamp) {
    return putULong("last_image_ts", static_cast<uint32_t>(timestamp));
}

} // namespace photo_frame