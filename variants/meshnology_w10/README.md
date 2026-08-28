# Meshnology W10

ESP32-S3 + Semtech **SX1262** + MCP23017 IO expander + 3.5" ST7796 **240×240 square** LCD + AXP2101 PMIC.

## Environments

| Env | Notes |
|-----|--------|
| `meshnology_w10_repeater` | Repeater + ST7796 UI |
| `meshnology_w10_companion_radio_usb` | Companion over USB Serial |
| `meshnology_w10_companion_radio_ble` | Companion over BLE |
| `meshnology_w10_companion_radio_wifi` | Companion over WiFi |
| `meshnology_w10_room_server` | Room server (+ GPS UART optional) |

Default RF: MeshCore root `LORA_FREQ=869.618` (868 MHz band).

## Hardware notes

- **LoRa**: SX1262 with DIO2 as RF switch (crystal, TCXO=0); XTA/XTB trim `0x17` from factory.
- **LoRa control pins** (RST / DIO1 / BUSY) are on **MCP23017** EXIO3/9/10, not MCU GPIO. RadioLib access goes through `Mcp23017Hal`.
- **SPI2 is shared** between LoRa (CS=14) and LCD (CS=10). Board exposes `lockSharedSpi()` / `unlockSharedSpi()`; radio HAL and `ST7796Display` both take the mutex around SPI transactions.
- **Display**: ST7796 240×240 square panel (same as factory), not round.
- **Deep sleep**: DIO1 is not an RTC GPIO, so packet-wakeup via `ext1` is **not available**. Wake sources are timer and/or user button (GPIO0).
- **AXP2101**: battery millivolts via `getBattMilliVolts()` when the PMIC is present.
- **GPS** (room_server): UART 43/44; RST/WAKE via MCP23017 EXIO11/12 when `ENV_INCLUDE_GPS=1`.

## Build

```bash
pio run -e meshnology_w10_repeater
pio run -e meshnology_w10_companion_radio_ble
```
