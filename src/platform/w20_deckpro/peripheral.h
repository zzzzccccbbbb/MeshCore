#ifndef __PERIPHERAL_H__
#define __PERIPHERAL_H__

#define GPS_PRIORITY     (configMAX_PRIORITIES - 1)
#define LORA_PRIORITY    (configMAX_PRIORITIES - 2)
#define WS2812_PRIORITY  (configMAX_PRIORITIES - 3)
#define BATTERY_PRIORITY (configMAX_PRIORITIES - 4)
#define A7682E_PRIORITY  (configMAX_PRIORITIES - 5)

enum {
    E_PERI_LORA = 0,
    E_PERI_TOUCH,
    E_PERI_KYEPAD,
    E_PERI_BQ25896,
    E_PERI_IP5356,
    E_PERI_SD,
    E_PERI_GPS,
    E_PERI_BHI260AP,
    E_PERI_LTR_553ALS,
    E_PERI_A7682E,
    E_PERI_PCM5102A,
    E_PERI_INK_SCREEN,
    E_PERI_MIC,
    E_PERI_CW2217,
    E_PERI_NUM_MAX,
};

// lora lr2021 (空口参数与 apps_configuration.h 对齐：915.23 MHz / SF12)
#define LORA_FREQ      915.23
#define LORA_MODE_SEND 0
#define LORA_MODE_RECV 1
#define LORA_MODE_CW   2
#define LORA_DEFAULT_MODE LORA_MODE_RECV
#define LORA_BW_KHZ       125.0f
#define LORA_SF           12
#define LORA_CR           5
#define LORA_SYNC_WORD    0x12
#define LORA_PREAMBLE_LEN 8
#define LORA_TX_POWER_DBM 22
#define LORA_CRC_ENABLED  true
/** Minimum time between consecutive packet starts in Send Test (ms). */
#define LORA_TX_INTERVAL_MS 3000

bool lora_init(void);
void lora_set_mode(int mode);
int lora_get_mode(void);
void lora_receive_loop(void);
void lora_transmit(const char *str);
const char *lora_get_last_sent(void);
/** Returns true once per completed over-the-air TX (for Send Test UI). */
bool lora_take_tx_for_ui(const char **out);
bool lora_get_recv(const char **str, int *rssi, float *snr);
void lora_set_recv_flag(void);
void lora_sleep(void);
/** 释放 Deck LR2021 占用的 SPI/复位线（本轮 Meshtastic Mesh 暂未支持 LR2021）。 */
void lora_release_for_mesh(void);

// keypad
#define KEYPAD_PRESS   1
#define KEYPAD_RELEASE 0

typedef void (*keypad_cb)(int state, char val);

bool keypad_init(int address);
int keypad_get_val(char *c);
void keypad_loop(void);
void keypad_regetser_cb(keypad_cb cb);
void keypad_set_flag(void);

// gyro
bool BHI260AP_init(void);
void BHI260AP_get_val(int val_type, float *x, float *y, float *z);

// LTR553
bool LTR553_init(void);
uint16_t LTR_553ALS_get_channel(int ch); // ch 0~1
uint16_t LTR_553ALS_get_ps(void);

// gps Quectel L76KB-A58
bool gps_init(void);
void gps_task_create(void);
void gps_task_suspend(void);
void gps_task_resume(void);
void gps_get_coord(double *lat, double *lng);
void gps_get_data(uint16_t *year, uint8_t *month, uint8_t *day);
void gps_get_time(uint8_t *hour, uint8_t *minute, uint8_t *second);
void gps_get_satellites(uint32_t *vsat);
void gps_get_speed(double *speed);
void gps_get_altitude(double *alt);
void gps_get_course(double *course);
void gps_get_hdop(float *hdop);
bool gps_get_fix_valid(void);
/** 挂起 Deck GPS 任务并释放 UART2，供 Meshtastic GPS 接管。 */
void gps_release_for_mesh(void);
/** 回到九宫格：挂起任务并释放 UART2（不在主页跑 GPS）。 */
void gps_prepare_idle_for_deck(void);
void gps_enable_for_screen(void);
void gps_disable_for_screen(void);

/** factory.ino：暂停 EPD flush 定时器，进入 deep sleep 前调用。 */
void factory_pause_flush_timer(void);
/** 恢复 EPD flush 定时器（OTA 上传结束后等）。 */
void factory_resume_flush_timer(void);
/** SoftAP 配网等：禁止 EPD 刷墨（比 pause 更强，render_start 也不会 resume）。 */
void factory_set_epd_flush_suppressed(bool suppressed);
bool factory_epd_flush_suppressed(void);
/** factory.ino：墨水屏 hibernate，降低睡后漏电。 */
void factory_epd_hibernate(void);
/** Sleep 保画面：仅 powerOff(0x02)，不发 0x07 深睡，减轻静置后发灰/变黑。 */
void factory_epd_hold_image(void);
/** hibernate / hold 后释放 EPD SPI 总线。 */
void factory_epd_release_bus(void);
/** Sleep 唤醒：重新初始化 deck EPD SPI（与 shell ink_screen_init_hsipi 一致）。 */
bool factory_epd_resume(void);

#endif