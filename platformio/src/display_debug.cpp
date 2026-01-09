// // Display Debug Mode - Comprehensive Hardware Testing Suite
// // This file runs instead of main.cpp when ENABLE_DISPLAY_DIAGNOSTIC is defined
// //
// // IMPORTANT NOTES (v0.11.0):
// // - testProgmemImage() is DISABLED: requires test_image.h (24MB file removed from repository)
// // - All other diagnostic tests remain fully functional
// // - Use gallery test with SD card images for image rendering validation
// //
// // Available Tests:
// // - Battery status test (MAX1704X or analog reading)
// // - Google Drive integration test (WiFi/NTP/OAuth/file download)
// // - Potentiometer test (continuous reading with real-time display)
// // - Gallery test (BMP and BIN file support from SD card)
// // - Stress test (Floyd-Steinberg dithering for B/W displays)
// // - Hardware failure diagnostics (washout pattern detection)

// #ifdef ENABLE_DISPLAY_DIAGNOSTIC

// #include <Arduino.h>
// #include "config.h"

// #include <driver/adc.h>
// #include <driver/rtc_io.h>
// #include <GxEPD2_7C.h>
// #include <WiFi.h>

// #ifdef SD_USE_SPI
// #include <SD.h>
// #define SD_CARD SD
// #else
// #include <SD_MMC.h>
// #define SD_CARD SD_MMC // Use SDIO SD_MMC library
// #endif

// #include <SPI.h>
// #include <WiFi.h>
// #include <vector>

// #include "bitmaps/Bitmaps7c800x480.h"
// #include "test_photo_6c_portrait.h"

// #define MICROSECONDS_IN_SECOND 1000000

// #ifdef ORIENTATION_PORTRAIT
// #define DISP_WIDTH 480
// #define DISP_HEIGHT 800
// #else
// #define DISP_WIDTH 800
// #define DISP_HEIGHT 480
// #endif

// #define HW_WIDTH 800
// #define HW_HEIGHT 480

// #define GxEPD2_DISPLAY_CLASS GxEPD2_7C
// #define GxEPD2_DRIVER_CLASS GxEPD2_730c_GDEP073E01
// #define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.

// #define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE) / (EPD::WIDTH / 2))
// GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display2(GxEPD2_DRIVER_CLASS(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

// RTC_DATA_ATTR int rtcCounter = 0;

// namespace display_debug {

// // Forward declarations
// #if defined(USE_HSPI_FOR_SD) && defined(SD_USE_SPI)
// SPIClass hspi(HSPI);
// #endif

// static bool displayInitialized = false;
// static bool sdCardInitialized = false;

// // Static image buffer for Mode 1 format (allocated once in PSRAM)
// static uint8_t* g_image_buffer = nullptr;
// static const size_t IMAGE_BUFFER_SIZE = DISP_WIDTH * DISP_HEIGHT; // 384,000 bytes for 800x480

// bool init_image_buffer()
// {
//     if (g_image_buffer != nullptr) {
//         log_w("Image buffer already initialized");
//         return true;
//     }

//     log_i("Initializing PSRAM image buffer...");
//     log_i("Buffer size: %u bytes (%.2f KB)", IMAGE_BUFFER_SIZE, IMAGE_BUFFER_SIZE / 1024.0f);

// #if CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC
//     // Try to allocate in PSRAM first
//     g_image_buffer = (uint8_t*)heap_caps_malloc(IMAGE_BUFFER_SIZE, MALLOC_CAP_SPIRAM);

//     if (g_image_buffer != nullptr) {
//         log_i("Successfully allocated %u bytes in PSRAM for image buffer", IMAGE_BUFFER_SIZE);
//         log_i("PSRAM free after allocation: %u bytes", ESP.getFreePsram());
//     } else {
//         log_w("Failed to allocate in PSRAM, falling back to regular heap");
//         g_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
//     }
// #else
//     // No PSRAM available, use regular heap
//     log_w("PSRAM not available, using regular heap");
//     g_image_buffer = (uint8_t*)malloc(IMAGE_BUFFER_SIZE);
// #endif

//     if (g_image_buffer == nullptr) {
//         log_e("[display_debug] CRITICAL: Failed to allocate image buffer!");
//         log_e("[display_debug] Free heap: %u bytes", ESP.getFreeHeap());
//         log_e("[display_debug] Cannot continue without image buffer");
//         return false;
//     } else {
//         log_i("[display_debug] Image buffer initialized at address: %p", g_image_buffer);
//         return true;
//     }
// }

// void cleanup_image_buffer()
// {
//     if (g_image_buffer != nullptr) {
//         log_i("[display_debug] Freeing image buffer at address: %p", g_image_buffer);
//         free(g_image_buffer);
//         g_image_buffer = nullptr;
//         log_i("[display_debug] Image buffer freed");
//     }
// }

// bool ensureDisplayInitialized()
// {
//     log_i("ensureDisplayInitialized");

//     if (!displayInitialized) {
//         if (!init_image_buffer()) {
//             return false;
//         }

//         SPI.begin(EPD_SCK_PIN, -1, EPD_MOSI_PIN, EPD_CS_PIN);
//         // display2.init(115200);
//         display2.init(115200, true, 30, false);
//         delay(500);

//         display2.setRotation(0);
//         display2.setFullWindow();

//         displayInitialized = true;
//         log_d("Display initialized");
//     }
//     return true;
// }

// void closeDisplay()
// {
//     log_i("closeDisplay");
//     if (displayInitialized) {
//         display2.powerOff();
//         displayInitialized = false;
//     }
// }

// bool ensureSDCardInitialized()
// {
//     log_i("ensureSDCardInitialized");

//     if (!sdCardInitialized) {

// #ifdef SD_USE_SPI
//         log_v("SD_USE_SPI");
// #ifdef USE_HSPI_FOR_SD
//         log_v("USE_HSPI_FOR_SD");
//         hspi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
//         if (!SD_CARD.begin(SD_CS_PIN, hspi)) {
//             log_e("Failed to initialize SD card");
//             hspi.end();
//             return false;
//         }
// #else
//         SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
//         if (!SD_CARD.begin(SD_CS_PIN, SPI)) {
//             log_e("Failed to initialize SD card");
//             SPI.end();
//             return false;
//         }
// #endif // USE_HSPI_FOR_SD
// #else
//         SD_CARD.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);
//         if (!SD_CARD.begin("/sdcard", false, true)) {
//             log_e("Failed to initialize SD_MM");
//             return false;
//         }
// #endif // SD_USE_SPI
//         sdCardInitialized = true;
//     }
//     return true;
// }

// void printSdCardStats()
// {
//     if (!sdCardInitialized) {
//         log_w("not initialized.");
//         return;
//     }

//     sdcard_type_t cardType = SD_CARD.cardType();

//     const char* card_type_str;
//     switch (cardType) {
//     case CARD_MMC:
//         card_type_str = "MMC";
//         break;
//     case CARD_SD:
//         card_type_str = "SDSC";
//         break;
//     case CARD_SDHC:
//         card_type_str = "SDHC";
//         break;
//     case CARD_UNKNOWN:
//         card_type_str = "Unknown";
//         break;
//     case CARD_NONE:
//         card_type_str = "No SD card attached!";
//         break;
//     default:
//         card_type_str = "Unknown card type!";
//         break;
//     }
//     log_v("Card Type: %s", card_type_str);
//     log_v("Card Size: %llu MB", SD_CARD.cardSize() / (1024 * 1024));
//     log_v("Total Size: %llu MB", SD_CARD.totalBytes() / (1024 * 1024));
//     log_v("Used Size: %llu MB", SD_CARD.usedBytes() / (1024 * 1024));
// } // end printStats

// void closeSDCard()
// {
//     log_i("closeSDCard");
//     if (sdCardInitialized) {
//         SD_CARD.end();

// #if defined(SD_USE_SPI) && defined(USE_HSPI_FOR_SD)
//         hspi.end();
// #elif defined(SD_USE_SPI)
//         SPI.end();
// #endif
//         sdCardInitialized = false;
//     }
// }

// bool ensureWiFiInitialized()
// {
//     log_i("ensureWifiInitialized");

//     unsigned long timeout = millis() + 10000;
//     const char ssid[] = "FiOS-RS1LV";
//     const char password[] = "airs900pop4456chew";
//     wl_status_t connection_status = WiFi.status();

//     log_v("connection status: %d", connection_status);

//     if (connection_status != WL_CONNECTED) {
//         log_v("Connecting to %s", ssid);

//         String macAddress = WiFi.macAddress();
//         log_d("Current mac address: %s", macAddress.c_str());

//         WiFi.mode(WIFI_STA);
//         WiFi.begin(ssid, password);

//         // Wait for connection with retry logic
//         while (millis() < timeout) {
//             Serial.print(".");
//             connection_status = WiFi.status();

//             if (connection_status == WL_CONNECTED) {
//                 log_d("Connected successfully!");
//                 return true;
//             }

//             delay(500);
//         }

//         // Timeout reached
//         log_w("Connection timeout!");
//         WiFi.disconnect(false);
//         return false;
//     } else {
//         log_v("Already connected!");
//         return true;
//     }
// }

// void closeWiFi()
// {
//     log_i("closeWiFi");
//     WiFi.disconnect(false);
// }

// // ============================================================================
// // Display Tests
// // ============================================================================

// void writeImageBufferOnDisplay()
// {
//     log_d("Setting full window...");
//     display2.setFullWindow();

//     // log_d("Writing screen buffer");
//     // display2.writeScreenBuffer();

//     log_d("Clearing screen");
//     display2.clearScreen();

//     log_v("Creating canvas for landscape overlay (image already rotated)...");

//     GFXcanvas8 landscapeCanvas(HW_WIDTH, HW_HEIGHT, false);
//     uint8_t** landscapeBufferPtr = (uint8_t**)((uint8_t*)&landscapeCanvas + sizeof(Adafruit_GFX));
//     *landscapeBufferPtr = g_image_buffer;

//     log_v("Drawing statusbar on RIGHT side of landscape...");
//     landscapeCanvas.fillRect(HW_WIDTH - 16, 0, 16, HW_HEIGHT, 0xFF);
//     landscapeCanvas.setRotation(1);

//     log_d("Writing pre-rotated image with overlays to display...");
//     display2.epd2.writeDemoBitmap(g_image_buffer, 0, 0, 0, HW_WIDTH, HW_HEIGHT, 1, false, false);

//     // Refresh display
//     log_d("Refreshing display...");
//     display2.refresh();
// }

// bool testPortraiImageFromSdCard()
// {
//     log_i("========================================");
//     log_i("Portrait Image Test (From SD Card)");
//     log_i("========================================");

//     if (!ensureSDCardInitialized()) {
//         log_e("Failed to initialize Sd Card!");
//         return false;
//     }

//     init_image_buffer();

//     const char images_directory[] = "/6c/bin";

//     File root = SD_CARD.open(images_directory, FILE_READ);
//     if (!root) {
//         log_e("Failed to open '%s' from Sd Card", images_directory);
//         closeSDCard();
//         return false;
//     }

//     if (!root.isDirectory()) {
//         log_e("'%s' is not a directory!", images_directory);
//         root.close();
//         closeSDCard();
//         return false;
//     }

//     uint32_t fileCount = 0;
//     File entry = root.openNextFile();

//     // Count valid .bin files
//     while (entry) {
//         if (!entry.isDirectory()) {
//             String fileName = String(entry.name());
//             if (fileName.endsWith(".bin") && !fileName.startsWith(".")) {
//                 fileCount++;
//                 log_v("Found file #%lu: %s (%lu bytes)", fileCount, entry.name(), entry.size());
//             }
//         }
//         entry.close();
//         entry = root.openNextFile();
//     }
//     root.close();

//     log_v("Total .bin file count inside %s is %lu", images_directory, fileCount);

//     if (fileCount < 1) {
//         log_w("Could not find any image inside the %s directory", images_directory);
//         closeSDCard();
//         return false;
//     }

//     // Pick a random file
//     uint32_t targetIndex = random(0, fileCount);
//     log_v("Picking file at index %lu (0-based)", targetIndex);

//     // Reopen directory to iterate again
//     root = SD_CARD.open(images_directory, FILE_READ);
//     if (!root) {
//         log_e("Failed to reopen '%s'", images_directory);
//         closeSDCard();
//         return false;
//     }

//     uint32_t currentIndex = 0;
//     entry = root.openNextFile();
//     bool success = false;

//     // Find the target file
//     while (entry) {
//         if (!entry.isDirectory()) {
//             String fileName = String(entry.name());
//             if (fileName.endsWith(".bin") && !fileName.startsWith(".")) {
//                 if (currentIndex == targetIndex) {
//                     // Found our target file!
//                     log_i("Selected file: %s", entry.name());

//                     size_t fileSize = entry.size();
//                     log_v("File size: %u bytes (expected: %u bytes)", fileSize, IMAGE_BUFFER_SIZE);

//                     // Validate file size
//                     if (fileSize != IMAGE_BUFFER_SIZE) {
//                         log_e("File size mismatch! File: %u bytes, Buffer: %u bytes", fileSize, IMAGE_BUFFER_SIZE);
//                         entry.close();
//                         break;
//                     }

//                     // Build full path - check if fileName already contains the full path
//                     String fullPath;
//                     if (fileName.startsWith("/")) {
//                         fullPath = fileName;
//                     } else {
//                         fullPath = String(images_directory) + "/" + fileName;
//                     }
//                     log_v("Full path: %s", fullPath.c_str());

//                     // Check buffer is valid
//                     if (!g_image_buffer) {
//                         log_e("g_image_buffer is NULL!");
//                         entry.close();
//                         break;
//                     }
//                     log_v("g_image_buffer address: %p", g_image_buffer);

//                     entry.close();

//                     File imageFile = SD_CARD.open(fullPath.c_str(), FILE_READ);
//                     if (!imageFile) {
//                         log_e("Failed to open file: %s", fullPath.c_str());
//                         break;
//                     }

//                     log_v("File opened successfully, available bytes: %u", imageFile.available());

//                     // Read file content into g_image_buffer
//                     log_v("Reading file into image buffer...");
//                     size_t bytesRead = imageFile.read(g_image_buffer, IMAGE_BUFFER_SIZE);
//                     imageFile.close();

//                     if (bytesRead != IMAGE_BUFFER_SIZE) {
//                         log_e("Failed to read complete file! Read: %u bytes, Expected: %u bytes", bytesRead, IMAGE_BUFFER_SIZE);
//                         break;
//                     }

//                     log_i("Successfully loaded %u bytes from SD card", bytesRead);
//                     success = true;
//                     break;
//                 }
//                 currentIndex++;
//             }
//         }
//         entry.close();
//         entry = root.openNextFile();
//     }
//     root.close();

//     closeSDCard();
//     delay(100);

//     if (!success) {
//         return false;
//     }

//     if (!ensureDisplayInitialized()) {
//         log_e("Failed to initialize the display");
//         return false;
//     }

//     writeImageBufferOnDisplay();

//     log_i("Image from SD card rendered successfully");
//     return true;
// }

// // Test 5g: Portrait Image Test with Rotation
// bool testPortraitImage()
// {
//     log_i("========================================");
//     log_i("Portrait Image Test");
//     log_i("========================================");

//     if (!ensureDisplayInitialized()) {
//         log_e("Failed to initialize the display");
//         return false;
//     }

//     log_v("Testing PRE-ROTATED portrait image (800×480 already rotated)...");

//     log_v("Copying PRE-ROTATED image from PROGMEM to RAM...");
//     memcpy_P(g_image_buffer, test_photo_6c_portrait, 800 * 480);

//     writeImageBufferOnDisplay();
//     return true;
// }

// // Test 5f: Official Library Image Test
// bool testOfficialLibraryImage()
// {
//     log_i("========================================");
//     log_i("Official Library Image Test WITH OVERLAYS");
//     log_i("========================================");

//     if (!ensureDisplayInitialized()) {
//         log_e("Failed to initialize the display");
//         return false;
//     }

//     log_v("Testing official GxEPD2 library image with overlays...");
//     log_v("Image: Bitmap7c800x480 (384000 bytes, standard format)");

//     log_d("Setting full window...");
//     display2.setFullWindow();

//     // log_d("Writing screen buffer");
//     // display2.writeScreenBuffer();

//     log_d("Clearing display...");
//     display2.clearScreen();

//     // Draw the bitmap (same order as working example)
//     log_d("Drawing bitmap...");
//     display2.epd2.writeDemoBitmap(Bitmap7c800x480_2, 0, 0, 0, 800, 480, 1, false, true);

//     // Refresh display to show the image
//     log_d("Refreshing display...");
//     display2.refresh();

//     log_v("Official library image rendered successfully");
//     return true;
// }

// void enterDeepSleep(uint16_t refresh_seconds = 60)
// {
//     log_i("enterDeepSleep(%lu)", refresh_seconds);

//     pinMode(WAKEUP_PIN, INPUT);
//     rtc_gpio_init(WAKEUP_PIN);
//     rtc_gpio_set_direction(WAKEUP_PIN, RTC_GPIO_MODE_INPUT_ONLY);
//     rtc_gpio_pulldown_dis(WAKEUP_PIN);
//     rtc_gpio_pullup_en(WAKEUP_PIN);
//     rtc_gpio_hold_dis(WAKEUP_PIN);

//     delay(10);

//     int pinState = digitalRead(WAKEUP_PIN);

//     if (pinState != 1) {
//         log_w("Testing RTC IO capability for GPIO %d... Pin state: %d - WARNING: Pin should be HIGH when idle - check wiring or RTC GPIO config", WAKEUP_PIN, pinState);
//     } else {
//         log_v("Testing RTC IO capability for GPIO %d... Pin state: %d - Pin correctly reads HIGH with RTC pull-up", WAKEUP_PIN, pinState);
//     }

//     esp_err_t wakeup_result = esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, WAKEUP_LEVEL);

//     if (wakeup_result == ESP_OK) {
//         log_d("EXT0 config: SUCCESS");
//     } else {
//         log_e("EXT0 config: FAILED - Error code: %d", wakeup_result);
//         log_e("This GPIO pin does not support RTC IO / EXT0 wakeup!");
//     }

//     // Configure timer wakeup if refresh microseconds is provided
//     if (refresh_seconds > 0) {
//         log_i("Configuring timer wakeup for %lu seconds", refresh_seconds);
//         uint64_t refresh_microseconds = (uint64_t)refresh_seconds * MICROSECONDS_IN_SECOND;
//         esp_sleep_enable_timer_wakeup(refresh_microseconds);
//     } else {
//         log_i("No timer wakeup configured - will sleep indefinitely");
//     }

//     log_i("Going to deep sleep now. Good night!");
//     Serial.flush(); // Ensure all serial output is sent before sleeping
//     esp_deep_sleep_start();
// }

// void printWakeUpReason()
// {

//     // Check wake-up reason
//     esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
//     const char* wakeup_reason_str;
//     switch (wakeup_reason) {
//     case ESP_SLEEP_WAKEUP_EXT0:
//         wakeup_reason_str = "EXT0 (Button)";
//         break;
//     case ESP_SLEEP_WAKEUP_EXT1:
//         wakeup_reason_str = "EXT1";
//         break;
//     case ESP_SLEEP_WAKEUP_TIMER:
//         wakeup_reason_str = "Timer";
//         break;
//     case ESP_SLEEP_WAKEUP_TOUCHPAD:
//         wakeup_reason_str = "Touchpad";
//         break;
//     case ESP_SLEEP_WAKEUP_ULP:
//         wakeup_reason_str = "ULP";
//         break;
//     case ESP_SLEEP_WAKEUP_GPIO:
//         wakeup_reason_str = "GPIO";
//         break;
//     case ESP_SLEEP_WAKEUP_UART:
//         wakeup_reason_str = "UART";
//         break;
//     case ESP_SLEEP_WAKEUP_WIFI:
//         wakeup_reason_str = "WiFi";
//         break;
//     case ESP_SLEEP_WAKEUP_COCPU:
//         wakeup_reason_str = "COCPU";
//         break;
//     case ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG:
//         wakeup_reason_str = "COCPU Trap";
//         break;
//     case ESP_SLEEP_WAKEUP_BT:
//         wakeup_reason_str = "Bluetooth";
//         break;
//     default:
//         wakeup_reason_str = "Power-on or Reset";
//         break;
//     }
//     log_i("Wake-up reason: %s", wakeup_reason_str);
// }

// void printBoardInfo()
// {
//     log_i("========================================");
//     log_i("BOARD INFORMATION");
//     log_i("========================================");

//     // Board identification
//     log_i("Board: %s", ARDUINO_BOARD);
//     log_i("Chip Model: %s", ESP.getChipModel());
//     log_i("Chip Revision: %d", ESP.getChipRevision());
//     log_i("Cores: %d", ESP.getChipCores());

//     // CPU Frequency
//     log_i("CPU Frequency: %d MHz", ESP.getCpuFreqMHz());

//     // Flash Memory
//     log_i("Flash Size: %u bytes (%.2f MB)", ESP.getFlashChipSize(), ESP.getFlashChipSize() / 1048576.0f);
//     log_i("Flash Speed: %u MHz", ESP.getFlashChipSpeed() / 1000000);

//     // Heap Memory
//     log_i("Free Heap: %u bytes (%.2f KB)", ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0f);
//     log_i("Total Heap: %u bytes (%.2f KB)", ESP.getHeapSize(), ESP.getHeapSize() / 1024.0f);
//     log_i("Min Free Heap: %u bytes (%.2f KB)", ESP.getMinFreeHeap(), ESP.getMinFreeHeap() / 1024.0f);

//     // PSRAM
//     if (psramFound()) {
//         log_i("PSRAM: FOUND");
//         log_i("PSRAM Size: %u bytes (%.2f MB)", ESP.getPsramSize(), ESP.getPsramSize() / 1048576.0f);
//         log_i("Free PSRAM: %u bytes (%.2f MB)", ESP.getFreePsram(), ESP.getFreePsram() / 1048576.0f);
//         log_i("Min Free PSRAM: %u bytes (%.2f MB)", ESP.getMinFreePsram(), ESP.getMinFreePsram() / 1048576.0f);
//     } else {
//         log_w("PSRAM: NOT FOUND");
//     }

//     // SDK and MAC
//     log_i("SDK Version: %s", ESP.getSdkVersion());

//     // Display configuration
//     log_i("========================================");
//     log_i("DISPLAY CONFIGURATION");
//     log_i("========================================");
//     log_i("Display size: %dx%d", DISP_WIDTH, DISP_HEIGHT);
//     log_i("Hardware size: %dx%d", HW_WIDTH, HW_HEIGHT);
// #if defined(ORIENTATION_PORTRAIT)
//     log_i("Orientation: PORTRAIT");
// #else
//     log_i("Orientation: LANDSCAPE");
// #endif

// #if defined(DISP_7C)
//     log_i("Display Type: 7-Color");
// #elif defined(DISP_6C)
//     log_i("Display Type: 6-Color");
// #else
//     log_i("Display Type: Black & White");
// #endif

//     log_i("========================================\n");
// }

// } // namespace display_debug

// void setup()
// {
//     // Inizializza la seriale
//     Serial.begin(115200);
//     while(!Serial.isConnected()) {
//         ;;
//     }
//     delay(5000);

//     log_i("========================================");
//     log_i("Display Debug Mode - STARTED");
//     log_i("========================================");

//     display_debug::printWakeUpReason();
//     display_debug::printBoardInfo();

//     rtcCounter += 1;

//     log_d("rtcCounter = %d", rtcCounter);

//     if (!display_debug::ensureSDCardInitialized()) {
//         log_e("SD Card initialization failed!");
//     } else {
//         log_d("SD Card initialized!");

//         display_debug::printSdCardStats();
//         delay(1000);
//         display_debug::closeSDCard();
//     }

//     // if (!display_debug::ensureWiFiInitialized()) {
//     //     log_w("Failed to connect to WiFi!");
//     // }
//     // delay(1000);
//     // display_debug::closeWiFi();

//     bool success = false;

//     if (rtcCounter % 3 == 0) {
//         success = display_debug::testPortraiImageFromSdCard();
//     } else if (rtcCounter % 2 == 0) {
//         success = display_debug::testPortraitImage();
//     } else {
//         success = display_debug::testOfficialLibraryImage();
//     }

//     delay(1000);

//     display_debug::cleanup_image_buffer();
//     display_debug::closeDisplay();

//     if (success) {
//         delay(10000);
//         display_debug::enterDeepSleep();
//     } else {
//         log_w("Failed, not entering deep sleep");
//     }
// }

// void loop()
// {
// }

// #endif // ENABLE_DISPLAY_DIAGNOSTIC

#include <SPI.h>
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include "image.h"
void setup()
{
    pinMode(6, INPUT); // BUSY
    pinMode(4, OUTPUT); // RES
    pinMode(16, OUTPUT); // DC
    pinMode(38, OUTPUT); // CS

    // SPI
    SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    SPI.begin(36, -1, 35, 38);
}

// Tips//
/*
1.Flickering is normal when EPD is performing a full screen update to clear ghosting from the previous image so to ensure better clarity and legibility for the new image.
2.There will be no flicker when EPD performs a partial refresh.
3.Please make sue that EPD enters sleep mode when refresh is completed and always leave the sleep mode command. Otherwise, this may result in a reduced lifespan of EPD.
4.Please refrain from inserting EPD to the FPC socket or unplugging it when the MCU is being powered to prevent potential damage.)
5.Re-initialization is required for every full screen update.
6.When porting the program, set the BUSY pin to input mode and other pins to output mode.
*/
void loop()
{

    /************Full display*******************/
    EPD_init_fast(); // Full screen refresh initialization.
    PIC_display(gImage_1); // To Display one image using full screen refresh.
    delay(1);
    EPD_sleep(); // Enter the sleep mode and please do not delete it, otherwise it will reduce the lifespan of the screen.
    delay(5000); // Delay for 5s.

    while (1)
        ; // The program stops here
}
