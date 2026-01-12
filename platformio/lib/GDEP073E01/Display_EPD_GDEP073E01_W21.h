#ifndef _DISPLAY_EPD_GDEP073E01_W21_H_
#define _DISPLAY_EPD_GDEP073E01_W21_H_

#define EPD_WIDTH 800
#define EPD_HEIGHT 480

namespace GDEP073E01 {

void SPI_Write(unsigned char value);
void EPD_W21_WriteDATA(unsigned char datas);
void EPD_W21_WriteCMD(unsigned char command);

// 8-bit color values for the display
enum Colors_8bit {
    COLOR_BLACK_8 = 0x00,
    COLOR_WHITE_8 = 0x11,
    COLOR_GREEN_8 = 0x66,
    COLOR_BLUE_8 = 0x55,
    COLOR_RED_8 = 0x33,
    COLOR_YELLOW_8 = 0x22,
    COLOR_CLEAN_8 = 0x77
};

// 4-bit color values for the display
enum Colors_4bit {
    COLOR_BLACK_4 = 0x00,
    COLOR_WHITE_4 = 0x01,
    COLOR_GREEN_4 = 0x06,
    COLOR_BLUE_4 = 0x05,
    COLOR_RED_4 = 0x03,
    COLOR_YELLOW_4 = 0x02,
    COLOR_CLEAN_4 = 0x07
};

// // Maintain backward compatibility with old names (deprecated)
// #define Black COLOR_BLACK_8
// #define White COLOR_WHITE_8
// #define Green COLOR_GREEN_8
// #define Blue COLOR_BLUE_8
// #define Red COLOR_RED_8
// #define Yellow COLOR_YELLOW_8
// #define Clean COLOR_CLEAN_8

// #define black COLOR_BLACK_4
// #define white COLOR_WHITE_4
// #define green COLOR_GREEN_4
// #define blue COLOR_BLUE_4
// #define red COLOR_RED_4
// #define yellow COLOR_YELLOW_4
// #define clean COLOR_CLEAN_4

#define PSR 0x00
#define PWRR 0x01
#define POF 0x02
#define POFS 0x03
#define PON 0x04
#define BTST1 0x05
#define BTST2 0x06
#define DSLP 0x07
#define BTST3 0x08
#define DTM 0x10
#define DRF 0x12
#define PLL 0x30
#define CDI 0x50
#define TCON 0x60
#define TRES 0x61
#define REV 0x70
#define VDCS 0x82
#define T_VDCS 0x84
#define PWS 0xE3
// EPD
void EPD_W21_Init(void);
void EPD_init(void);
void PIC_display(const unsigned char* picData);
void EPD_sleep(void);
void EPD_refresh(void);
void lcd_chkstatus(void);
void set_display_timeout(unsigned long timeout_ms);
unsigned long get_display_timeout(void);
void PIC_display_Clear(void);
void EPD_horizontal(void);
void EPD_vertical(void);
void E6_color(unsigned char color);
void EPD_Display_White(void);
void EPD_Display_Black(void);
void EPD_Display_Yellow(void);
void EPD_Display_blue(void);
void EPD_Display_Green(void);
void EPD_Display_red(void);
void EPD_init_fast(void);

} // namespace GDEP073E01

#endif
/***********************************************************
            end file
***********************************************************/
