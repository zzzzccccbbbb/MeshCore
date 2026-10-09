#pragma once
#include <Arduino.h>
#include <Wire.h>

/**
 * IP5356M I2C — migrated from w20_cp for standalone w20_meshtastic builds.
 * Register semantics per vendor datasheet (IP5356M with registers).
 *
 * 8-bit addr: write 0xEA / read 0xEB → 7-bit slave 0x75.
 */
class IP5356 {
 public:
  static constexpr uint8_t kDefaultAddr = 0x75;

  static constexpr uint8_t kRegBatVadcLo = 0x50;
  static constexpr uint8_t kRegBatVadcHi = 0x51;
  static constexpr uint8_t kRegInIadcLo = 0x54;
  static constexpr uint8_t kRegInIadcHi = 0x55;
  static constexpr uint8_t kRegVinVadcLo = 0x5C;
  static constexpr uint8_t kRegVinVadcHi = 0x5D;
  static constexpr uint8_t kRegVbusVadcLo = 0x62;
  static constexpr uint8_t kRegVbusVadcHi = 0x63;
  static constexpr uint8_t kRegNtcVadcLo = 0x64;
  static constexpr uint8_t kRegNtcVadcHi = 0x65;
  static constexpr uint8_t kRegBatIadcLo = 0x6E;
  static constexpr uint8_t kRegBatIadcHi = 0x6F;
  static constexpr uint8_t kRegSocCap = 0x7B;
  static constexpr uint8_t kRegSysState0 = 0xC4;
  static constexpr uint8_t kRegSysState3 = 0xD0;
  static constexpr uint8_t kRegChgState2 = 0xE9;

  static constexpr float kBatVoltageMvPerLsb = 0.26855f;
  static constexpr float kVinVbusVoltageMvPerLsb = 1.611328f;
  static constexpr float kInCurrentMaPerLsb = 0.671387f;
  static constexpr float kBatCurrentMaPerLsb = 1.6785f;
  static constexpr float kNtcVoltageMvPerLsb = 0.26855f;

  /** 0xE9[6:4] 充电阶段（IP5356M V1.01 §3.1 / §4.55）。 */
  static constexpr uint8_t kChgStateIdle = 0;
  static constexpr uint8_t kChgStateTrickle = 1;
  static constexpr uint8_t kChgStateCc = 2;
  static constexpr uint8_t kChgStateCv = 3;
  static constexpr uint8_t kChgStateCvDetect = 4;
  static constexpr uint8_t kChgStateFull = 5;
  static constexpr uint8_t kChgStateTimeout = 6;

  bool begin(TwoWire &wire, uint8_t intPin = 0xFF, uint8_t slaveAddr7bit = kDefaultAddr)
  {
    _wire = &wire;
    _intPin = intPin;
    _present = false;
    _addr = slaveAddr7bit;

    if (intPin != 0xFF) {
      pinMode(intPin, INPUT_PULLUP);
    }

    if (!probeAddr(_addr)) {
      return false;
    }
    _present = true;
    return true;
  }

  bool isPresent() const { return _present; }
  uint8_t address() const { return _addr; }

  int readVoltageMv()
  {
    if (!_present)
      return -1;
    uint8_t lo = 0, hi = 0;
    if (!readReg8(kRegBatVadcLo, lo))
      return -1;
    if (!readReg8(kRegBatVadcHi, hi))
      return -1;
    uint16_t raw = ((uint16_t)hi << 8) | lo;
    return (int)(raw * kBatVoltageMvPerLsb + 0.5f);
  }

  int readSocPercent()
  {
    if (!_present)
      return -1;
    uint8_t v = 0;
    if (!readReg8(kRegSocCap, v))
      return -1;
    int p = (int)v;
    if (p > 100)
      p = 100;
    return p;
  }

  int percentFromVoltage(int batMv, int emptyMv, int fullMv) const
  {
    if (batMv < emptyMv)
      return 0;
    if (batMv > fullMv)
      return 100;
    return (batMv - emptyMv) * 100 / (fullMv - emptyMv);
  }

  int readPercent(int emptyMv, int fullMv)
  {
    int soc = readSocPercent();
    if (soc >= 0)
      return soc;
    int mv = readVoltageMv();
    if (mv < 0)
      return -1;
    return percentFromVoltage(mv, emptyMv, fullMv);
  }

  struct Status {
    bool vinOk = false;
    bool vbusOk = false;
    bool inputPresent = false;
    bool charging = false;
    bool boostEn = false;
    bool vbusOv = false;
    bool vinOv = false;
    bool sinkFastChargeOk = false;
    uint8_t chgState = 0;
    uint8_t raw_c4 = 0;
    uint8_t raw_d0 = 0;
    uint8_t raw_e9 = 0;
    uint8_t raw0 = 0;
    uint8_t raw1 = 0;
  };

  bool readStatus(Status &st)
  {
    st = Status();
    if (!_present)
      return false;
    uint8_t c4 = 0, d0 = 0, e9 = 0;
    if (!readReg8(kRegSysState0, c4))
      return false;
    if (!readReg8(kRegSysState3, d0))
      return false;
    if (!readReg8(kRegChgState2, e9))
      return false;

    st.raw_c4 = c4;
    st.raw_d0 = d0;
    st.raw_e9 = e9;
    st.raw0 = c4;
    st.raw1 = e9;

    st.vinOk = (c4 & 0x80) != 0;
    st.vbusOk = ((c4 & 0x10) != 0) && ((d0 & 0x80) != 0);
    st.inputPresent = st.vinOk || st.vbusOk;
    st.charging = (e9 & 0x08) != 0;
    st.boostEn = (d0 & 0x10) != 0;
    st.vbusOv = (c4 & 0x20) != 0;
    st.sinkFastChargeOk = (e9 & 0x80) != 0;
    st.chgState = (uint8_t)((e9 >> 4) & 0x07);
    return true;
  }

  int readInputCurrentMa()
  {
    return readAdc16Scaled(kRegInIadcLo, kInCurrentMaPerLsb);
  }

  int readBatCurrentMa()
  {
    return readAdc16Scaled(kRegBatIadcLo, kBatCurrentMaPerLsb);
  }

  int readVinMv()
  {
    return readAdc16Scaled(kRegVinVadcLo, kVinVbusVoltageMvPerLsb);
  }

  int readVbusMv()
  {
    return readAdc16Scaled(kRegVbusVadcLo, kVinVbusVoltageMvPerLsb);
  }

  int readNtcMv()
  {
    return readAdc16Scaled(kRegNtcVadcLo, kNtcVoltageMvPerLsb);
  }

  /** 0xE8/0xF6[5:4] NTC 恒流源 (µA)；auto/读失败时返回 defaultUa。 */
  int readNtcSourceCurrentUa(uint8_t cfgAddr7bit, int defaultUa = 80)
  {
    uint8_t v = 0;
    if (!readReg8AtAddr(cfgAddr7bit, 0xF6, v))
      return defaultUa;
    switch ((v >> 4) & 0x03) {
    case 2:
      return 80;
    case 3:
      return 20;
    default:
      return defaultUa;
    }
  }

  bool readReg8(uint8_t reg, uint8_t &out)
  {
    if (!_present)
      return false;
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0)
      return false;
    if (_wire->requestFrom((int)_addr, 1) != 1)
      return false;
    out = (uint8_t)_wire->read();
    return true;
  }

  bool readReg16BE(uint8_t reg, uint16_t &out)
  {
    if (!_present)
      return false;
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0)
      return false;
    if (_wire->requestFrom((int)_addr, 2) != 2)
      return false;
    uint8_t hi = (uint8_t)_wire->read();
    uint8_t lo = (uint8_t)_wire->read();
    out = ((uint16_t)hi << 8) | lo;
    return true;
  }

 private:
  TwoWire *_wire = nullptr;
  uint8_t _addr = kDefaultAddr;
  uint8_t _intPin = 0xFF;
  bool _present = false;

  bool probeAddr(uint8_t addr)
  {
    _wire->beginTransmission(addr);
    return _wire->endTransmission() == 0;
  }

  int readAdc16Scaled(uint8_t regLo, float lsb)
  {
    if (!_present)
      return -1;
    uint8_t lo = 0, hi = 0;
    if (!readReg8(regLo, lo))
      return -1;
    if (!readReg8((uint8_t)(regLo + 1), hi))
      return -1;
    const uint16_t raw = ((uint16_t)hi << 8) | lo;
    return (int)(raw * lsb + 0.5f);
  }

  bool readReg8AtAddr(uint8_t addr7, uint8_t reg, uint8_t &out)
  {
    if (!_wire)
      return false;
    _wire->beginTransmission(addr7);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0)
      return false;
    if (_wire->requestFrom((int)addr7, 1) != 1)
      return false;
    out = (uint8_t)_wire->read();
    return true;
  }
};
