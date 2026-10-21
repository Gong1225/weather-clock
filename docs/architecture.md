# 架构说明

## 整体结构

工程是一个单 `.ino` + 多 `.h` 页面的 Arduino 程序，编译为单一固件烧录进 ESP8266。

```
SmallDesktopDisplay.ino   入口：setup / loop / 配网 / NTP / 网页服务 / 闪屏 / 页面分发
btn.h                     按键手势状态机
page_week.h               一周天气页
page_pomo.h               番茄钟页
page_countdown.h          倒数日页（EEPROM 持久化）
number.* / weathernum.*   大数字与天气图标绘制
font/                      UI 平滑字体(UIFont_20) + 大数字 JPEG 精灵
img/                      温度/湿度/卡通/天气现象图标
tools/genfonts.py         字库生成
```

## 页面系统

页面编号为全局 `curPage`：

| 编号 | 页面 |
| --- | --- |
| 0 | 桌面（天气时钟） |
| 1 | 一周天气 |
| 2 | 番茄钟 |
| 3 | 倒数日 |

`switchPage(p)` 负责按目标页调用对应 `enter` 函数重绘。`loop()` 里根据 `curPage` 把按键事件 `ev` 分发给各页的 `xxxLoop(ev)`。

各页通过 `extern void switchPage(uint8_t)` 互相跳转，构成循环：
桌面 →（单击）一周天气 →（单击）番茄钟 →（长按）桌面；倒数日由手机遥控进入后（单击/长按）回桌面。

## 按键手势（btn.h）

单 GPIO（GPIO4，内部上拉，按下接地）通过状态机区分三种手势：

- **稳定电平过滤**：电平需稳定 ≥30ms 才被采纳，过滤长按过程中的触点抖动（这是早期“一周天气长按偶发回桌面”的根因）。
- **上电解锁保护**：上电后直到首次检测到“松开”才解锁，忽略上电瞬间的电平毛刺，确保每次上电停在桌面、不误跳页。
- **单击 / 双击 / 长按**：
  - 单击：按下→松开，且 280ms 内无第二次按下；
  - 双击：280ms 窗口内两次单击；
  - 长按：按住 ≥2000ms。
  - 长按释放后 700ms 内的误触单击会被吞掉，避免长按结束的抖动被当成一次跳转。

## 反色框架与闪屏

屏幕以 `tft.invertDisplay(1)` 进入反色框架：代码里写入的颜色按“显示值取反”理解（写 `0x0000` 显示白、写 `0xFFFF` 显示黑）。该方式对反色漂移免疫，所有页面统一。

闪屏提醒 `startFlash(times)` + `tickFlash()`：`tickFlash()` 在 `loop()` 里周期性切换 `invertDisplay(0/1)`，实现整屏反色闪烁，用于番茄钟到点等无声音提醒。

## 网页遥控服务

`ESP8266WebServer server(80)` 在 `setupWeb()` 注册接口（见 `docs/web-api.md`）。

关键约束（详见 web-api.md）：**handler 内禁止发起外网 HTTP**，外网拉取统一延迟到 `loop()` 通过 `g_needWeather` / `g_needWeek` 标志执行，否则会破坏 ESP8266 WiFi 栈导致重启。

## EEPROM 布局

| 内容 | 起始地址 | 说明 |
| --- | --- | --- |
| 亮度 | 1 | 0–100，有效范围外不采纳 |
| 城市代码 | 10 | 桌面天气城市代码 |
| 倒数日 | 20 | 最多 4 条，每条 24 字节（名称 13 + 日期 11） |

倒数日结构体 `CDEntry { char name[13]; char date[11]; }`，`cdLoad()` 会把未初始化（`0xFF` 首字节）的槽位清空为无效，避免历史残值被当成有效条目。

## 字库

`font/UIFont_20.h` 为 TFT_eSPI VLW 格式封闭字集（约 315 字），覆盖桌面 / 一周天气 / 番茄钟 / 倒数日全部固定 UI 词。由 `tools/genfonts.py` 从微软雅黑生成，新增页面文案后重跑即可补字。

大数字（时钟、番茄计时）使用 `font/` 下 `O_3660_*`（橙）、`W_3660_*`（白）、`W_1830_*`（白小号）三套 JPEG 精灵，由 `number.cpp` 调用 `TJpg_Decoder` 绘制。
