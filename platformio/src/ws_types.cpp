#include "ws_types.h"
#include "config.h"
#include "renderer.h"
#include <ArduinoJson.h>
#include <esp_chip_info.h>

namespace photo_frame {
namespace ws {

static bool s_hasBatteryInfo = false;
static BatteryInfo s_batteryInfo;
static bool s_hasDisplayRotation  = false;
static uint16_t s_displayRotation = 0;

uint32_t BoardInfo::getBinaryFileSize() {
    uint32_t size = PFR1_MAX_IMAGE_SIZE_FOR(DISP_WIDTH, DISP_HEIGHT);
    return size;
}

String BoardInfo::getBoardModel() {
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    switch (chip_info.model) {
    case CHIP_ESP32S3: return "ESP32-S3";
    case CHIP_ESP32:   return "ESP32";
    case CHIP_ESP32S2: return "ESP32-S2";
    case CHIP_ESP32C3: return "ESP32-C3";
#ifdef CHIP_ESP32H2
    case CHIP_ESP32H2: return "ESP32-H2";
#endif
    default: return "ESP32 (Unknown)";
    }
}

uint32_t BoardInfo::getFlashSize() { return ESP.getFlashChipSize(); }

String BoardInfo::getFlashSizeStr() {
    uint32_t flashSize = getFlashSize();

    if (flashSize >= 1024 * 1024) {
        return String(flashSize / (1024 * 1024)) + "MB";
    } else if (flashSize >= 1024) {
        return String(flashSize / 1024) + "KB";
    } else {
        return String(flashSize) + "B";
    }
}

String BoardInfo::getDisplayType() {
#ifdef DISP_6C
    return "6C";
#elif defined(DISP_BW)
    return "BW";
#else
    return "Unknown";
#endif
}

BoardInfo::DisplaySize BoardInfo::getDisplaySize() { return DisplaySize{DISP_WIDTH, DISP_HEIGHT}; }

uint8_t BoardInfo::getDisplayRotation() {
    if (s_hasDisplayRotation) {
        return s_displayRotation;
    }

#ifdef DEFAULT_ORIENTATION
    return DEFAULT_ORIENTATION;
#else
    return 0;
#endif
}

void BoardInfo::setDisplayRotation(uint8_t rotation) {
    s_displayRotation    = rotation;
    s_hasDisplayRotation = true;
}

int8_t BoardInfo::getBatteryLevel() {
    if (!s_hasBatteryInfo) {
        return -1;
    }

    return static_cast<int8_t>(s_batteryInfo.percent);
}

int32_t BoardInfo::getBatteryVoltage() {
    if (!s_hasBatteryInfo) {
        return -1;
    }

    return static_cast<int32_t>(s_batteryInfo.millivolts);
}

void BoardInfo::setBatteryInfo(const BatteryInfo& info) {
    s_batteryInfo    = info;
    s_hasBatteryInfo = true;
}

void BoardInfo::clearBatteryInfo() { s_hasBatteryInfo = false; }

String BoardInfo::toJson() {
    // Create JSON document
    StaticJsonDocument<512> doc;

    DisplaySize displaySize = getDisplaySize();

    doc["board"]            = getBoardModel();
    doc["flash_size"]       = getFlashSizeStr();
    doc["flash_size_bytes"] = getFlashSize();
    doc["display_type"]     = getDisplayType();
    doc["display_width"]    = displaySize.width;
    doc["display_height"]   = displaySize.height;
    doc["display_rotation"] = getDisplayRotation();
    doc["server_version"]   = WS_SERVER_VERSION;
    doc["file_version"]     = PFR1_FILE_VERSION;
    doc["binary_file_size"] = getBinaryFileSize();

    int8_t battery          = getBatteryLevel();
    if (battery >= 0) {
        doc["battery_level"] = battery;
    } else {
        doc["battery_level"] = nullptr;
    }

    int32_t voltage = getBatteryVoltage();
    if (voltage >= 0) {
        doc["battery_voltage_mv"] = voltage;
    } else {
        doc["battery_voltage_mv"] = nullptr;
    }

    // Serialize to string
    String output;
    serializeJson(doc, output);

    log_d("[BoardInfo] Config JSON: %s", output.c_str());

    return output;
}

// ===============================================
// WSErrorInfo Implementation
// ===============================================

String WSErrorInfo::toJson() const {
    StaticJsonDocument<256> doc;
    doc["type"]    = type;
    doc["code"]    = code;
    doc["message"] = message;

    String output;
    serializeJson(doc, output);
    return output;
}

// ===============================================
// WSChunkAck Implementation
// ===============================================

String WSChunkAck::toJson() const {
    StaticJsonDocument<256> doc;
    doc["type"]     = type;
    doc["received"] = received;

    String output;
    serializeJson(doc, output);
    return output;
}

// ===============================================
// WSReadyInfo Implementation
// ===============================================

String WSReadyInfo::toJson() const {
    StaticJsonDocument<256> doc;
    doc["type"]       = type;
    doc["session_id"] = sessionId;

    String output;
    serializeJson(doc, output);
    return output;
}

// ===============================================
// WSSuccessInfo Implementation
// ===============================================

String WSSuccessInfo::toJson() const {
    StaticJsonDocument<256> doc;
    doc["type"]    = type;
    doc["message"] = message;

    String output;
    serializeJson(doc, output);
    return output;
}

// ===============================================
// WSMessageInfo Implementation
// ===============================================

String WSMessageInfo::toJson() const {
    StaticJsonDocument<256> doc;
    doc["type"]    = type;
    doc["message"] = message;

    String output;
    serializeJson(doc, output);
    return output;
}

} // namespace ws
} // namespace photo_frame
