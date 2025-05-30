/*****************************************************************************
* | File      	:   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-02-19
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>

/**
 * data
 **/
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

/**
 * GPIO config (Waveshare e-Paper driver board)
 **/
// #define EPD_SCK_PIN  13
// #define EPD_MOSI_PIN 14
// #define EPD_CS_PIN   15
// #define EPD_RST_PIN  26
// #define EPD_DC_PIN   27
// #define EPD_BUSY_PIN 25

extern uint8_t EPD_CS_PIN;    // Chip Select pin
extern uint8_t EPD_DC_PIN;    // Data/Command pin
extern uint8_t EPD_RST_PIN;   // Reset pin
extern uint8_t EPD_BUSY_PIN;  // Busy pin
extern uint8_t EPD_MISO_PIN;  // Master In Slave Out pin (MISO)
extern uint8_t EPD_MOSI_PIN;  // Master Out Slave In pin (MOSI)
extern uint8_t EPD_SCK_PIN;   // Serial Clock pin (SCK)
// extern uint8_t EPD_PWR_PIN;   // Power pin (optional, can be -1 if not used)


// #define EPD_CS_PIN     D7  // arancione
// #define EPD_DC_PIN     D8  // verde
// #define EPD_RST_PIN    D9  // bianco
// #define EPD_BUSY_PIN   D10 // viola
// #define EPD_MISO_PIN   D12 // MISO
// #define EPD_MOSI_PIN   D11 // blu // MOSI (DIN)
// #define EPD_SCK_PIN    D13 // giallo
// #define EPD_PWR_PIN    -1

#define GPIO_PIN_SET   1
#define GPIO_PIN_RESET 0

/**
 * GPIO read and write
 **/
#define DEV_Digital_Write(_pin, _value) digitalWrite(_pin, _value == 0 ? LOW : HIGH)
#define DEV_Digital_Read(_pin)          digitalRead(_pin)

/**
 * delay x ms
 **/
#define DEV_Delay_ms(__xms) delay(__xms)

/*------------------------------------------------------------------------------------------------------*/
UBYTE DEV_Module_Init(void);
void GPIO_Mode(UWORD GPIO_Pin, UWORD Mode);
void DEV_SPI_WriteByte(UBYTE data);
UBYTE DEV_SPI_ReadByte();
void DEV_SPI_Write_nByte(UBYTE* pData, UDOUBLE len);

#endif
