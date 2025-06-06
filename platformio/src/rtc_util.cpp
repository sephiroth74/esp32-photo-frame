#include "rtc_util.h"
#include "config.h"
#include <WiFi.h>

namespace photo_frame {

DateTime fetch_remote_datetime(SDCard& sdCard) {
    Serial.println(F("Fetching time from WiFi..."));
    DateTime result = DateTime((uint32_t)0);

    fs::File file   = sdCard.open(WIFI_FILENAME, FILE_READ); // Open the file to write the time
    if (!file) {
        Serial.println(F("Failed to open WiFi credentials file!"));
        Serial.println(F("Please check if the file exists on the SD card."));
        return result;
    }

    String wifi_ssid     = file.readStringUntil('\n');
    String wifi_password = file.readStringUntil('\n');

    wifi_ssid.trim();
    wifi_password.trim();

    file.close();

    if (wifi_ssid.isEmpty() || wifi_password.isEmpty()) {
        Serial.println(F("WiFi credentials are empty!"));
        Serial.println(F("Please check the WiFi credentials file on the SD card."));
        return result;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());

    unsigned long timeout         = millis() + WIFI_CONNECT_TIMEOUT;
    wl_status_t connection_status = WiFi.status();
    while ((connection_status != WL_CONNECTED) && (millis() < timeout)) {
        Serial.print(".");
        delay(200);
        connection_status = WiFi.status();
    }
    Serial.println();

    if (connection_status != WL_CONNECTED) {
        Serial.println(F("Failed to connect to WiFi!"));
        Serial.println(F("Please check your WiFi credentials."));
        return result;
    } else {
        Serial.println(F("Connected to WiFi!"));
    }

    // acquire the time from the NTP server
    configTzTime(TIMEZONE, NTP_SERVER1, NTP_SERVER2);

    // wait for the time to be set
    Serial.print(F("Waiting for time."));
    while (time(nullptr) < 1000000000) {
        Serial.print(F("."));
        delay(1000);
    }
    Serial.println(F("done."));

    // Set the RTC to the time we just set
    // get the local time

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, NTP_TIMEOUT)) {
        Serial.println(F("Failed to obtain time from NTP server!"));
        return result;
    }

    result = DateTime(timeinfo.tm_year + 1900,
                      timeinfo.tm_mon + 1,
                      timeinfo.tm_mday,
                      timeinfo.tm_hour,
                      timeinfo.tm_min,
                      timeinfo.tm_sec);

    WiFi.disconnect();
    return result;
} // featch_remote_datetime

DateTime fetch_datetime(SDCard& sdCard) {
    Serial.println("Initializing RTC...");

    DateTime now;
    RTC_DS3231 rtc;

    pinMode(RTC_POWER_PIN, OUTPUT);
    digitalWrite(RTC_POWER_PIN, HIGH); // Power on the RTC
    delay(1000);                       // Wait for RTC to power up

    Wire1.begin(RTC_SDA_PIN /* SDA */, RTC_SCL_PIN /* SCL */);
    if (!rtc.begin(&Wire1)) {
        Serial.println(F("Couldn't find RTC"));
        now = fetch_remote_datetime(sdCard);
        if (!now.isValid()) {
            Serial.println(F("Failed to fetch time from WiFi!"));
        }

    } else {
        rtc.disable32K();
        if (rtc.lostPower()) {
            // Set the time to a default value if the RTC lost power
            Serial.println(F("RTC lost power, setting the time!"));
            now = fetch_remote_datetime(sdCard);
            if (!now.isValid()) {
                Serial.println(F("Failed to fetch time from WiFi!"));
            } else {
                rtc.adjust(now);
            }
        } else {
            Serial.println(F("RTC is running!"));
            now = rtc.now();
        }
    }

    Wire1.end();
    digitalWrite(RTC_POWER_PIN, LOW); // Power off the RTC
    return now;
}
} // namespace photo_frame