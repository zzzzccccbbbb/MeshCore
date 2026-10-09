#pragma once

#if defined(W20_MESHNOLOGY) && defined(USE_W20_DECKPRO_SHELL)

/** 默认/退出 Deck LoRa 子菜单后：MeshCore Semtech 持有射频。 */
void w20_lora_arbiter_request_mesh(void);

/** 进入 Deck LoRa 子菜单（Send/Recv Test 等）时：Deck peri_lora 持有。 */
void w20_lora_arbiter_request_deck(void);

/** 阻塞至 Deck 持有并完成 lora_init（子菜单 entry 用，超时 ms）。 */
bool w20_lora_arbiter_ensure_deck(uint32_t timeout_ms);

void w20_lora_arbiter_tick(void);

bool w20_mesh_radio_try_init(void);
void w20_mesh_radio_shutdown(void);

/** Test 页只读：Deck peri_lora 或 MeshCore Semtech 已就绪即 PASS，不抢射频。 */
bool w20_lora_hw_ok(void);

/** Setting 关 Power Lora：释放 Deck/Mesh 驱动。 */
void w20_lora_mesh_drop_on_module_power_off(void);

/** Setting 开 Power Lora：模组上电后完整 re-init MeshCore Semtech。 */
bool w20_lora_mesh_reinit_after_module_power_on(uint32_t settle_ms, uint32_t timeout_ms);

#endif
