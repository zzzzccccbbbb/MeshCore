# W20 烧录与 WiFi OTA

工程：`/home/zcb/platformIO/w20_meshcore`  
环境：`meshnology_w20_companion_radio_ble`  
硬件：W20（ESP32-S3，16MB Flash），分区表 `partition-table-16MB.csv`  
产物目录（下文称 **BUILD**）：

```text
.pio/build/meshnology_w20_companion_radio_ble/
```

串口示例：`/dev/ttyACM0`（以 `ls /dev/ttyACM*` / `ls /dev/ttyUSB*` 为准）。

---

## 一、首次：编译

```bash
cd /home/zcb/platformIO/w20_meshcore

# buildfs 需要源目录 data/（可为空；LittleFS 镜像仍输出到 BUILD）
mkdir -p data

pio run -e meshnology_w20_companion_radio_ble
pio run -e meshnology_w20_companion_radio_ble -t buildfs
```

成功后 BUILD 中会有：

| 文件 | 用途 |
|------|------|
| `bootloader.bin` | 全量烧录 |
| `partitions.bin` | 全量烧录 |
| `firmware.bin` | 全量烧录（主程序 app） |
| `littlefs.bin` | 全量烧录（文件系统） |
| `w20-firmware-ota.bin` | **仅** 二次 WiFi OTA 用 |

说明：

- `data/` 是打进 LittleFS 的**内容源**；镜像产物是 BUILD 里的 `littlefs.bin`，与其它 bin **同目录**。
- `buildfs` **不会**清掉 `firmware.bin` / `w20-firmware-ota.bin`；会整目录清掉的是 `pio run -t clean`。

---

## 二、首次：全量打包并 USB 烧录

在 **BUILD** 目录合并出厂镜像，再擦写整片 Flash：

```bash
cd /home/zcb/platformIO/w20_meshcore/.pio/build/meshnology_w20_companion_radio_ble

python -m esptool --chip esp32s3 merge-bin -o firmware_full.bin \
  0x0      bootloader.bin \
  0x8000   partitions.bin \
  0x10000  firmware.bin \
  0x910000 littlefs.bin

PORT=/dev/ttyACM0   # 按实际修改

# 建议先擦再写（首次 / 改过分区表时）
python -m esptool --chip esp32s3 -p "$PORT" -b 921600 erase-flash
python -m esptool --chip esp32s3 -p "$PORT" -b 921600 write-flash \
  0x0 firmware_full.bin
```

也可不合并，直接四件套写入（偏移相同）：

```bash
python -m esptool --chip esp32s3 -p "$PORT" -b 921600 write-flash \
  0x0      bootloader.bin \
  0x8000   partitions.bin \
  0x10000  firmware.bin \
  0x910000 littlefs.bin
```

烧录失败时：按住 **BOOT** → 点一下 **RST** → 松开 **BOOT**，再重试。

验证：

```bash
cd /home/zcb/platformIO/w20_meshcore
pio device monitor -e meshnology_w20_companion_radio_ble -p /dev/ttyACM0 -b 115200
```

应能正常启动、进 Deck 主界面；若未写 LittleFS，常见 `Filesystem mount failed`。

| 分区 | 偏移 | 文件 |
|------|------|------|
| bootloader | `0x0` | `bootloader.bin` |
| 分区表 | `0x8000` | `partitions.bin` |
| app0 | `0x10000` | `firmware.bin` |
| LittleFS | `0x910000` | `littlefs.bin` |

---

## 三、二次：WiFi OTA（只更新 app）

日常改代码后，一般**不必再全刷**，只编固件并用浏览器升级：

```bash
cd /home/zcb/platformIO/w20_meshcore
pio run -e meshnology_w20_companion_radio_ble
# 使用 BUILD/w20-firmware-ota.bin
```

### 步骤（两种路径，进页自动选）

| 设备状态 | 行为 | 你怎么连 |
|----------|------|----------|
| **已连路由器**（有 LAN IP） | STA 局域网 OTA | 手机/PC 与设备同一 WiFi，打开屏上 `http://192.168.x.x:8080/ota` |
| **未联网** | SoftAP OTA（通常更快） | 连屏上 **`Mesh-xxxxxx`** 热点，打开 `http://192.168.4.1:8080/ota` |

1. 设备菜单：**Wifi → WiFi OTA**（有 STA 则保持联网；无 STA 则起 SoftAP）。
2. 按屏上提示连网并打开 URL：
   - SoftAP：屏显 **`WiFi: Mesh-xxxxxx`** + `http://192.168.4.1:8080/ota`
   - 已联网：屏显局域网 `http://192.168.x.x:8080/ota`
3. 上传 **`w20-firmware-ota.bin`**（BUILD 目录；文件名须含 **`w20`**）。
4. **保持停在 WiFi OTA 页**直到成功并自动重启。

进页偏中小弹层 **`ERASING FLASH` / `Do not touch (~20s)`**（上方保留 WiFi/URL；不重复 Status/Progress）；预擦期间墨水停刷。擦完自动收起弹层并刷 **`Server ready`** + SoftAP/LAN 升级 URL。此间请勿操作（返回可能要等当前擦块结束）。经路由器 STA 可能偏慢；要快可先 Disconnect 再用 SoftAP。

### 注意

| 可以 | 不可以 |
|------|--------|
| `w20-firmware-ota.bin`（首字节魔数 `0x57`，`pio run` 自动生成） | 裸 `firmware.bin`（无魔数会被拒） |
| | `firmware_full.bin` / factory（含 bootloader，不能网页 OTA） |

上传中触控可能较卡；网页中途断开会中止本次写 Flash（`Upload aborted`），一般可重新打开页面再传。要彻底退出 OTA，返回离开该页即可。

进 WiFi/OTA 会 **shutdown BLE controller**（可再恢复）；离开 WiFi 菜单后 BLE 会重新 begin。

### 常见问题

| 现象 | 处理 |
|------|------|
| SoftAP 打不开 | 手机须连 **Mesh-xxx**（不是家里 WiFi）；URL 带 **:8080** |
| LAN 打不开 / 极慢 | 确认同网段；路由器 AP 隔离会挡；可 Disconnect 后走 SoftAP |
| `Name need w20` | 文件名须包含 `w20` |
| `Bad file header` | 须用 `w20-firmware-ota.bin`，不要传裸 `firmware.bin` |
| 从旧分区布局升级 | 用当前分区表 **全片擦除 + 第二节全量烧录** 一次 |

---

## 流程对照

```text
首次：
  mkdir -p data → pio run → buildfs
       → merge-bin → firmware_full.bin
       → erase-flash + write-flash @ 0x0

二次（日常）：
  pio run → 浏览器上传 w20-firmware-ota.bin
```

| | USB 全量 | WiFi OTA |
|--|----------|----------|
| 文件 | `firmware_full.bin` 或四件套 | `w20-firmware-ota.bin` |
| 是否擦片 | 建议 `erase-flash` | 否（只写另一 app 槽） |
| 何时用 | 新机、改分区、FS 损坏 | 日常固件迭代 |
