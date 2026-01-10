#include "display_driver_bw.h"
#include "config.h"

// Include the GDEY075T7 library headers
// The pins are taken from config.h (EPD_BUSY_PIN, EPD_RST_PIN, EPD_DC_PIN, EPD_CS_PIN)
#include <Display_EPD_GDEY075T7_W21.h>
#include <Display_EPD_GDEY075T7_W21_spi.h>

using namespace GDEY075T7;

DisplayDriverBW::DisplayDriverBW(int8_t cs_pin, int8_t dc_pin, int8_t rst_pin, int8_t busy_pin,
    int8_t sck_pin, int8_t mosi_pin)
    : _cs_pin(cs_pin)
    , _dc_pin(dc_pin)
    , _rst_pin(rst_pin)
    , _busy_pin(busy_pin)
    , _sck_pin(sck_pin)
    , _mosi_pin(mosi_pin)
{
    initialized = false;
}

DisplayDriverBW::~DisplayDriverBW()
{
    if (initialized) {
        sleep();
    }
}

void DisplayDriverBW::configureSPI()
{
    // Configure pins
    pinMode(_busy_pin, INPUT);
    pinMode(_rst_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_cs_pin, OUTPUT);

    // Initialize SPI
    SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    SPI.begin(_sck_pin, -1, _mosi_pin, _cs_pin);
}

bool DisplayDriverBW::init()
{
    if (initialized) {
        log_w("Display already initialized");
        return true;
    }

    log_i("Initializing B&W display (GDEY075T7)...");

    // Configure SPI and pins
    configureSPI();

    // Add small delay for hardware stabilization
    delay(100);

    // Initialize the display using the library's fast init
    EPD_Init_Fast();

    initialized = true;
    log_i("B&W display initialized successfully");

    return true;
}

bool DisplayDriverBW::picDisplay(uint8_t* imageBuffer)
{
    if (!initialized) {
        log_e("Display not initialized");
        return false;
    }

    if (!imageBuffer) {
        log_e("Image buffer is NULL");
        return false;
    }

    log_d("Displaying image on B&W display...");

    // Display the image using the library's fast update function
    EPD_WhiteScreen_ALL_Fast(imageBuffer);

    log_d("Image displayed successfully");
    return true;
}

void DisplayDriverBW::sleep()
{
    if (!initialized) {
        log_w("Display not initialized, skipping sleep");
        return;
    }

    log_d("Putting B&W display to deep sleep...");

    // Enter deep sleep mode - IMPORTANT: Do not skip this to preserve display lifespan
    EPD_DeepSleep();

    initialized = false;
    log_d("Display is now in deep sleep mode");
}

void DisplayDriverBW::refresh()
{
    if (!initialized) {
        log_w("Display not initialized");
        return;
    }

    log_d("Refreshing B&W display...");

    // The GDEY075T7 library doesn't have a separate refresh function
    // The refresh is handled internally by EPD_WhiteScreen_ALL_Fast
    // This is here for interface compatibility

    log_d("Refresh complete");
}

void DisplayDriverBW::clear()
{
    if (!initialized) {
        log_w("Display not initialized");
        return;
    }

    log_d("Clearing B&W display to white...");

    // Clear the display to white
    EPD_WhiteScreen_White();

    log_d("Display cleared");
}