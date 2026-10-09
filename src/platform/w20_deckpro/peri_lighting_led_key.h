#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void peri_lighting_led_key_init(void);
/** 在主循环中周期性调用（建议在 lv_task_handler 之后）；GPIO48 边沿由中断记录。 */
void peri_lighting_led_key_poll(void);
/** Setting 里手动开关照明灯时调用：与按键逻辑同步（关=OFF，开=常亮）。 */
void peri_lighting_led_key_sync_from_ui(bool lighting_on);

/** 睡眠收消息等：照明灯闪两下（非阻塞，需周期 poll）。 */
void peri_lighting_lamp_flash_twice(void);

#ifdef __cplusplus
}
#endif
