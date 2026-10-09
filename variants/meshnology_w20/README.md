# Meshnology W20 — Deck Pro 菜单 + MeshCore

与 `w20_meshtastic` **同一套主菜单**（LVGL 九宫格），**唯一差别**：点 **Mesh** 进入 MeshCore companion（BLE App），而不是 Meshtastic UI。

## 菜单项（完整）

LoRa 测试 · Setting · GPS · **WiFi / OTA** · **Mesh → MeshCore** · Battery · Test · eBook · Shutdown

## 编译 / 烧录

```bash
cd ~/platformIO/w20_meshcore
pio run -e meshnology_w20_companion_radio_ble
pio run -e meshnology_w20_companion_radio_ble -t upload
```

编译成功后自动生成 WiFi 浏览器 OTA 包：

`.pio/build/meshnology_w20_companion_radio_ble/w20-firmware-ota.bin`

（首字节魔数 `0x57`，上传文件名须含 `w20`）

## 架构要点

| 层 | 路径 |
|----|------|
| Deck Pro UI + WiFi OTA | `src/platform/w20_deckpro/` |
| EPD/触摸 shell | `src/platform/w20_deckpro_shell/` |
| Meshtastic API stub | `src/platform/w20_mt_stubs/` |
| Mesh 页显示 | `W20ShellEpdDisplay` 共享 shell 墨水屏 |
| Mesh 逻辑 | MeshCore `companion_radio` + `ui-new` |

返回主菜单：原 Deck 退出 Mesh 手势 → `ui_deckpro_exit_mesh()`。
