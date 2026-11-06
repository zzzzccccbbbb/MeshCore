# Meshnology M19 (MeshCore)

ESP32-S3 + **LR2021** + **MCP23017** + **ST7789 240×240**.  
Pin/power truth source: factory firmware `~/esp32/m19/m19` (`board_pins.h`).

## Design notes

| Piece | Pattern |
|-------|---------|
| Radio / RF switch table | Like W12 (`CustomLR2021`, DIO5/6/9/10/11) |
| MCP + shared SPI mutex | Like W10 (`Mcp23017Hal`) |
| Display | W13 `ST7789LCDDisplay` framebuffer + M19 SPI lock |
| BLE / node name | W13-style `@@MAC6` (6 hex, no prefix) |
| Five-way | GPIO13 ADC ladder → `UI_HAS_JOYSTICK` (CENTER/LEFT/RIGHT); BOOT=back |

Critical differences vs W12:

- IRQ: GPIO **38** = LR2021 **DIO7** (`LR2021_IRQ_DIO=7`)
- LoRa RST / VEXT / PA / ADC_CTRL / GPS power → **MCP23017**
- TCXO **1.8 V**; default `LORA_FREQ=915.23`
- VEXT active-**low**; GNSS_CTRL active-**low**

## Build / flash

```bash
pio run -e meshnology_m19_terminal_chat          # no UI, radio smoke
pio run -e meshnology_m19_repeater               # ST7789 + repeater
pio run -e meshnology_m19_companion_radio_ble    # companion BLE
pio run -e meshnology_m19_companion_radio_ble -t upload
```

## Envs

- `meshnology_m19_terminal_chat` / `kiss_modem` — no display
- `meshnology_m19_repeater` / `room_server` / `sensor`
- `meshnology_m19_companion_radio_{usb,ble,wifi}`

## On-device checklist

1. MCP @0x20 on I2C 14/15  
2. Power: VEXT → LORA_PO → PA_EN_M (Sub-G), PA_EN_G off  
3. Serial: `radio init` OK (TCXO 1.8 V)  
4. With display env: backlight GPIO5, panel after MCP LCD_RST pulse  
5. GPS UART 43/44 @9600 after Board GNSS power-on  
6. Battery: EMA + MCP ADC_CTRL; charging icon via `HWCDC::isPlugged()`  

