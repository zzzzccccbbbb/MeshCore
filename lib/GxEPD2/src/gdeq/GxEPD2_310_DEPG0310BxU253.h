// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// based on Demo Example from Good Display: https://www.good-display.com/product/426.html
// Panel: DEPG0310BxU253 : (assuming similar to GDEQ031T10, but with custom init and LUT)
// Controller: UC8253 : https://v4.cecdn.yun300.cn/100001_1909185148/UC8253.pdf
//
// Author: Jean-Marc Zingg (adapted for DEPG0310BxU253 by Grok)
//
// Version: see library.properties
//A
// Library: https://github.com/ZinggJM/GxEPD2

#ifndef _GxEPD2_310_DEPG0310BxU253_H_
#define _GxEPD2_310_DEPG0310BxU253_H_

#include "../GxEPD2_EPD.h"

class GxEPD2_310_DEPG0310BxU253 : public GxEPD2_EPD
{
  public:
    // attributes
    static const uint16_t WIDTH = 240;
    static const uint16_t WIDTH_VISIBLE = WIDTH;
    static const uint16_t HEIGHT = 320;
    static const GxEPD2::Panel panel = GxEPD2::GDEQ031T10; // Custom panel, not in standard enum
    static const bool hasColor = false;
    static const bool hasPartialUpdate = true;
    static const bool usePartialUpdateWindow = true; // set false for better image
    /* false：不在 refresh 后再 writeImageAgain。每帧由主机写 OLD+NEW（对齐大凌 demo），
     * 避免 UC8253 刷新后缓冲交换/二次写 0x10 造成隔帧白字。 */
    static const bool hasFastPartialUpdate = false;
    static const uint16_t power_on_time = 50; // ms
    static const uint16_t power_off_time = 50; // ms
    static const uint16_t full_refresh_time = 1100; // ms
    static const uint16_t partial_refresh_time = 700; // ms
    // constructor
    GxEPD2_310_DEPG0310BxU253(int16_t cs, int16_t dc, int16_t rst, int16_t busy);
    // methods (virtual)
    //  Support for Bitmaps (Sprites) to Controller Buffer and to Screen
    void clearScreen(uint8_t value = 0xFF); // init controller memory and screen (default white)
    void writeScreenBuffer(uint8_t value = 0xFF); // init controller memory (default white)
    void writeScreenBufferAgain(uint8_t value = 0xFF); // init previous buffer
    // write to controller memory, without screen refresh; x and w should be multiple of 8
    void writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                             int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, without screen refresh; x and w should be multiple of 8
    void writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write to controller memory, with screen refresh; x and w should be multiple of 8
    void drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    // write sprite of native data to controller memory, with screen refresh; x and w should be multiple of 8
    void drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void refresh(bool partial_update_mode = false); // screen refresh from controller memory to full screen
    void refresh(int16_t x, int16_t y, int16_t w, int16_t h); // screen refresh from controller memory, partial screen
    void powerOff(); // turns off generation of panel driving voltages, avoids screen fading over time
    void hibernate(); // turns powerOff() and sets controller to deep sleep for minimum power use, ONLY if wakeable by RST (rst >= 0)
  private:
    void _writeScreenBuffer(uint8_t command, uint8_t value);
    void _writeImage(uint8_t command, const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void _writeImagePart(uint8_t command, const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                         int16_t x, int16_t y, int16_t w, int16_t h, bool invert = false, bool mirror_y = false, bool pgm = false);
    void _setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void _PowerOn();
    void _PowerOff();
    void _InitDisplay();
    /** 大凌 DEPG0310BBU253FxX demo：Epaper_Init_partial（含硬复位） */
    void _Init_Partial();
    void _setLut();
    void _Update_Full();
    void _Update_Part();
    /** 将 bitmap 窗口写入主机侧 previous（硬复位后控制器 RAM 会丢） */
    void _storePrev(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h,
                    bool invert, bool mirror_y, bool pgm);
    /** demo EPD_Dis_Part_RAM_OLD：把 previous 窗口写到 0x10 */
    void _writePrevToOld(int16_t x, int16_t y, int16_t w, int16_t h);
    bool _skip_partial_init = false; /* writeImageForFullRefresh 期间跳过局刷 Init */
};

#endif
