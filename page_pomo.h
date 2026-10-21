/* *****************************************************************
 * page_pomo.h - 番茄钟（全屏倒计时）
 *
 * 数据源：手机网页遥控设定时长（默认 25 分钟专注 / 5 分钟休息）。
 * 到点无蜂鸣器，用"整屏闪烁"提醒（与彩蛋闪屏同一机制）。
 *
 * 交互：双击 = 开始/暂停·继续   长按 = 回桌面
 * 渲染：复用 number.h 大数字（printfW3660）画 MM:SS；白底黑字框架。
 * *****************************************************************/
#ifndef PAGE_POMO_H
#define PAGE_POMO_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "number.h"
#include "btn.h"

extern TFT_eSPI tft;
extern TFT_eSprite clk;            // 与桌面时钟共用（.ino 定义）
extern uint16_t bgColor;           // 背景写入值 0x0000（invert(1) 下显示白）
extern const uint8_t UIFont_20[];
extern Number dig;
extern void switchPage(uint8_t p);
extern void startFlash(int times);

enum PomoMode { PM_IDLE = 0, PM_FOCUS, PM_BREAK, PM_PAUSE, PM_DONE };

static PomoMode g_pomoMode = PM_IDLE;
static int g_pomoRemain = 0;       // 剩余秒
static int g_pomoTotal = 0;        // 总秒
static bool g_pomoIsBreak = false;
static unsigned long g_pomoLastSec = 0;

static void pomoStart(int minutes, bool isBreak) {
  if (minutes < 1) minutes = 1;
  if (minutes > 180) minutes = 180;
  g_pomoTotal = minutes * 60;
  g_pomoRemain = g_pomoTotal;
  g_pomoIsBreak = isBreak;
  g_pomoMode = isBreak ? PM_BREAK : PM_FOCUS;
  g_pomoLastSec = millis();
}
static void pomoStop() { g_pomoMode = PM_IDLE; g_pomoRemain = 0; }
static void pomoPauseToggle() {
  if (g_pomoMode == PM_FOCUS || g_pomoMode == PM_BREAK) {
    g_pomoMode = PM_PAUSE;
  } else if (g_pomoMode == PM_PAUSE) {
    g_pomoMode = g_pomoIsBreak ? PM_BREAK : PM_FOCUS;
    g_pomoLastSec = millis();      // 续算，避免跳秒
  }
}

static void pomoDraw() {
  clk.setColorDepth(8);
  clk.loadFont(UIFont_20);
  tft.fillScreen(bgColor);

  const char *title = (g_pomoMode == PM_PAUSE) ? "已暂停"
                    : (g_pomoMode == PM_DONE)  ? "时间到"
                    : (g_pomoIsBreak)         ? "休息中" : "专注中";

  // 顶栏
  clk.createSprite(224, 34);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);
  clk.setTextColor(0xFFFF, bgColor);
  clk.drawString(title, 2, 6);
  clk.setTextDatum(TR_DATUM);
  clk.setTextColor(0x73AE, bgColor);
  clk.drawString(g_pomoIsBreak ? "休息" : "专注", 222, 9);
  clk.pushSprite(8, 8);
  clk.deleteSprite();
  tft.fillRect(8, 46, 224, 1, 0x18E4);

  // 大数字 MM:SS（复用桌面同款大数字，反色后屏幕上为黑）
  int mm = g_pomoRemain / 60, ss = g_pomoRemain % 60;
  int x0 = 33, y0 = 92;
  dig.printfW3660(x0, y0, mm / 10);
  dig.printfW3660(x0 + 40, y0, mm % 10);
  tft.fillRect(x0 + 82, y0 + 12, 8, 8, 0xFFFF);   // 冒号两点
  tft.fillRect(x0 + 82, y0 + 40, 8, 8, 0xFFFF);
  dig.printfW3660(x0 + 98, y0, ss / 10);
  dig.printfW3660(x0 + 138, y0, ss % 10);

  // 进度条（剩余比例）
  int prog = (g_pomoTotal > 0) ? (g_pomoRemain * 198 / g_pomoTotal) : 0;
  uint16_t barCol = g_pomoIsBreak ? (uint16_t)0x2E9BD6 : (uint16_t)0xE0802A;
  clk.createSprite(200, 8);
  clk.fillSprite(0x0000);
  clk.fillRect(1, 1, prog, 6, barCol);
  clk.pushSprite(20, 184);
  clk.deleteSprite();

  // 底部提示
  clk.createSprite(224, 22);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TC_DATUM);
  clk.setTextColor(0x73AE, bgColor);
  clk.drawString(g_pomoMode == PM_IDLE ? "双击 开始  长按 回桌面"
                : "双击 暂停/继续  长按 回桌面", 112, 1);
  clk.pushSprite(8, 214);
  clk.deleteSprite();

  clk.unloadFont();
}

static void pomoEnter() {
  tft.invertDisplay(1);             // 与桌面同框架
  tft.fillScreen(bgColor);
  pomoDraw();
}

static void pomoLoop(uint8_t ev) {
  if (ev == BEV_DOUBLE) {
    if (g_pomoMode == PM_IDLE || g_pomoMode == PM_DONE) pomoStart(25, false);
    else pomoPauseToggle();
    pomoDraw();
  } else if (ev == BEV_LONG) {
    switchPage(0);                  // 回桌面
    return;
  }
  // 每秒递减
  if ((g_pomoMode == PM_FOCUS || g_pomoMode == PM_BREAK) &&
      millis() - g_pomoLastSec >= 1000) {
    g_pomoLastSec = millis();
    g_pomoRemain--;
    if (g_pomoRemain <= 0) {
      g_pomoRemain = 0;
      g_pomoMode = PM_DONE;
      startFlash(4);                // 整屏闪烁提醒（无蜂鸣器）
    }
    pomoDraw();
  }
}

#endif
