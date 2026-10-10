#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <GxEPD2_BW.h>
#include <gdeq/GxEPD2_310_DEPG0310BxU253.h>
#include <Adafruit_GFX.h>
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
 * MeshCore DisplayDriver on Deck shared GxEPD2.
 * Draw linearly into GFXcanvas1, blit with Y-mirror like w20_meshtastic EInkDisplay2
 * (matches Deck disp_flush when W20_DECKPRO_FLIP_Y=0).
 */
class W20ShellEpdDisplay : public DisplayDriver {
  using DeckEpd = GxEPD2_BW<GxEPD2_310_DEPG0310BxU253, GxEPD2_310_DEPG0310BxU253::HEIGHT>;
  DeckEpd *_epd = nullptr;
  GFXcanvas1 *_canvas = nullptr;
  bool _isOn = false;
  uint16_t _curr_color = 1; /* canvas: 1=black */
  CRC32 _crc;
  int _last_crc = 0;
  int _cursor_x = 0;
  int _cursor_y = 0;
  const float scale_x = EINK_SCALE_X;
  const float scale_y = EINK_SCALE_Y;

public:
  W20ShellEpdDisplay() : DisplayDriver(128, 128) {}

  bool begin();
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

/** Touch/back hit helpers: Mesh uses Deck-aligned orientation (no software FLIP). */
bool w20_shell_epd_flip_x(void);
bool w20_shell_epd_flip_y(void);
uint8_t w20_shell_epd_rotation(void);
