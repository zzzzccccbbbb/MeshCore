#include <Arduino.h>
#include <Mesh.h>
#include <helpers/TxtDataHelpers.h>
#include "MyMesh.h"

static uint32_t _atoi(const char* sp) {
  uint32_t n = 0;
  while (*sp && *sp >= '0' && *sp <= '9') {
    n *= 10;
    n += (*sp++ - '0');
  }
  return n;
}

#include <helpers/MultiSerialInterface.h>
MultiSerialInterface interface_manager;

#if defined(BLE_PIN_CODE)
  #ifdef ESP32
    #include <helpers/esp32/SerialBLEInterface.h>
    SerialBLEInterface bluetooth_interface;
  #else
    #error "SerialBLEInterface is not defined for this platform"
  #endif
#endif

#ifdef WIFI_SSID
  #ifndef TCP_PORT
    #define TCP_PORT 5000
  #endif
  #ifdef ESP32
    #include <helpers/esp32/SerialWifiInterface.h>
    SerialWifiInterface wifi_interface;
  #else
    #error "SerialWifiInterface is not defined for this platform"
  #endif
#endif

#if defined(ENABLE_USB_INTERFACE)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface usb_serial_interface;
#endif

#if defined(SERIAL_RX)
  #include <helpers/ArduinoSerialInterface.h>
  ArduinoSerialInterface hardware_serial_interface;
  HardwareSerial companion_serial(1);
#endif

#if defined(ESP32)
  #if defined(MESHNOLOGY_W20)
    #include <LittleFS.h>
    DataStore store(LittleFS, rtc_clock);
  #else
    #include <SPIFFS.h>
    DataStore store(SPIFFS, rtc_clock);
  #endif
#else
  #error "W20 MeshCore deck build requires ESP32"
#endif

#ifdef DISPLAY_CLASS
  #include "UITask.h"
  UITask ui_task(&board, &interface_manager);
#endif

#if defined(USE_W20_DECKPRO_SHELL)
  #include "platform/w20_deckpro_shell/w20_deckpro_shell.h"
  #include "platform/w20_deckpro_shell/w20_meshcore_ui.h"
#endif

StdRNG fast_rng;
SimpleMeshTables tables;
MyMesh the_mesh(radio_driver, fast_rng, rtc_clock, tables, store
   #ifdef DISPLAY_CLASS
      , &ui_task
   #endif
);

void halt() {
  while (1) ;
}

#if defined(ESP32) && defined(WIFI_SSID)
  bool wifi_needs_reconnect = false;
  unsigned long last_wifi_reconnect_attempt = 0;
#endif

void setup() {
  Serial.begin(115200);

#if defined(MESHNOLOGY_W20)
  /* Meshtastic W20: EN/CS before splash so LORA3V3 settles during EPD draw */
  #include "platform/w20_deckpro/utilities.h"
  #include <Wire.h>
  #include <driver/gpio.h>
  /* Soft-sleep may leave gpio_hold on EN/NSS; release before digitalWrite. */
  gpio_hold_dis((gpio_num_t)P_LORA_EN);
  gpio_hold_dis((gpio_num_t)P_LORA_NSS);
  gpio_deep_sleep_hold_dis();
  pinMode(P_LORA_EN, OUTPUT);
  digitalWrite(P_LORA_EN, HIGH);
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(BOARD_EPD_CS, OUTPUT);
  digitalWrite(BOARD_EPD_CS, HIGH);
  Serial.printf("W20 boot: LORA_EN=%d NSS=%d (pre-splash)\n",
                digitalRead(P_LORA_EN), digitalRead(P_LORA_NSS));
#endif

#if defined(MESHNOLOGY_W20)
  /* Wire 尽早：棕断弱电门禁需读 CW/IP；正常路径供 radio_init RTC */
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
#endif

#if defined(USE_W20_DECKPRO_SHELL)
  /* 棕断锁存：刷 Low battery off 后 deep sleep，打断闪屏环（不返回） */
  w20_deckpro_boot_lowbatt_halt_if_needed();
  /* 极早 splash（与 meshtastic W20 一致），再做射频/mesh 长初始化 */
  w20_deckpro_early_splash();
#endif

#if defined(MESHNOLOGY_W20)
  /* EN already high during splash (~1.4s settle). Idle NSS/BUSY before HAL claims SPI. */
  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);
  pinMode(P_LORA_RESET, OUTPUT);
  digitalWrite(P_LORA_RESET, HIGH);
  pinMode(P_LORA_BUSY, INPUT_PULLUP);
  const bool radio_ok = radio_init();
  if (!radio_ok) {
    Serial.printf("WARNING: radio_init failed; EN=%d continuing Deck without Mesh RF\n",
                  digitalRead(P_LORA_EN));
  }
  board.begin();
#else
  board.begin();
  const bool radio_ok = radio_init();
  if (!radio_ok) {
    Serial.println("WARNING: radio_init failed; booting UI without LoRa/Mesh RF");
  }
#endif

  // getRngSeed needs a live radio; fall back so identity still works offline
  fast_rng.begin(radio_ok ? radio_driver.getRngSeed() : (uint32_t)esp_random());

#if defined(MESHNOLOGY_W20)
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR: LittleFS mount failed");
  } else {
    LittleFS.mkdir("/identity");
    LittleFS.mkdir("/bl");
  }
#else
  SPIFFS.begin(true);
#endif
  store.begin();
  the_mesh.begin(
#if defined(USE_W20_DECKPRO_SHELL)
      true
#elif defined(DISPLAY_CLASS)
      false /* display.begin deferred to shell */
#else
      false
#endif
  );

#if defined(BLE_PIN_CODE)
  {
    /* 对齐 w10 companion：节点名/BLE 用公钥前 4 字节 hex（8 位），去掉 Meshnology-XXXX */
    NodePrefs *prefs = the_mesh.getNodePrefs();
    const bool legacy = (strncmp(prefs->node_name, "Meshnology", 10) == 0) ||
                        (strcmp(prefs->node_name, "@@MAC4") == 0) ||
                        (strcmp(prefs->node_name, "@@MAC") == 0) ||
                        (strcmp(prefs->node_name, "NONAME") == 0) ||
                        (strcmp(prefs->node_name, "Meshnology-W20") == 0);
#ifdef ADVERT_NAME
    const bool legacy_advert = (strcmp(prefs->node_name, ADVERT_NAME) == 0);
#else
    const bool legacy_advert = false;
#endif
    if (legacy || legacy_advert) {
      char pub_key_hex[10];
      mesh::Utils::toHex(pub_key_hex, the_mesh.self_id.pub_key, 4);
      StrHelper::strncpy(prefs->node_name, pub_key_hex, sizeof(prefs->node_name));
      the_mesh.savePrefs();
    }
    bluetooth_interface.begin(BLE_NAME_PREFIX, prefs->node_name, the_mesh.getBLEPin());
  }
  interface_manager.addInterface(InterfaceType::Bluetooth, &bluetooth_interface);
#endif

#ifdef WIFI_SSID
  board.setInhibitSleep(true);
  WiFi.setAutoReconnect(true);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
      if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
          wifi_needs_reconnect = true;
      } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
          wifi_needs_reconnect = false;
      }
  });
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  wifi_interface.begin(TCP_PORT);
  interface_manager.addInterface(InterfaceType::WiFi, &wifi_interface);
#endif

#if defined(ENABLE_USB_INTERFACE)
  usb_serial_interface.begin(Serial);
  interface_manager.addInterface(InterfaceType::USB, &usb_serial_interface);
#endif

#if defined(SERIAL_RX)
  companion_serial.setPins(SERIAL_RX, SERIAL_TX);
  companion_serial.begin(115200);
  hardware_serial_interface.begin(companion_serial);
  interface_manager.addInterface(InterfaceType::HardwareSerial, &hardware_serial_interface);
#endif

  the_mesh.startInterface(interface_manager);
  sensors.begin();

#if ENV_INCLUDE_GPS == 1
  the_mesh.applyGpsPrefs();
#endif

#if defined(USE_W20_DECKPRO_SHELL)
  w20_deckpro_shell_init();
  w20_meshcore_ui_setup(&display, &sensors, the_mesh.getNodePrefs());
#elif defined(DISPLAY_CLASS)
  if (display.begin()) {
    ui_task.begin(&display, &sensors, the_mesh.getNodePrefs());
  }
#endif

  board.onBootComplete();
}

void loop() {
  the_mesh.loop();
  interface_manager.loop();
  sensors.loop();

#if defined(USE_W20_DECKPRO_SHELL)
  w20_deckpro_shell_loop();
#elif defined(DISPLAY_CLASS)
  ui_task.loop();
#endif

  rtc_clock.tick();

#if defined(ESP32) && defined(WIFI_SSID)
  if (wifi_needs_reconnect && (millis() - last_wifi_reconnect_attempt > 10000)) {
    WiFi.disconnect();
    WiFi.reconnect();
    last_wifi_reconnect_attempt = millis();
  }
#endif
}
