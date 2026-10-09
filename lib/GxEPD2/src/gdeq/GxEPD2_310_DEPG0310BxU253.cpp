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
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_310_DEPG0310BxU253.h"

GxEPD2_310_DEPG0310BxU253::GxEPD2_310_DEPG0310BxU253(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
}

void GxEPD2_310_DEPG0310BxU253::clearScreen(uint8_t value)
{
  _writeScreenBuffer(0x13, value); // set current
  refresh(false); // full refresh
  _initial_write = false;
}

void GxEPD2_310_DEPG0310BxU253::writeScreenBuffer(uint8_t value)
{
  if (_initial_write) return clearScreen(value);
  _writeScreenBuffer(0x13, value); // set current
}

void GxEPD2_310_DEPG0310BxU253::_writeScreenBuffer(uint8_t command, uint8_t value)
{
  if (!_init_display_done) _InitDisplay();
  _writeCommand(command);
  _startTransfer();
  for (uint32_t i = 0; i < uint32_t(WIDTH) * uint32_t(HEIGHT) / 8; i++)
  {
    _transfer(value);
  }
  _endTransfer();
}

void GxEPD2_310_DEPG0310BxU253::writeImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x13, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::_writeImage(uint8_t command, const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  uint16_t wb = (w + 7) / 8; // width bytes, bitmaps are padded
  x -= x % 8; // byte boundary
  w = wb * 8; // byte boundary
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_init_display_done) _InitDisplay();
  _writeCommand(command);
  _startTransfer();
  for (int16_t j = 0; j < h1; j++)
  {
    for (int16_t i = 0; i < w1 / 8; i++)
    {
      uint8_t data;
      // use wb, h of bitmap for index!
      uint16_t idx = mirror_y ? dx / 8 + ((h - 1 - (j + dy)) * wb) + i : (j + dy) * wb + dx / 8 + i;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _transfer(data);
    }
  }
  _endTransfer();
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_310_DEPG0310BxU253::_writeImagePart(uint8_t command, const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                         int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
  if ((w_bitmap < 0) || (h_bitmap < 0) || (w < 0) || (h < 0)) return;
  if ((x_part < 0) || (x_part >= w_bitmap)) return;
  if ((y_part < 0) || (y_part >= h_bitmap)) return;
  uint16_t wb_bitmap = (w_bitmap + 7) / 8; // width bytes, bitmaps are padded
  x_part -= x_part % 8; // byte boundary
  w = w_bitmap - x_part < w ? w_bitmap - x_part : w; // limit
  h = h_bitmap - y_part < h ? h_bitmap - y_part : h; // limit
  x -= x % 8; // byte boundary
  w = 8 * ((w + 7) / 8); // byte boundary, bitmaps are padded
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x; // limit
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y; // limit
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  int16_t dx_part = x_part - x_part % 8; // not needed, x_part already on byte boundary
  int16_t dy_part = y_part;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_init_display_done) _InitDisplay();
  _writeCommand(command);
  _startTransfer();
  for (int16_t j = 0; j < h1; j++)
  {
    for (int16_t i = 0; i < w1 / 8; i++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      uint16_t idx = mirror_y ? (dx_part + dx) / 8 + ((h_bitmap - 1 - (j + dy_part + dy)) * wb_bitmap) + i : (j + dy_part + dy) * wb_bitmap + (dx_part + dx) / 8 + i;
      if (pgm)
      {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      }
      else
      {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      _transfer(data);
    }
  }
  _endTransfer();
  delay(1); // yield() to avoid WDT on ESP8266 and ESP32
}

void GxEPD2_310_DEPG0310BxU253::writeImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_310_DEPG0310BxU253::writeImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    writeImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_310_DEPG0310BxU253::writeNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    writeImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_310_DEPG0310BxU253::drawImage(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_310_DEPG0310BxU253::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_310_DEPG0310BxU253::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImage(black, color, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_310_DEPG0310BxU253::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(black, color, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_310_DEPG0310BxU253::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeNative(data1, data2, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
}

void GxEPD2_310_DEPG0310BxU253::refresh(bool partial_update_mode)
{
  if (partial_update_mode) refresh(0, 0, WIDTH, HEIGHT);
  else
  {
    if (_using_partial_mode) _using_partial_mode = false;
    _Update_Full();
    _initial_refresh = false;
  }
}

void GxEPD2_310_DEPG0310BxU253::refresh(int16_t x, int16_t y, int16_t w, int16_t h)
{
  refresh(false); // No partial support, always full refresh
}

void GxEPD2_310_DEPG0310BxU253::powerOff(void)
{
  _PowerOff();
}

void GxEPD2_310_DEPG0310BxU253::hibernate()
{
  _PowerOff();
  if (_rst >= 0)
  {
    _writeCommand(0x07); // deep sleep
    _writeData(0xA5);    // check code
    _hibernating = true;
    _init_display_done = false;
  }
}

void GxEPD2_310_DEPG0310BxU253::_PowerOn()
{
  if (!_power_is_on)
  {
    _writeCommand(0x04);
    _waitWhileBusy("_PowerOn", power_on_time);
  }
  _power_is_on = true;
}

void GxEPD2_310_DEPG0310BxU253::_PowerOff()
{
  if (_power_is_on)
  {
    _writeCommand(0x02); // power off
    _waitWhileBusy("_PowerOff", power_off_time);
  }
  _power_is_on = false;
}

void GxEPD2_310_DEPG0310BxU253::_InitDisplay()
{
  _reset(); // Always reset as per provided code
  _power_is_on = false;
  _writeCommand(0x00); // PANEL SETTING
  _writeData(0xF7);
  _writeData(0x0d);
  _writeCommand(0x50);
  _writeData(0x97);
  _writeCommand(0x60);
  _writeData(0x22);
  _setLut();
  _init_display_done = true;
}

const uint8_t GxEPD2_310_DEPG0310BxU253_lut[] PROGMEM = {
    0x0D, 0x00, 0x3F, 0x3F, 0x10, 0x0E,       //1s
    0x00, 0x00, 0x00,
    //VCOM
    0x01, 0x13, 0x13, 0x00, 0x00, 0x01, 0x00,
    0x01, 0x13, 0x13, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //LUT_WW
    0x01, 0x13, 0x53, 0x00, 0x00, 0x01, 0x00,
    0x01, 0x13, 0x93, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //LUT_BW
    0x01, 0x13, 0x53, 0x00, 0x00, 0x01, 0x00,
    0x01, 0x13, 0x93, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //LUT_WB
    0x01, 0x93, 0x13, 0x00, 0x00, 0x01, 0x00,
    0x01, 0x53, 0x13, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //LUT_BB
    0x01, 0x93, 0x13, 0x00, 0x00, 0x01, 0x00,
    0x01, 0x53, 0x13, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //Reserved
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

void GxEPD2_310_DEPG0310BxU253::_setLut()
{
  uint16_t count;
  const uint8_t* wavedata = GxEPD2_310_DEPG0310BxU253_lut;
  _writeCommand(0x30);
  _writeDataPGM(wavedata++, 1);
  _writeCommand(0x01);
  _writeData(0x03);
  _writeDataPGM(wavedata++, 1);
  _writeDataPGM(wavedata++, 1);
  _writeDataPGM(wavedata++, 1);
  _writeDataPGM(wavedata++, 1);
  _writeCommand(0x82);
  _writeDataPGM(wavedata++, 1);
  _writeCommand(0x2A);
  _writeDataPGM(wavedata++, 1);
  _writeDataPGM(wavedata++, 1);
  _writeDataPGM(wavedata++, 1);
  _writeCommand(0x20);
  for(count=0;count<42;count++) _writeDataPGM(wavedata++, 1);
  _writeCommand(0x21);
  for(count=0;count<42;count++) _writeDataPGM(wavedata++, 1);
  _writeCommand(0x22);
  for(count=0;count<42;count++) _writeDataPGM(wavedata++, 1);
  _writeCommand(0x23);
  for(count=0;count<42;count++) _writeDataPGM(wavedata++, 1);
  _writeCommand(0x24);
  for(count=0;count<42;count++) _writeDataPGM(wavedata++, 1);
}

void GxEPD2_310_DEPG0310BxU253::_Update_Full()
{
  _writeCommand(0x17);
  _writeData(0xA5);
  _waitWhileBusy("_Update_Full", full_refresh_time);
  _init_display_done = false; // needed, reason unknown
}
