#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Start periodic SOS broadcast (Primary channel) while LED blink mode is active. */
void w20_sos_beacon_start(void);
/** Stop SOS broadcast. */
void w20_sos_beacon_stop(void);
/** Call from peri_lighting_led_key_poll(); sends every ~2.5 s when active and mesh group ready. */
void w20_sos_beacon_poll(void);
bool w20_sos_beacon_active(void);
/** 频道收到文本：别人的消息则结束 SOS 并关灯。 */
void w20_sos_on_channel_text(const char *text);
/** 收到私聊/其它对端消息时结束 SOS（无 name 前缀可解析时用）。 */
void w20_sos_on_peer_message(void);

#ifdef __cplusplus
}
#endif
