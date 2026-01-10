#include "display_driver_6c.h"
#include "config.h"

// Include the GDEP073E01 library headers
// The pins are taken from config.h (EPD_BUSY_PIN, EPD_RST_PIN, EPD_DC_PIN, EPD_CS_PIN)
#include <Display_EPD_GDEP073E01_W21.h>
#include <Display_EPD_GDEP073E01_W21_spi.h>

using namespace GDEP073E01;

DisplayDriver6C::DisplayDriver6C(int8_t cs_pin, int8_t dc_pin, int8_t rst_pin, int8_t busy_pin,
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

DisplayDriver6C::~DisplayDriver6C()
{
    if (initialized) {
        sleep();
    }
}

void DisplayDriver6C::configureSPI()
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

bool DisplayDriver6C::init()
{
    if (initialized) {
        log_w("Display already initialized");
        return true;
    }

    log_i("Initializing 6-color display (GDEP073E01)...");

    // Configure SPI and pins
    configureSPI();

    // Add small delay for hardware stabilization
    delay(100);

    // Initialize the display using the library's fast init
    EPD_init_fast();

    initialized = true;
    log_i("6-color display initialized successfully");

    return true;
}

bool DisplayDriver6C::picDisplay(uint8_t* imageBuffer)
{
    if (!initialized) {
        log_e("Display not initialized");
        return false;
    }

    if (!imageBuffer) {
        log_e("Image buffer is NULL");
        return false;
    }

    log_d("Displaying image on 6-color display...");

    // Display the image using the library function
    PIC_display(imageBuffer);

    // Small delay for display stability
    delay(1);

    log_d("Image displayed successfully");
    return true;
}

void DisplayDriver6C::sleep()
{
    if (!initialized) {
        log_w("Display not initialized, skipping sleep");
        return;
    }

    log_d("Putting 6-color display to sleep...");

    // Enter sleep mode - IMPORTANT: Do not skip this to preserve display lifespan
    EPD_sleep();

    initialized = false;
    log_d("Display is now in sleep mode");
}

void DisplayDriver6C::refresh()
{
    if (!initialized) {
        log_w("Display not initialized");
        return;
    }

    log_w("Not implemented for this display");

    log_d("Refresh complete");
}

void DisplayDriver6C::clear()
{
    if (!initialized) {
        log_w("Display not initialized");
        return;
    }

    log_d("Clearing 6-color display to white...");

    // Clear the display using the library function
    PIC_display_Clear();
    log_d("Display cleared");
}