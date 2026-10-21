/* *****************************************************************
 * page_countdown.h - 倒数日 / 待办
 *
 * 数据：EEPROM 存最多 4 条（名称 ≤12 汉字 + 日期 YYYY-MM-DD）。
 * 由手机网页遥控增删；按剩余天数升序展示，最近一条浅底纹高亮。
 *
 * 交互：双击 = 刷新（重读 EEPROM）  单击/长按 = 回桌面
 * 渲染：平滑字体 UIFont_20 + 白底黑字框架（与一周天气同款）。
 * *****************************************************************/
#ifndef PAGE_CD_H
#define PAGE_CD_H

#include <Arduino.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
#include <TimeLib.h>
#include "btn.h"

extern TFT_eSPI tft;
extern TFT_eSprite clk;            // 与桌面时钟共用（.ino 定义）
extern uint16_t bgColor;           // 背景写入值 0x0000（invert(1) 下显示白）
extern const uint8_t UIFont_20[];
extern void switchPage(uint8_t p);

#define CD_ADDR 20                  // EEPROM 起始（亮度@1、城市@10 之后）
#define CD_MAX  4
#define CD_ENT  24                  // 每条：名称13 + 日期11 字节

static struct CDEntry { char name[13]; char date[11]; } g_cd[CD_MAX];

/* 公历日期 → 距纪元天数（仅用于差值，绝对基准无所谓） */
static long cdDays(int y, int m, int d) {
  y -= (m <= 2) ? 1 : 0;
  int er = y / 4 - y / 100 + y / 400;
  int mm = (m + 9) % 12;
  int yy = y - mm / 10;
  return 365L * y + er + (mm * 306 + 5) / 10 + (d - 1);
}
/* 距今天数（>0 未到，=0 今天，<0 已过） */
static long cdUntil(const char *date) {
  int y = atoi(date), m = atoi(date + 5), d = atoi(date + 8);
  if (y < 2000 || m < 1 || m > 12 || d < 1 || d > 31) return 0x7fffffff;
  return cdDays(y, m, d) - cdDays(year(), month(), day());
}

static void cdSave() {
  for (int i = 0; i < CD_MAX; i++) {
    int base = CD_ADDR + i * CD_ENT;
    for (int j = 0; j < 13; j++) EEPROM.write(base + j, g_cd[i].name[j]);
    for (int j = 0; j < 11; j++) EEPROM.write(base + 13 + j, g_cd[i].date[j]);
  }
  EEPROM.commit();
}
static void cdLoad() {
  for (int i = 0; i < CD_MAX; i++) {
    int base = CD_ADDR + i * CD_ENT;
    for (int j = 0; j < 13; j++) g_cd[i].name[j] = EEPROM.read(base + j);
    for (int j = 0; j < 11; j++) g_cd[i].date[j] = EEPROM.read(base + 13 + j);
    // 未初始化（0xFF）或首字节非 ASCII 的残损槽位视为空，避免旧固件残留被当作有效条目
    if (g_cd[i].name[0] == (char)0xFF) { g_cd[i].name[0] = 0; g_cd[i].date[0] = 0; }
  }
}
static void cdAdd(const char *name, const char *date) {
  int slot = -1;
  for (int i = 0; i < CD_MAX; i++) { if (g_cd[i].name[0] == 0) { slot = i; break; } }
  if (slot < 0) slot = 0;          // 满了覆盖最旧
  strncpy(g_cd[slot].name, name, 12); g_cd[slot].name[12] = 0;
  strncpy(g_cd[slot].date, date, 10); g_cd[slot].date[10] = 0;
  cdSave();
}
static void cdDel(int idx) {
  if (idx < 0 || idx >= CD_MAX) return;
  g_cd[idx].name[0] = 0; g_cd[idx].date[0] = 0;
  cdSave();
}

static void cdDraw() {
  clk.setColorDepth(8);
  clk.loadFont(UIFont_20);
  tft.fillScreen(bgColor);

  // 顶栏
  clk.createSprite(224, 34);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);
  clk.setTextColor(0xFFFF, bgColor);
  clk.drawString("倒数日", 2, 6);
  char today[12];
  snprintf(today, sizeof(today), "%d/%d", month(), day());
  clk.setTextDatum(TR_DATUM);
  clk.setTextColor(0x73AE, bgColor);
  clk.drawString(today, 222, 9);
  clk.pushSprite(8, 8);
  clk.deleteSprite();
  tft.fillRect(8, 46, 224,1, 0x18E4);

  // 收集有效项并按剩余天数升序
  int idx[CD_MAX], n = 0;
  for (int i = 0; i < CD_MAX; i++) if (g_cd[i].name[0]) idx[n++] = i;
  for (int a = 0; a < n; a++)
    for (int b = a + 1; b < n; b++)
      if (cdUntil(g_cd[idx[a]].date) > cdUntil(g_cd[idx[b]].date)) {
        int t = idx[a]; idx[a] = idx[b]; idx[b] = t;
      }
  int nearest = (n > 0) ? idx[0] : -1;

  for (int r = 0; r < n; r++) {
    int i = idx[r];
    int y = 52 + r * 25;
    clk.createSprite(240, 25);
    clk.fillSprite(bgColor);
    if (i == nearest && cdUntil(g_cd[i].date) >= 0)
      clk.fillRoundRect(2, 0, 236, 24, 6, 0x0882);   // 最近一条浅底纹
    clk.setTextDatum(TL_DATUM);
    clk.setTextColor(0xFFFF, bgColor);
    clk.drawString(g_cd[i].name, 10, 2);
    long du = cdUntil(g_cd[i].date);
    char buf[16];
    if (du > 0)      snprintf(buf, sizeof(buf), "%ld 天", du);
    else if (du == 0) snprintf(buf, sizeof(buf), "今天");
    else             snprintf(buf, sizeof(buf), "已过 %ld 天", -du);
    clk.setTextDatum(TR_DATUM);
    clk.setTextColor(du >= 0 ? 0xFFFF : 0x73AE, bgColor);
    clk.drawString(buf, 150, 2);
    clk.setTextDatum(TL_DATUM);
    clk.setTextColor(0x73AE, bgColor);
    clk.drawString(g_cd[i].date, 160, 2);
    clk.pushSprite(0, y);
    clk.deleteSprite();
  }

  if (n == 0) {
    clk.createSprite(200, 40);
    clk.fillSprite(bgColor);
    clk.setTextColor(0x73AE, bgColor);
    clk.drawString("暂无，手机添加", 20, 12);
    clk.pushSprite(20, 110);
    clk.deleteSprite();
  }

  // 底部提示
  clk.createSprite(224, 22);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TC_DATUM);
  clk.setTextColor(0x73AE, bgColor);
  clk.drawString("双击刷新  长按回桌面", 112, 1);
  clk.pushSprite(8, 214);
  clk.deleteSprite();

  clk.unloadFont();
}

static void cdEnter(bool reload) {
  if (reload) cdLoad();
  tft.invertDisplay(1);
  tft.fillScreen(bgColor);
  cdDraw();
}

static void cdLoop(uint8_t ev) {
  if (ev == BEV_DOUBLE) { cdLoad(); cdDraw(); }       // 刷新
  else if (ev == BEV_CLICK)  { switchPage(0); }       // 回桌面（循环收尾）
  else if (ev == BEV_LONG)   { switchPage(0); }       // 回桌面
}

#endif
