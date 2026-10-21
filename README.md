# 天气时钟（Weather Clock）

一个运行在 ESP8266 上的桌面天气时钟。基于 240×240 ST7789 彩屏，提供**天气时钟桌面、一周天气、番茄钟、倒数日**四个页面，支持**手机网页遥控**与**闪屏提醒**，所有配置（亮度 / 城市 / 倒数日）掉电保存。

> 开源版本已移除全部上游原作者标识与版本水印，仅保留纯净可复刻的工程。

---

## 功能特性

- **四页界面**：天气时钟桌面、一周天气、番茄钟、倒数日，可循环切换。
- **实时天气**：桌面显示当前温度 / 湿度 / 空气质量与滚动天气文案；一周天气调用 Open-Meteo 免费接口（无需密钥）。
- **番茄钟**：专注 / 休息计时，到点整屏闪烁提醒（设备无蜂鸣器，用反色闪屏代替声音）。
- **倒数日**：手机端增删事项，屏幕按剩余天数升序展示。
- **手机遥控**：连入同一 WiFi 后用浏览器打开设备 IP 即可调亮度、切页、刷天气、启停番茄钟、增删倒数日。
- **掉电保存**：亮度、城市代码、倒数日写入 EEPROM。
- **上电默认桌面**：每次重新上电稳定停在桌面页，不会误跳其它页。

---

## 硬件清单

| 部件 | 说明 |
| --- | --- |
| 主控 | ESP8266 开发板（NodeMCU / ESP-12E 模块） |
| 屏幕 | 1.3" / 1.54" ST7789 240×240 TFT（3.3V 逻辑） |
| 按键 | 轻触开关 1 个（接 GPIO4，另一侧接地，启用内部上拉） |
| 其它 | 面包板 / 杜邦线、3.3V 供电（USB 即可） |

### 接线表

| 屏幕引脚 | ESP8266 GPIO | NodeMCU 丝印 |
| --- | --- | --- |
| SCK / CLK | GPIO14 | D5 |
| MOSI / SDA | GPIO13 | D7 |
| RES / RST | GPIO2 | D4 |
| DC | GPIO0 | D3 |
| BL（背光） | GPIO5 | D1 |
| CS | 未用（代码置 -1） | — |
| VCC | 3.3V | 3V3 |
| GND | GND | GND |
| 按键 | GPIO4（内拉上拉） | D2 |

> 注意：ESP8266 为 3.3V 器件，屏幕必须为 3.3V 逻辑电平，勿直接接 5V。

---

## 软件依赖

- **Arduino IDE ≥ 1.8** 或 **arduino-cli**
- 开发板包：**esp8266 by ESP8266 Community**（测试版本 2.7.4），板型选 **NodeMCU 1.0 (ESP-12E Module)**
- 库：
  - `TFT_eSPI`（显示驱动，**需按 `docs/tft_setup.md` 配置 `User_Setup.h`**）
  - `TJpg_Decoder`（天气图标 JPEG 解码）
  - `ArduinoJson`（解析天气接口）
  - `TimeLib`（NTP 时间）
  - 其余 `ESP8266WiFi` / `ESP8266HTTPClient` / `WiFiUdp` / `ESP8266WebServer` / `SPI` / `EEPROM` 为 ESP8266 核心自带

---

## 编译与烧录

### 方式一：arduino-cli

```bash
# 编译
arduino-cli compile --fqbn esp8266:esp8266:nodemcu --output-dir build SmallDesktopDisplay

# 烧录（COM 口按你的系统调整，Windows 多为 COMx，Linux/macOS 为 /dev/ttyUSBx）
arduino-cli upload -p COM3 --fqbn esp8266:esp8266:nodemcu --input-dir build SmallDesktopDisplay
```

### 方式二：Arduino IDE

1. 安装上述开发板包与库；
2. 工具 → 开发板选 **NodeMCU 1.0 (ESP-12E Module)**；
3. 工具 → Flash Size 选 **4MB (FS:none OTA:~1019KB)**，CPU Frequency 80MHz，Upload Speed 115200；
4. 按 `docs/tft_setup.md` 改好 `TFT_eSPI/User_Setup.h`；
5. 烧录。

固件体积约 780KB，在 4MB 板型的应用分区上限内。

---

## 配网与固定 IP

打开 `SmallDesktopDisplay.ino`，修改顶部 WiFi 信息（开源版本为占位符，请改成你自己的）：

```cpp
const char ssid[] = "你的WiFi名称";   // 仅支持 2.4GHz
const char pass[] = "你的WiFi密码";
```

设备固定使用局域网 IP `192.168.1.10`（网关 `192.168.1.1`）。若你的路由器网段不同，请同步修改 `setup()` 里的：

```cpp
IPAddress local_IP(192,168,1,10), gateway_IP(192,168,1,1), subnet_IP(255,255,255,0);
IPAddress dns1(192,168,1,1), dns2(223,5,5,5);
WiFi.config(local_IP, gateway_IP, subnet_IP, dns1, dns2);
```

默认城市为**西安**（桌面天气按城市代码拉取；一周天气坐标硬编码为西安，见 `page_week.h` 的 `WEEK_URL`，换城市改其中的 `latitude` / `longitude` 即可）。

---

## 手机遥控

手机连入同一 WiFi 后，浏览器打开 `http://192.168.1.10`，出现控制面板：

- **亮度**：拖动滑块即时生效；
- **页面**：桌面 / 天气 / 番茄 / 倒数日 一键切换；
- **刷新天气**：重新拉取桌面与一周天气；
- **番茄钟**：专注 25 / 专注 45 / 休息 5 / 停止；
- **倒数日**：输入名称与日期后「添加」，列表项右侧「删」可移除。

接口也可被其它程序直接调用，见 `docs/web-api.md`。

---

## 页面与按键交互

设备侧面只有一个按键，通过**单击 / 双击 / 长按**组合操作：

| 页面 | 单击 | 双击 | 长按 |
| --- | --- | --- | --- |
| 天气时钟桌面 (0) | 进一周天气 | 刷新天气 | — |
| 一周天气 (1) | 进番茄钟 | 刷新本周天气 | 回桌面 |
| 番茄钟 (2) | — | 开始 / 暂停 | 回桌面 |
| 倒数日 (3) | 回桌面 | 刷新 | 回桌面 |

> 实体按键循环为：桌面 → 一周天气 → 番茄钟 → 桌面。**倒数日页通过手机遥控面板的「倒数日」按钮进入**（当前固件实体按键未挂接跳到倒数日的路径）。

**闪屏提醒**：番茄钟倒计时归零时，整屏反色闪烁 4 次提示（无蜂鸣器）。

---

## 字库说明

UI 中文使用 `font/UIFont_20.h`（TFT_eSPI VLW 格式，封闭字集，已包含天气 / 番茄钟 / 倒数日全部固定词）。该文件由 `tools/genfonts.py` 从 Windows 自带的微软雅黑 `msyh.ttc` 生成：

```bash
pip install Pillow
python tools/genfonts.py        # 输出到 font/UIFont_20.h
```

若你新增了页面文案导致某个字渲染成空白，重跑脚本即可把新字补进字库。

大数字（时钟 / 番茄计时）使用 `font/` 下 `O_3660_*`、`W_3660_*`、`W_1830_*` 三套 JPEG 位图精灵；天气图标在 `img/`（温度、湿度、`pangzi/` 卡通、`tianqi/` 天气现象）。

---

## EEPROM 布局

| 内容 | 起始地址 | 说明 |
| --- | --- | --- |
| 亮度 | 1 | 0–100，有效范围外不采纳 |
| 城市代码 | 10 | 桌面天气城市代码（如西安 101110113） |
| 倒数日 | 20 | 最多 4 条，每条 24 字节（名称 13 + 日期 11） |

---

## 目录结构

```
SmallDesktopDisplay/
├── SmallDesktopDisplay.ino   # 主程序（入口、配网、网页服务、闪屏、页面分发）
├── btn.h                     # 按键手势（消抖 + 上电解锁保护 + 单击/双击/长按）
├── page_week.h               # 页面：一周天气（Open-Meteo）
├── page_pomo.h               # 页面：番茄钟
├── page_countdown.h          # 页面：倒数日（EEPROM 存储）
├── number.h / number.cpp     # 大数字 JPEG 精灵绘制
├── weathernum.h / weathernum.cpp  # 天气现象图标绘制
├── qr1b.h                    # 配网提示二维码（1bpp 压缩）
├── font/                     # UI 平滑字体 + 大数字位图精灵
├── img/                      # 温度/湿度图标、卡通、天气现象图标
└── tools/
    └── genfonts.py           # UI 字库生成工具
```

---

## 开源协议

本项目以 **MIT 协议** 开源，详见 `LICENSE`。

## 免责声明

- 本工程用于学习与非商业用途，使用者需自行确保硬件接线正确，因接线或改装导致的设备损坏作者不承担责任。
- 天气数据来自第三方公开接口（Open-Meteo、天气网），其可用性、准确性与条款由对应服务提供方负责。
