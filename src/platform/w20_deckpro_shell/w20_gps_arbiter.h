#pragma once

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

void w20_gps_arbiter_request_mesh(void);
void w20_gps_arbiter_request_deck(void);

bool w20_gps_arbiter_ensure_deck(uint32_t timeout_ms);

void w20_gps_arbiter_tick(void);

bool w20_mesh_gps_try_init(void);
void w20_mesh_gps_shutdown(void);

/** Test 页只读：Deck gps_init 或 Meshtastic gps 曾初始化成功即 PASS，不抢 GPS。 */
bool w20_gps_hw_ok(void);

/** Setting 关 Power GPS：释放 Deck/Mesh GPS 驱动（delete gps）。 */
void w20_gps_mesh_drop_on_module_power_off(void);

/** Setting 开 Power GPS：模组上电后强制 Meshtastic 重新 createGps（超时 ms 内 tick）。 */
bool w20_gps_mesh_reinit_after_module_power_on(uint32_t settle_ms, uint32_t timeout_ms);

#endif
