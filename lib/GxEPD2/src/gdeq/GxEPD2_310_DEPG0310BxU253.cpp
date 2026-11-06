// Display Library for SPI e-paper panels from Dalian Good Display and boards from Waveshare.
// Requires HW SPI and Adafruit_GFX. Caution: the e-paper panels require 3.3V supply AND data lines!
//
// Panel: DEPG0310BxU253 (UC8253). Full refresh keeps vendor LUT/0x17 path;
// partial refresh follows GDEQ031T10 UC8253 window + 0x12 sequence.
//
// Library: https://github.com/ZinggJM/GxEPD2

#include "GxEPD2_310_DEPG0310BxU253.h"
#include <string.h>

/*
 * previous 帧缓冲必须放文件静态区，不能做类成员：
 * GxEPD2_BW 构造按值拷贝 epd2，成员大数组会在静态初始化时栈溢出，
 * 随后在 lv_disp_drv_register / heap realloc 处以 StoreProhibited 爆掉。
 */
static const uint32_t DEPG0310_PREV_BYTES =
    (uint32_t)GxEPD2_310_DEPG0310BxU253::WIDTH * (uint32_t)GxEPD2_310_DEPG0310BxU253::HEIGHT / 8;
static uint8_t s_depg0310_prev[DEPG0310_PREV_BYTES];
static bool s_depg0310_prev_valid = false;

GxEPD2_310_DEPG0310BxU253::GxEPD2_310_DEPG0310BxU253(int16_t cs, int16_t dc, int16_t rst, int16_t busy) :
  GxEPD2_EPD(cs, dc, rst, busy, LOW, 10000000, WIDTH, HEIGHT, panel, hasColor, hasPartialUpdate, hasFastPartialUpdate)
{
}

void GxEPD2_310_DEPG0310BxU253::clearScreen(uint8_t value)
{
  memset(s_depg0310_prev, value, DEPG0310_PREV_BYTES);
  s_depg0310_prev_valid = true;
  _writeScreenBuffer(0x10, value); // set previous
  _writeScreenBuffer(0x13, value); // set current
  refresh(false); // full refresh
  _initial_write = false;
}

void GxEPD2_310_DEPG0310BxU253::writeScreenBuffer(uint8_t value)
{
  if (_initial_write) return clearScreen(value);
  _writeScreenBuffer(0x13, value); // set current
}

void GxEPD2_310_DEPG0310BxU253::writeScreenBufferAgain(uint8_t value)
{
  _writeScreenBuffer(0x10, value); // set previous
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

void GxEPD2_310_DEPG0310BxU253::writeImageForFullRefresh(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  /* 局刷后 panel=0xD7；全刷前必须回到 0xF7+自定义 LUT，否则会变成「假 FULL」(~750ms) */
  _InitDisplay();
  _skip_partial_init = true;
  _writeImage(0x10, bitmap, x, y, w, h, invert, mirror_y, pgm); // set previous
  _writeImage(0x13, bitmap, x, y, w, h, invert, mirror_y, pgm); // set current
  _skip_partial_init = false;
  _storePrev(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::writeImageAgain(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  /* hasFastPartialUpdate=false 时通常不调用；若调用只更新主机 prev，勿再写控制器 0x10 */
  _storePrev(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::writeImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                        int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x13, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::writeImagePartAgain(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                             int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  _writeImagePart(0x10, bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
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
  /*
   * 局刷对齐 demo Display_windows_partial_image 每帧：
   *   Init_partial(硬复位+0xD7/E0/E5=100/50=0x17) → OLD(0x10) → NEW(0x13) → 0x04/0x12
   * 数据极性与全刷/GxEPD 页缓冲一致（勿再额外翻位，否则全刷后局刷字发白、叠墨）。
   */
  if (!_skip_partial_init && command == 0x13) {
    _Init_Partial();
    _writePrevToOld(x1, y1, w1, h1);
  } else if (!_init_display_done) {
    _InitDisplay();
  }
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(command);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb, h of bitmap for index!
      uint16_t idx = mirror_y ? j + dx / 8 + uint16_t((h - 1 - (i + dy))) * wb : j + dx / 8 + uint16_t(i + dy) * wb;
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
  _writeCommand(0x92); // partial out
  /* 主机 prev = 刚写入的 NEW，供下一帧 OLD（硬复位后控制器 RAM 会丢） */
  if (!_skip_partial_init && command == 0x13) {
    _storePrev(bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
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
  int16_t dx_part = x_part; // already on byte boundary
  int16_t dy_part = y_part;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  if (!_skip_partial_init && command == 0x13) {
    _Init_Partial();
    _writePrevToOld(x1, y1, w1, h1);
  } else if (!_init_display_done) {
    _InitDisplay();
  }
  if (_initial_write) writeScreenBuffer(); // initial full screen buffer clean
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _writeCommand(command);
  _startTransfer();
  for (int16_t i = 0; i < h1; i++)
  {
    for (int16_t j = 0; j < w1 / 8; j++)
    {
      uint8_t data;
      // use wb_bitmap, h_bitmap of bitmap for index!
      uint16_t idx = mirror_y ? (dx_part + dx) / 8 + j + uint16_t((h_bitmap - 1 - (i + dy_part + dy))) * wb_bitmap
                              : (dx_part + dx) / 8 + j + uint16_t(i + dy_part + dy) * wb_bitmap;
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
  _writeCommand(0x92); // partial out
  if (!_skip_partial_init && command == 0x13) {
    _storePrev(bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
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
  writeImageAgain(bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::drawImagePart(const uint8_t bitmap[], int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  writeImagePart(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  refresh(x, y, w, h);
  writeImagePartAgain(bitmap, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
}

void GxEPD2_310_DEPG0310BxU253::drawImage(const uint8_t* black, const uint8_t* color, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    drawImage(black, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_310_DEPG0310BxU253::drawImagePart(const uint8_t* black, const uint8_t* color, int16_t x_part, int16_t y_part, int16_t w_bitmap, int16_t h_bitmap,
                       int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (black)
  {
    drawImagePart(black, x_part, y_part, w_bitmap, h_bitmap, x, y, w, h, invert, mirror_y, pgm);
  }
}

void GxEPD2_310_DEPG0310BxU253::drawNative(const uint8_t* data1, const uint8_t* data2, int16_t x, int16_t y, int16_t w, int16_t h, bool invert, bool mirror_y, bool pgm)
{
  if (data1)
  {
    drawImage(data1, x, y, w, h, invert, mirror_y, pgm);
  }
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
  if (_initial_refresh) return refresh(false); // initial update needs be full update
  // intersection with screen
  int16_t w1 = x < 0 ? w + x : w; // reduce
  int16_t h1 = y < 0 ? h + y : h; // reduce
  int16_t x1 = x < 0 ? 0 : x; // limit
  int16_t y1 = y < 0 ? 0 : y; // limit
  w1 = x1 + w1 < int16_t(WIDTH) ? w1 : int16_t(WIDTH) - x1; // limit
  h1 = y1 + h1 < int16_t(HEIGHT) ? h1 : int16_t(HEIGHT) - y1; // limit
  if ((w1 <= 0) || (h1 <= 0)) return;
  // make x1, w1 multiple of 8
  w1 += x1 % 8;
  if (w1 % 8 > 0) w1 += 8 - w1 % 8;
  x1 -= x1 % 8;
  if (usePartialUpdateWindow) _writeCommand(0x91); // partial in
  _setPartialRamArea(x1, y1, w1, h1);
  _Update_Part();
  if (usePartialUpdateWindow) _writeCommand(0x92); // partial out（demo Display_windows_partial_image）
  _using_partial_mode = true;
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

void GxEPD2_310_DEPG0310BxU253::_setPartialRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  uint16_t xe = (x + w - 1) | 0x0007; // byte boundary inclusive (last byte)
  uint16_t ye = y + h - 1;
  x &= 0xFFF8; // byte boundary
  _writeCommand(0x90); // partial window
  _writeData(x);
  _writeData(xe);
  _writeData(y / 256);
  _writeData(y % 256);
  _writeData(ye / 256);
  _writeData(ye % 256);
  _writeData(0x01);
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
  /* 全刷路径：硬复位后 Panel 0xF7 + 自定义 LUT（从局刷 OTP 切回） */
  _reset();
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
  _hibernating = false;
  _using_partial_mode = false;
}

void GxEPD2_310_DEPG0310BxU253::_Init_Partial()
{
  /* demo Epaper_Init_partial：每帧硬复位 + 全套局刷寄存器 */
  _reset();
  _hibernating = false;
  _power_is_on = false;
  _writeCommand(0x00); // Panel setting
  _writeData(0xD7);
  _writeCommand(0xE0);
  _writeData(0x02);
  _writeCommand(0xE5);
  _writeData(100);
  _writeCommand(0x50);
  _writeData(0x17);
  _init_display_done = true;
}

void GxEPD2_310_DEPG0310BxU253::_storePrev(const uint8_t bitmap[], int16_t x, int16_t y, int16_t w, int16_t h,
                                          bool invert, bool mirror_y, bool pgm)
{
  if (!bitmap) return;
  uint16_t wb = (w + 7) / 8;
  x -= x % 8;
  w = wb * 8;
  int16_t x1 = x < 0 ? 0 : x;
  int16_t y1 = y < 0 ? 0 : y;
  int16_t w1 = x + w < int16_t(WIDTH) ? w : int16_t(WIDTH) - x;
  int16_t h1 = y + h < int16_t(HEIGHT) ? h : int16_t(HEIGHT) - y;
  int16_t dx = x1 - x;
  int16_t dy = y1 - y;
  w1 -= dx;
  h1 -= dy;
  if ((w1 <= 0) || (h1 <= 0)) return;
  const uint16_t screen_wb = WIDTH / 8;
  for (int16_t i = 0; i < h1; i++) {
    for (int16_t j = 0; j < w1 / 8; j++) {
      uint16_t idx = mirror_y ? j + dx / 8 + uint16_t((h - 1 - (i + dy))) * wb
                              : j + dx / 8 + uint16_t(i + dy) * wb;
      uint8_t data;
      if (pgm) {
#if defined(__AVR) || defined(ESP8266) || defined(ESP32)
        data = pgm_read_byte(&bitmap[idx]);
#else
        data = bitmap[idx];
#endif
      } else {
        data = bitmap[idx];
      }
      if (invert) data = ~data;
      s_depg0310_prev[(uint32_t)(y1 + i) * screen_wb + (uint32_t)(x1 / 8 + j)] = data;
    }
  }
  s_depg0310_prev_valid = true;
}

void GxEPD2_310_DEPG0310BxU253::_writePrevToOld(int16_t x, int16_t y, int16_t w, int16_t h)
{
  _writeCommand(0x91); // partial in
  _setPartialRamArea(x, y, w, h);
  _writeCommand(0x10); // OLD
  _startTransfer();
  const uint16_t screen_wb = WIDTH / 8;
  for (int16_t i = 0; i < h; i++) {
    for (int16_t j = 0; j < w / 8; j++) {
      uint8_t data = 0xFF; /* demo 首帧 OLD=白 */
      if (s_depg0310_prev_valid) {
        data = s_depg0310_prev[(uint32_t)(y + i) * screen_wb + (uint32_t)(x / 8 + j)];
      }
      _transfer(data);
    }
  }
  _endTransfer();
  _writeCommand(0x92); // partial out
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

void GxEPD2_310_DEPG0310BxU253::_Update_Part()
{
  /* demo：Init_Partial 已配好，此处仅 0x04 → 0x12（帧间不 deepsleep） */
  _PowerOn();
  _writeCommand(0x12); // Update
  _waitWhileBusy("_Update_Part", partial_refresh_time);
}
