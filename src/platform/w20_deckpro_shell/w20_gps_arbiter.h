#pragma once

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

void w20_gps_arbiter_request_mesh(void);
void w20_gps_arbiter_request_deck(void);

bool w20_gps_arbiter_ensure_deck(uint32_t timeout_ms);

void w20_gps_arbiter_tick(void);

bool w20_mesh_gps_try_init(void);
void w20_mesh_gps_shutdown(void);

/** Test 页只读：Deck init / Meshtastic gps /（MeshCore）Setting 电源轨 ON 即 PASS，不抢 GPS。 */
bool w20_gps_hw_ok(void);

/** Setting 关 Power GPS：释放 Deck/Mesh GPS 驱动（delete gps）。 */
void w20_gps_mesh_drop_on_module_power_off(void);

/** Setting 开 Power GPS：模组上电后恢复驱动。MeshCore（无 Meshtastic GPS）仅轨+仲裁；否则 createGps。 */
bool w20_gps_mesh_reinit_after_module_power_on(uint32_t settle_ms, uint32_t timeout_ms);

#endif
