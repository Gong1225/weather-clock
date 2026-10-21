# TFT_eSPI 配置（User_Setup.h）

本工程使用 `TFT_eSPI` 库驱动 ST7789 屏幕，且引脚为非默认值，必须修改 TFT_eSPI 库的 `User_Setup.h`（位于 `Arduino/libraries/TFT_eSPI/User_Setup.h`，或你的 arduino-cli 库路径下），否则编译/显示会异常。

## 推荐配置

打开 `User_Setup.h`，**注释掉其它驱动宏**，保留以下内容：

```cpp
// ===== 驱动 =====
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// ===== 引脚（对应工程接线表）=====
#define TFT_MOSI  13      // SDA / D7
#define TFT_SCLK  14      // SCL / D5
#define TFT_CS    -1      // 未用
#define TFT_DC     0      // D3
#define TFT_RST    2      // D4
#define TFT_BL     5      // 背光 D1（也可设 -1，由工程自己的 PWM 控制）

// ===== 颜色顺序（ST7789 常用）=====
#define TFT_RGB_ORDER TFT_RGB
// 若屏幕颜色发反（红蓝颠倒），改为：
// #define TFT_RGB_ORDER TFT_BGR

// ===== 字体 =====
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
// 注：工程 UI 中文使用自带的 UIFont_20（VLW），不依赖 TFT_eSPI 内置中文字体

// 关闭 SPI 频率过高导致的不稳定（ESP8266 80MHz 下足够）
#define SPI_FREQUENCY  27000000
```

## 说明

- 工程的 GPIO 映射（SCK=14 / MOSI=13 / RES=2 / DC=0 / BL=5 / KEY=4）与 `SmallDesktopDisplay.ino` 顶部注释一致。
- 若你换了屏幕型号（如 ILI9341），把 `ST7789_DRIVER` 换成对应驱动宏，并核对宽高与颜色顺序。
- `TFT_BL` 设为 `5` 时由库控制背光；工程自身也通过 `LCD_BL_PIN 5` 做 PWM 调光，二者指向同一引脚，正常工作。
- 颜色顺序若不对（例如整屏偏色），把 `TFT_RGB_ORDER` 在 `TFT_RGB` 与 `TFT_BGR` 间切换即可。
