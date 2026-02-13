#pragma once

#include "battery_manager.h"
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

  constexpr static char type[] = "board_info";

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
   * @return Rotation in quadrants (0-3) corresponding to 0°, 90°, 180°, 270°
   */
  static uint8_t getDisplayRotation();

  /**
   * @brief Override current display rotation (set by main)
   * @param rotation Rotation in quadrants (0-3) corresponding to 0°, 90°, 180°,
   * 270°
   */
  static void setDisplayRotation(uint8_t rotation);

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
  static void setBatteryInfo(const BatteryInfo &info);

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
   * @brief Set the device's IP address (for GET_CONFIG response)
   * @param ip IP address string (e.g., "192.168.4.1")
   */
  static void setIpAddress(const std::string &ip);

  /**
   * @brief Get the device's IP address
   * @return IP address string (e.g., "192.168.4.1")
   */
  static std::string getIpAddress();

  /**
   * @brief Set the device's IP port (for GET_CONFIG response)
   * @param port IP port number (e.g., 81)
   */
  static void setIpPort(uint16_t port);

  /**
   * @brief Get the device's IP port
   * @return IP port number (e.g., 81)
   */
  static uint16_t getIpPort();

  /**
   * @brief Set the device's SSID (for GET_CONFIG response)
   * @param ssid SSID string (e.g., "MyWiFi")
   */
  static void setSsid(const std::string &ssid);

  /**
   * @brief Get the device's SSID
   * @return SSID string (e.g., "MyWiFi")
   */
  static std::string getSsid();

  /**
   * @brief Get total system info as JSON
   * @return JSON string with all board information
   */
  static String toJson();
};

/**
 * @brief WebSocket error information structure
 * Contains error message and code for reporting issues to clients
 */
struct WSErrorInfo {
  String message;
  uint16_t code;
  constexpr static char type[] = "error";
  WSErrorInfo(const String &msg = "", uint16_t c = 0) : message(msg), code(c) {}

  String toJson() const;
};

struct WSChunkAck {
  uint32_t received;
  constexpr static char type[] = "chunk_ack";
  WSChunkAck(uint32_t r = 0) : received(r) {}

  String toJson() const;
};

struct WSReadyInfo {
  constexpr static char type[] = "ready";
  uint8_t sessionId;
  WSReadyInfo(uint8_t id = 0) : sessionId(id) {}

  String toJson() const;
};

struct WSFinalResponse {
  constexpr static char type[] = "final_response";
  bool success;
  String message;
  String filepath;
  WSFinalResponse(bool s = true, const String &msg = "", const String &path = "") : success(s), message(msg), filepath(path) {}

  String toJson() const;
};

struct WSMessageInfo {
  String message;
  String type;
  WSMessageInfo(const String &msg, const String &t) : message(msg), type(t) {}

  String toJson() const;
};

} // namespace ws
} // namespace photo_frame
