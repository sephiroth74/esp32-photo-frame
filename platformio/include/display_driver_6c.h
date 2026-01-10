#ifndef _DISPLAY_DRIVER_6C_H_
#define _DISPLAY_DRIVER_6C_H_

#include "display_driver.h"
#include <SPI.h>

/**
 * Display driver for GDEP073E01 6-color e-paper display
 * Implements the DisplayDriver interface for 6-color display operations
 */
class DisplayDriver6C : public DisplayDriver {
public:
    DisplayDriver6C(int8_t cs_pin, int8_t dc_pin, int8_t rst_pin, int8_t busy_pin,
                    int8_t sck_pin, int8_t mosi_pin);
    virtual ~DisplayDriver6C();

    // DisplayDriver interface implementation
    bool init() override;
    bool picDisplay(uint8_t* imageBuffer) override;
    void sleep() override;
    void refresh() override;
    void clear() override;
    const char* getDisplayType() const override { return "6-Color"; }
    bool hasColor() const override { return true; }  // 6-color display

protected:
    void configureSPI() override;

private:
    // Pin definitions
    int8_t _cs_pin;
    int8_t _dc_pin;
    int8_t _rst_pin;
    int8_t _busy_pin;
    int8_t _sck_pin;
    int8_t _mosi_pin;

    // SPI transaction settings
    static constexpr uint32_t SPI_FREQUENCY = 10000000; // 10MHz
};

#endif // _DISPLAY_DRIVER_6C_H_