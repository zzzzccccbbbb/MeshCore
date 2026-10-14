# W20 MeshCore — 烧录与 WiFi OTA 指南

工程目录：`/home/zcb/platformIO/w20_meshcore`  
硬件：W20（ESP32-S3），16MB Flash，分区表 `partition-table-16MB.csv`（与 `w20_meshtastic` 对齐）。  
默认环境：`meshnology_w20_companion_radio_ble`（下文记为 `$ENV`）。

---

## 一、首次烧录（USB 串口，只需做一次）

### 前提

| 项目 | 要求 |
|------|------|
| 硬件 | W20（ESP32-S3），USB 数据线 |
| 环境 | 已安装 PlatformIO、esptool（`python -m esptool`） |
| 串口 | WSL 常见为 `/dev/ttyACM0` 或 `/dev/ttyUSB0`（以 `ls /dev/ttyACM*` 为准） |
| 工程目录 | `/home/zcb/platformIO/w20_meshcore` |

### 步骤 1：编译主固件

```bash
cd /home/zcb/platformIO/w20_meshcore
pio run -e meshnology_w20_companion_radio_ble
```

成功后会生成（路径：`.pio/build/meshnology_w20_companion_radio_ble/`）：

| 文件 | 用途 |
|------|------|
| `firmware.bin` | 仅主程序 app（烧到 `0x10000`） |
| `bootloader.bin` / `partitions.bin` | 合出厂包用 |
| `w20-firmware-ota.bin` | **后续 WiFi OTA**（`firmware.bin` 前加魔数 `0x57`） |

说明：本工程默认**不会**像 meshtastic 那样自动生成 `firmware.factory.bin`；出厂包见下文第六节手搓 / `mergebin`。

### 步骤 2：编译文件系统（LittleFS）

```bash
pio run -e meshnology_w20_companion_radio_ble -t buildfs
```

生成：

- `.pio/build/meshnology_w20_companion_radio_ble/littlefs.bin`

### 步骤 3：设备进入下载模式

1. USB 连接电脑  
2. 若烧录失败，按住 **BOOT**，点一下 **RST**，再松开 **BOOT**

### 步骤 4：全量刷写（esptool）

将 `/dev/ttyACM0` 换成实际串口。建议先擦再写：

```bash
BUILD=.pio/build/meshnology_w20_companion_radio_ble
PORT=/dev/ttyACM0

python -m esptool --chip esp32s3 -p "$PORT" -b 921600 erase-flash

python -m esptool --chip esp32s3 -p "$PORT" -b 921600 write-flash \
  0x0      "$BUILD/bootloader.bin" \
  0x8000   "$BUILD/partitions.bin" \
  0x10000  "$BUILD/firmware.bin" \
  0x910000 "$BUILD/littlefs.bin"
```

也可先打出 `firmware_full.bin`（见第六节），再一次写入：

```bash
python -m esptool --chip esp32s3 -p "$PORT" -b 921600 write-flash \
  0x0 "$BUILD/firmware_full.bin"
```

（若 `firmware_full.bin` 已含 LittleFS，则不必再单独写 `0x910000`。）

### 步骤 5：验证启动

```bash
pio device monitor -e meshnology_w20_companion_radio_ble -p /dev/ttyACM0 -b 115200
```

正常应看到设备启动、Deck 主界面，以及 WiFi OTA 相关日志（如 `[W20WifiOta]`）。  
**不应**出现 `Filesystem mount failed`（未写 LittleFS 时常这样）。

### 首次烧录写入内容一览

| 分区 | 偏移 | 文件 |
|------|------|------|
| bootloader | `0x0` | `bootloader.bin` |
| 分区表 | `0x8000` | `partitions.bin` |
| app0 | `0x10000` | `firmware.bin` |
| LittleFS（spiffs 名） | `0x910000` | `littlefs.bin` |

---

## 二、第二次及以后升级（WiFi 浏览器 OTA，无需 USB）

### 前提

| 项目 | 要求 |
|------|------|
| 设备 | 已完成首次烧录，能正常开机 |
| 网络 | 家里/办公室有 WiFi 路由器（或 SoftAP 直连） |
| 电脑 | 与设备在 **同一局域网**（路由器路径） |
| 固件包 | 必须是 **带 w20 文件名的 OTA 包**（`w20-firmware-ota.bin`），不是 full/factory |

### 步骤 1：在电脑上编译新版本固件

```bash
cd /home/zcb/platformIO/w20_meshcore
pio run -e meshnology_w20_companion_radio_ble
```

WiFi 升级要用的文件（编译后由 `extra_scripts/w20_ota_wrap.py` 自动生成）：

```text
.pio/build/meshnology_w20_companion_radio_ble/w20-firmware-ota.bin
```

该文件在 `firmware.bin` 前加了 **1 字节魔数 `0x57`（'W'）**；设备会校验首字节并剥除后再写入 OTA 分区。

**校验规则（设备端强制）：**

1. 文件名须包含 **`w20`**（不区分大小写），例如 `w20-firmware-ota.bin`  
2. 文件首字节须为 **`0x57`**

**不要用** `firmware.bin`（无魔数会被拒）、**不要用** 含 bootloader 的 full/factory 镜像。

USB 仅更新 app（分区已正确时）：

```bash
pio run -e meshnology_w20_companion_radio_ble -t upload
```

### 步骤 2：设备端连接 WiFi（路由器路径，推荐）

1. Deck 主菜单 → 点 **Wifi**  
2. 进入 **WIFI Config**（每次进入都会开 SoftAP，可重新配网）  
3. 手机连 SoftAP `Mesh-XXXXXX`，在 `http://192.168.4.1` 配网（网页顶部显示 **w20 MESHnology**）  
4. 连接成功后网页提示 **Connected**（含 IP）  
5. 或设备已保存过 WiFi，会自动连上路由器  
6. 确认屏上或配置页有 IP 地址（如 `192.168.3.124`），不是 `0.0.0.0`

设备需 **唤醒**（不要处于 Deck 软睡眠），否则 WiFi 可能已关闭。

### 步骤 2b：免路由器路径（仅 SoftAP 直连）

若尚未配网、或暂时无法连路由器，可跳过步骤 2，直接：

1. Deck 主菜单 → **Wifi** → **WiFi OTA**  
2. 设备自动开 SoftAP `Mesh-XXXXXX`，屏上显示 `http://192.168.4.1/ota`（**80 端口**，含 DNS 强制门户）  
3. 电脑/手机连接该 SoftAP，按屏上地址打开后上传固件

### 步骤 3：打开 WiFi OTA 页面

仍在 **Wifi** 菜单：

1. 点 **「- WiFi OTA」**  
2. 墨水屏会显示升级地址，例如：  
   - 已连路由器：`http://192.168.3.124:8080/ota`（**8080**）  
   - 未连路由器（SoftAP）：`http://192.168.4.1/ota`（**80**）  
3. 进入该页后，设备自动启动对应端口的 HTTP 升级服务  
4. **保持在这一页，不要返回**（离开会关闭服务）

### 步骤 4：电脑浏览器上传固件

1. **路由器路径**：电脑连接与设备 **同一 WiFi**  
2. **SoftAP 路径**：电脑连接设备 **Mesh-XXXXXX** AP  
3. 浏览器打开屏上地址  
4. 选择 **`w20-firmware-ota.bin`**（文件名须含 **w20**）  
5. 点击 **Upload and Upgrade**，等待成功提示  

### 步骤 5：等待自动重启

上传成功后设备约 **3 秒** 自动重启；可在串口或 About 核对版本号（如 `v1.3-261010C`）。

### WiFi OTA 流程简图

```text
编译 w20-firmware-ota.bin
       ↓
路径 A（路由器）：Wifi → WIFI Config → WiFi OTA（http://LAN_IP:8080/ota）
路径 B（免路由器）：Wifi → WiFi OTA（Mesh-AP → http://192.168.4.1/ota）
       ↓
浏览器选文件 → Upload and Upgrade → 自动重启
```

### 串口日志参考

```text
[W20WifiOta] listening http://192.168.3.124:8080/ota mode=STA
[W20WifiOta] Server ready
[W20WifiOta] Receiving...
[W20WifiOta] Success, rebooting
```

SoftAP 直连时：

```text
[W20 OTA] SoftAP ready Mesh-xxxxxx
[W20WifiOta] DNS captive portal on 192.168.4.1 (OTA :80)
[W20WifiOta] listening http://192.168.4.1/ota mode=AP
```

---

## 三、开发调试：后续 USB 烧录（可选）

已完成首次全量烧录、分区未变时：

| 场景 | 命令 |
|------|------|
| 仅更新主固件 | `pio run -e meshnology_w20_companion_radio_ble -t upload` |
| 主固件 + 文件系统 | `pio run -e ... -t buildfs`，再 `upload` + `uploadfs` |
| 仅写 app | `python -m esptool ... write-flash 0x10000 firmware.bin` |

---

## 四、常见问题

| 现象 | 处理 |
|------|------|
| 浏览器打不开 OTA 地址 | STA 用 `http://IP:8080/ota`（**8080**）；SoftAP 用 `http://192.168.4.1/ota`；须停在 **WiFi OTA** 页 |
| PC ping 不通设备 IP | 见下方 **「AP 隔离现场验证」**；确认同网段、未仍连 Mesh-AP |
| 上传后无法启动 | 是否误传了 full/factory 或裸 `firmware.bin`？应使用 `w20-firmware-ota.bin` |
| 提示 `Name need w20` | 重命名，文件名须包含 `w20` |
| 提示 `Bad file header` | 须带 `0x57` 首字节；重新 `pio run` 生成 `w20-firmware-ota.bin` |
| 屏上显示 `WiFi required` | SoftAP 启动失败；查串口 `[W20 OTA]` / `[WiFiProv]`，或先 **WIFI Config** |
| `Filesystem mount failed` | 全量烧录并写入 `littlefs.bin @ 0x910000` |
| 从旧分区布局升级 | 用当前 `partition-table-16MB.csv` **全片擦除重刷** 一次 |
| WSL 串口找不到 | Windows 设备管理器看 COM，或 `usbipd` 挂载 |

### AP 隔离现场验证

串口已有 `listening http://x.x.x.x:8080/ota mode=STA`，但 PC ping/浏览器失败时：

1. PC 与设备同网段，且 **未仍连着** `Mesh-xxxxxx` SoftAP  
2. 路由器路径必须用 **`:8080/ota`**，不是 `:80`  
3. 串口仍为 STA `CONNECTED`，无掉线  
4. 能 ping 网关、不能 ping 设备 → 多为路由器 **AP 隔离**；可用手机热点对照验证  

**推荐路径**：Config 配网成功 → **直接进 WiFi OTA**（勿再进 Config）→ PC 连同一路由器 → ping → 浏览器打开 `:8080/ota`。

---

## 五、两种方式对照

| | 首次烧录 | WiFi 升级（第 2 次起） |
|--|---------|----------------------|
| 连接 | USB 串口 | WiFi 局域网 |
| 工具 | esptool | 浏览器 |
| 固件文件 | boot+partitions+`firmware.bin`+`littlefs.bin`（或 `firmware_full.bin`） | `w20-firmware-ota.bin`（名含 w20 + 首字节 0x57） |
| 是否擦除 Flash | 建议全片擦除 | 否（只写 app0↔app1 另一槽） |

---

## 六、出厂固件打包方式（full）

本工程编译后默认有 `bootloader.bin` / `partitions.bin` / `firmware.bin`；**没有**自动的 `firmware.factory.bin`。按与 meshtastic `WIFI_OTA.md` 相同布局合并即可。

### 方式 A：手搓四件套 → `firmware_full.bin`（推荐出厂）

```bash
cd /home/zcb/platformIO/w20_meshcore
ENV=meshnology_w20_companion_radio_ble
pio run -e $ENV
pio run -e $ENV -t buildfs

BUILD=.pio/build/$ENV
python -m esptool --chip esp32s3 merge-bin -o "$BUILD/firmware_full.bin" \
  0x0      "$BUILD/bootloader.bin" \
  0x8000   "$BUILD/partitions.bin" \
  0x10000  "$BUILD/firmware.bin" \
  0x910000 "$BUILD/littlefs.bin"
```

烧录：

```bash
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 921600 erase-flash
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 921600 write-flash \
  0x0 .pio/build/meshnology_w20_companion_radio_ble/firmware_full.bin
```

### 方式 B：仅 boot+分区+app（不含 LittleFS）

```bash
pio run -e meshnology_w20_companion_radio_ble -t mergebin
# 生成 .pio/build/.../firmware-merged.bin（偏移布局同 esptool merge_bin）
# 仍需单独：write-flash 0x910000 littlefs.bin
```

### 与 meshtastic 对照

| | w20_meshtastic | w20_meshcore（本仓库） |
|--|----------------|------------------------|
| 分区表 | `partition-table-16MB.csv` | 相同 |
| OTA 包 | `w20-firmware-ota.bin`（`0x57`） | 相同（`w20_ota_wrap.py`） |
| 自动 `firmware.factory.bin` | 有 | **无**（用方式 A 手搓 full） |
| 安装脚本 | `bin/w20-device-install.sh` | 用上文 esptool 命令 |

（**无** espota / wifiota-s3 stub 分区。）
