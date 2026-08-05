#include <Arduino.h>
#include <Wire.h>

#include "MeshnologyW13Board.h"

void MeshnologyW13Board::begin() {
  NRF52BoardDCDC::begin();
  btn_prev_state = HIGH;

  pinMode(PIN_VBAT_READ, INPUT);

  pinMode(PIN_BUTTON1, INPUT_PULLUP);
  pinMode(PIN_BUTTON2, INPUT_PULLUP);
  pinMode(PIN_BUTTON3, INPUT_PULLUP);
  pinMode(PIN_BUTTON4, INPUT_PULLUP);
  pinMode(PIN_BUTTON5, INPUT_PULLUP);
  pinMode(PIN_BUTTON6, INPUT_PULLUP);

#if defined(PIN_WIRE_SDA) && defined(PIN_WIRE_SCL)
  Wire.setPins(PIN_WIRE_SDA, PIN_WIRE_SCL);
#endif
  Wire.begin();

#ifdef PIN_GPS_RESET
  pinMode(PIN_GPS_RESET, OUTPUT);
  digitalWrite(PIN_GPS_RESET, !PIN_GPS_RESET_ACTIVE);
#endif

#ifdef PIN_GPS_EN
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, PIN_GPS_EN_ACTIVE);
#endif

#ifdef P_LORA_TX_LED
  pinMode(P_LORA_TX_LED, OUTPUT);
  digitalWrite(P_LORA_TX_LED, LOW);
#endif

  delay(10);
}
