#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <GxEPD2_BW.h>
#include <gdeq/GxEPD2_310_DEPG0310BxU253.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <CRC32.h>
#include <math.h>

#ifndef EINK_SCALE_X
#define EINK_SCALE_X 1.875f
#endif
#ifndef EINK_SCALE_Y
#define EINK_SCALE_Y 2.5f
#endif

/**
 * MeshCore DisplayDriver that paints onto the Deck Pro shell's shared GxEPD2 instance
 * (HSPI / DEPG0310). Do not call begin() before w20_deckpro_shell_init().
 */
class W20ShellEpdDisplay : public DisplayDriver {
  using DeckEpd = GxEPD2_BW<GxEPD2_310_DEPG0310BxU253, GxEPD2_310_DEPG0310BxU253::HEIGHT>;
  DeckEpd *_epd = nullptr;
  bool _isOn = false;
  uint16_t _curr_color = GxEPD_BLACK;
  CRC32 _crc;
  int _last_crc = 0;
  const float scale_x = EINK_SCALE_X;
  const float scale_y = EINK_SCALE_Y;

public:
  W20ShellEpdDisplay() : DisplayDriver(128, 128) {}

  bool begin();
  /** Restore Deck LVGL rotation (0) after leaving Mesh page. */
  void restoreDeckRotation();
  bool isOn() override { return _isOn; }
  bool isEink() override { return true; }
  void forceFullRefresh() override { _last_crc = 0; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(ColorVal bkg = UIColor::window_bkg) override;
  void setTextSize(int sz) override;
  void setColor(ColorVal c) override;
  void setCursor(int x, int y) override;
  void print(const char *str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t *bits, int w, int h) override;
  uint16_t getTextWidth(const char *str) override;
  void endFrame() override;
};
