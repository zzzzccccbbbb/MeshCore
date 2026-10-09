#pragma once

// W20 LoRa LR2021 pinout — keep in sync with variants/esp32s3/w20-meshnology/variant.h
// MISO=11 MOSI=13 SCK=12 NSS=1 NRST=14 BUSY=4 DIO1=5 EN=39(ME6231C33M5G→LORA3V3)
#define W20_LORA_EN_PIN     39
#define W20_LORA_SCLK_PIN   12
#define W20_LORA_MOSI_PIN   13
#define W20_LORA_MISO_PIN   11
#define W20_LORA_CS_PIN     1   /* NSS */
#define W20_LORA_RST_PIN    14  /* NRST */
#define W20_LORA_BUSY_PIN   4
#define W20_LORA_IRQ_PIN    5   /* schematic DIO1 → chip DIO9 */
