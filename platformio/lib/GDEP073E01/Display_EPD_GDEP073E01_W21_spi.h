#ifndef _DISPLAY_EPD_GDEP073E01_W21_SPI_
#define _DISPLAY_EPD_GDEP073E01_W21_SPI_
#include "Arduino.h"

namespace GDEP073E01 {

// Check if display pins are configured externally
// If not, use default values
#ifndef EPD_BUSY_PIN
#define EPD_BUSY_PIN 6
#endif
#ifndef EPD_RST_PIN
#define EPD_RST_PIN 4
#endif
#ifndef EPD_DC_PIN
#define EPD_DC_PIN 16
#endif
#ifndef EPD_CS_PIN
#define EPD_CS_PIN 38
#endif

// IO settings - using configurable pins
#define isEPD_W21_BUSY digitalRead(EPD_BUSY_PIN) // BUSY
#define EPD_W21_RST_0 digitalWrite(EPD_RST_PIN, LOW) // RES
#define EPD_W21_RST_1 digitalWrite(EPD_RST_PIN, HIGH)
#define EPD_W21_DC_0 digitalWrite(EPD_DC_PIN, LOW) // DC
#define EPD_W21_DC_1 digitalWrite(EPD_DC_PIN, HIGH)
#define EPD_W21_CS_0 digitalWrite(EPD_CS_PIN, LOW) // CS
#define EPD_W21_CS_1 digitalWrite(EPD_CS_PIN, HIGH)

void SPI_Write(unsigned char value);
void EPD_W21_WriteDATA(unsigned char datas);
void EPD_W21_WriteCMD(unsigned char command);

}

#endif