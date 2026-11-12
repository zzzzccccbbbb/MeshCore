#include "MeshnologyW15Board.h"
#include "HWCDC.h"

#ifndef W15_BATT_SAMPLES
  #define W15_BATT_SAMPLES 16
#endif
#ifndef W15_BATT_SPIKE_MV
  #define W15_BATT_SPIKE_MV 800
#endif

bool MeshnologyW15Board::isExternalPowered() {
  /* ESP32-S3 USB-Serial/JTAG: host plugged ≈ VBUS present (same as M19). */
  return HWCDC::isPlugged();
}

uint16_t MeshnologyW15Board::getBattMilliVolts() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  uint32_t pin_mv_sum = 0;
  for (int i = 0; i < W15_BATT_SAMPLES; i++) {
    pin_mv_sum += (uint32_t)analogReadMilliVolts(PIN_VBAT_READ);
  }

  uint32_t pin_mv = pin_mv_sum / W15_BATT_SAMPLES;
  /* Factory divider 100k/100k → ADC_MULTIPLIER 2.0 */
  uint32_t mv = (uint32_t)((float)pin_mv * adc_mult);

  /* EMA + spike reject (same idea as M19 / factory). */
  if (!_batt_filt_ok || _batt_filt_mv == 0) {
    _batt_filt_mv = mv;
    _batt_filt_ok = true;
  } else {
    int32_t d = (int32_t)mv - (int32_t)_batt_filt_mv;
    if (d > W15_BATT_SPIKE_MV || d < -W15_BATT_SPIKE_MV) {
      return (uint16_t)_batt_filt_mv;
    }
    _batt_filt_mv = (_batt_filt_mv * 7 + mv) / 8;
  }
  return (uint16_t)_batt_filt_mv;
}
