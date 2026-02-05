#pragma once

#include "battery.h"
#include <Arduino.h>

namespace photo_frame {
namespace ws {

/**
 * @brief Board information and configuration
 *
 * Collects and provides system information about the ESP32 board,
 * display, and current configuration state.
 */
class BoardInfo {
  public:
    struct DisplaySize {
        uint16_t width;
        uint16_t height;
    };

    /**
     * @brief Get board model name
     * @return Board model string
     */
    static String getBoardModel();

    /**
     * @brief Get total flash size in bytes
     * @return Flash size
     */
    static uint32_t getFlashSize();

    /**
     * @brief Get flash size as human readable string
     * @return Flash size string (e.g., "16MB")
     */
    static String getFlashSizeStr();

    /**
     * @brief Get display size (width, height)
     * @return Display size struct
     */
    static DisplaySize getDisplaySize();

    /**
     * @brief Get display type
     * @return Display type string ("6C" or "BW")
     */
    static String getDisplayType();

    /**
     * @brief Get current display rotation
     * @return Rotation (0, 90, 180, 270)
     */
    static uint16_t getDisplayRotation();

    /**
     * @brief Override current display rotation (set by main)
     * @param rotation Rotation in degrees (0, 90, 180, 270)
     */
    static void setDisplayRotation(uint16_t rotation);

    /**
     * @brief Get battery level percentage
     * @return Battery percentage (0-100), or -1 if unavailable
     */
    static int8_t getBatteryLevel();

    /**
     * @brief Get battery voltage in millivolts
     * @return Battery voltage, or -1 if unavailable
     */
    static int32_t getBatteryVoltage();

    /**
     * @brief Override current battery info (set by main)
     * @param info Battery info structure
     */
    static void setBatteryInfo(const battery_info_t& info);

    /**
     * @brief Clear battery info override
     */
    static void clearBatteryInfo();

    /**
     * @brief Get size of the compiled binary file
     * @return Binary file size in bytes
     */
    static uint32_t getBinaryFileSize();

    /**
     * @brief Get total system info as JSON
     * @return JSON string with all board information
     */
    static String toJson();
};

} // namespace ws
} // namespace photo_frame
