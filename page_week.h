/* *****************************************************************
 * page_week.h - 一周天气预报（数据源：Open-Meteo，免 key）
 *
 * 接口：GET https://api.open-meteo.com/v1/forecast
 *       ?latitude=34.3416&longitude=108.9398   ← 西安（西沣/雁塔），换城市改这里
 *       &daily=weather_code,temperature_2m_max,temperature_2m_min
 *       &timezone=Asia/Shanghai&forecast_days=7
 * 一行一天：日期（今天/明天/周日…） 最高温 最低温 天气
 *
 * 【渲染栈】与首页桌面同源：TFT_eSprite clk + UIFont_20 平滑字体，
 * 白底黑字 + 温度彩色数字分级 + 今天行浅底纹。
 *
 * 【配色框架】仍在 invertDisplay(1) 框架下，颜色按"显示值取反"写入：
 *   bgColor=0x0000 显示白   TFT_WHITE=0xFFFF 显示黑
 *   高温 #F5A83D/#E8862D/#F2C14E、低温 #5EB4DC/#2E9BD6/#1E6FD9
 *
 * 交互：单击=去番茄钟页  双击=刷新  长按=回桌面
 * *****************************************************************/
#ifndef PAGE_WEEK_H
#define PAGE_WEEK_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern TFT_eSprite clk;            // 与桌面时钟共用（.ino 定义）
extern uint16_t bgColor;           // 桌面背景写入值 0x0000（invert(1) 下显示白）
extern const uint8_t UIFont_20[];
extern void switchPage(uint8_t p);

bool g_needWeek = false;   // 进入一周天气页时置位，由 loop() 在 WebServer handler 外拉取（避免内嵌 HTTPS 崩溃）

#define WEEK_URL "https://api.open-meteo.com/v1/forecast?latitude=34.3416&longitude=108.9398&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=Asia/Shanghai&forecast_days=7"
#define WEEK_DAYS 7

/* 显示色 → 面板写入值（16 位取反，invert(1) 框架） */
#define WC_TEXT   0xFFFF   // 标题/今天明天：显示黑
#define WC_DARK   0xBDD7   // 日期/天气：显示 #444
#define WC_MUTE   0x73AE   // 辅助灰：显示 #888
#define WC_LINE   0x18E4   // 分隔线：显示 #E5E2DA
#define WC_TODAY  0x0882   // 今天行底纹：显示 #F2EFE8

/* 温度分档：高温暖色、低温冷色（首页温度彩色数字同款气质，白底可读） */
static uint16_t wcHot(int t) {
  if (t >= 30) return 0x13DA;      // 橙 #E8862D
  if (t >= 22) return 0x0AB8;      // 暖黄 #F5A83D
  return 0x09F6;                   // 浅金 #F2C14E
}
static uint16_t wcCold(int t) {
  if (t < 8)  return 0xE484;       // 蓝 #1E6FD9
  if (t < 14) return 0xD325;       // 青 #2E9BD6
  return 0xA244;                   // 浅青 #5EB4DC
}

static bool  g_weekValid = false;
static char  g_wxDate[WEEK_DAYS][8];    // "今天"/"明天"/"周日".."周六"
static char  g_wxRange[24];             // 头部日期范围 "10/16 - 10/22"
static int8_t g_wxTmax[WEEK_DAYS];
static int8_t g_wxTmin[WEEK_DAYS];
static char  g_wxText[WEEK_DAYS][8];    // 天气文字（≤2 汉字）

/* WMO weather_code → 中文（≤2 字，映射表来自 Open-Meteo 文档） */
static const char *wmoText(int c) {
  switch (c) {
    case 0:  return "晴";
    case 1:  return "晴";
    case 2:  return "少云";
    case 3:  return "阴";
    case 45: case 48: return "雾";
    case 51: case 53: case 55: return "小雨";
    case 56: case 57: return "冻雾";
    case 61: return "小雨";
    case 63: return "中雨";
    case 65: return "大雨";
    case 66: case 67: return "阵雨";
    case 71: return "小雪";
    case 73: return "中雪";
    case 75: return "大雪";
    case 77: return "雪粒";
    case 80: case 81: return "阵雨";
    case 82: return "暴雨";
    case 85: case 86: return "阵雪";
    case 95: return "雷雨";
    case 96: case 99: return "冰雹";
    default: return "多云";
  }
}

/* ---------- 拉取 ---------- */
static bool weekFetch() {
  WiFiClientSecure cli;
  cli.setInsecure();
  cli.setBufferSizes(4096, 1024);      // TLS 缓冲：响应仅 ~1.5KB，原 5120 过大且无谓占堆
  HTTPClient http;
  bool ok = false;
  if (http.begin(cli, WEEK_URL)) {
    http.setTimeout(12000);
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
      String body = http.getString();      // ~1.5KB
      http.end();
      cli.stop();                          // 关键：显式关闭 TLS，回收 ~6KB 缓冲。
                                           // 不调 stop()，ESP8266 上每次进天气页都会泄漏堆，
                                           // 几次之后桌面页每秒 createSprite 就分配失败→定格卡死。
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, body);
      if (err) { Serial.printf("[week] json %s\r\n", err.c_str()); }
      else {
        JsonArray tArr = doc["daily"]["time"].as<JsonArray>();
        JsonArray cArr = doc["daily"]["weather_code"].as<JsonArray>();
        JsonArray xArr = doc["daily"]["temperature_2m_max"].as<JsonArray>();
        JsonArray nArr = doc["daily"]["temperature_2m_min"].as<JsonArray>();
        if (!tArr.isNull() && !cArr.isNull()) {
          // 今天星期几：0=周日..6=周六（epoch 天 +4 对 7 取模，与 time.h 的 weekday 对齐）
          time_t nowT = now();
          int dow = (int)(((int64_t)nowT / 86400 + 4) % 7);
          static const char *WD[7] = { "周日", "周一", "周二", "周三", "周四", "周五", "周六" };
          const char *d0 = tArr[0].as<const char*>();
          const char *d1 = tArr[WEEK_DAYS - 1].as<const char*>();
          if (d0 && strlen(d0) >= 10 && d1 && strlen(d1) >= 10)
            snprintf(g_wxRange, sizeof(g_wxRange), "%c%c/%c%c - %c%c/%c%c",
                     d0[5], d0[6], d0[8], d0[9], d1[5], d1[6], d1[8], d1[9]);
          int i = 0;
          for (JsonVariant v : tArr) {
            if (i >= WEEK_DAYS) break;
            if (i == 0)      strncpy(g_wxDate[i], "今天", 7);
            else if (i == 1) strncpy(g_wxDate[i], "明天", 7);
            else             strncpy(g_wxDate[i], WD[(dow + i) % 7], 7);
            g_wxText[i][0] = 0;
            strncpy(g_wxText[i], wmoText((int)cArr[i].as<float>()), 7);
            g_wxTmax[i] = (int8_t)lroundf(xArr[i].as<float>());
            g_wxTmin[i] = (int8_t)lroundf(nArr[i].as<float>());
            i++;
          }
          Serial.printf("[week] got %d days\r\n", i);
          g_weekValid = (i > 0);
          ok = g_weekValid;
        } else {
          Serial.println("[week] no daily");
        }
      }
    } else {
      Serial.printf("[week] HTTP %d\r\n", code);
    }
  } else {
    Serial.println("[week] begin fail");
  }
  http.end();
  cli.stop();        // 所有路径都兜底关闭，杜绝泄漏
  return ok;
}

/* ---------- 渲染：复刻首页桌面（白底黑字 + 平滑字体 + 温度分色） ---------- */
static void weekDraw() {
  clk.setColorDepth(8);
  clk.loadFont(UIFont_20);
  tft.fillScreen(bgColor);                       // 显示白底

  // 头部：左"一周天气"黑粗体，右日期范围灰
  clk.createSprite(224, 34);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);
  clk.setTextColor(WC_TEXT, bgColor);
  clk.drawString("一周天气", 2, 6);
  clk.setTextDatum(TR_DATUM);
  clk.setTextColor(WC_MUTE, bgColor);
  clk.drawString(g_wxRange, 222, 9);
  clk.pushSprite(8, 8);
  clk.deleteSprite();
  tft.fillRect(8, 46, 224, 1, WC_LINE);          // 头部分隔线

  if (!g_weekValid) {
    clk.createSprite(200, 96);
    clk.fillSprite(bgColor);
    clk.setTextDatum(TL_DATUM);
    clk.setTextColor(WC_TEXT, bgColor);
    clk.drawString("获取失败", 8, 10);
    clk.setTextColor(WC_DARK, bgColor);
    clk.drawString("双击 重试", 8, 44);
    clk.drawString("长按 回桌面", 8, 70);
    clk.pushSprite(20, 80);
    clk.deleteSprite();
    clk.unloadFont();
    return;
  }

  // 7 行数据：一行一天（行高 25，今天行浅底纹突出）
  for (int i = 0; i < WEEK_DAYS; i++) {
    int ry = 52 + i * 25;
    clk.createSprite(240, 25);
    clk.fillSprite(bgColor);
    if (i == 0) clk.fillRoundRect(2, 0, 236, 24, 6, WC_TODAY);

    clk.setTextDatum(TL_DATUM);
    clk.setTextColor(i < 2 ? WC_TEXT : WC_DARK, bgColor);   // 今天/明天黑粗，其余深灰
    clk.drawString(g_wxDate[i], 10, 2);

    char t[10];
    snprintf(t, sizeof(t), "%d℃", g_wxTmax[i]);
    clk.setTextDatum(TR_DATUM);
    clk.setTextColor(wcHot(g_wxTmax[i]), bgColor);
    clk.drawString(t, 132, 2);

    snprintf(t, sizeof(t), "%d℃", g_wxTmin[i]);
    clk.setTextColor(wcCold(g_wxTmin[i]), bgColor);
    clk.drawString(t, 184, 2);

    clk.setTextDatum(TL_DATUM);
    clk.setTextColor(WC_DARK, bgColor);
    clk.drawString(g_wxText[i], 192, 2);

    clk.pushSprite(0, ry);
    clk.deleteSprite();
  }

  // 底部提示（文案必须短：224px sprite 内最多约 9 个汉字 + 2 空格，
  // 旧文案"双击 刷新    长按 回桌面"约 250px 两侧被裁，2026-10-16 用户反馈字体有问题即此）
  clk.createSprite(224, 22);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TC_DATUM);
  clk.setTextColor(WC_MUTE, bgColor);
  clk.drawString("双击刷新  长按回桌面", 112, 1);
  clk.pushSprite(8, 214);
  clk.deleteSprite();

  clk.unloadFont();
}

static void weekEnter(bool fetch) {
  tft.invertDisplay(1);          // 与桌面同框架
  tft.fillScreen(bgColor);

  clk.setColorDepth(8);
  clk.loadFont(UIFont_20);
  clk.createSprite(224, 34);
  clk.fillSprite(bgColor);
  clk.setTextDatum(TL_DATUM);
  clk.setTextColor(WC_TEXT, bgColor);
  clk.drawString("一周天气", 2, 6);
  clk.setTextDatum(TR_DATUM);
  clk.setTextColor(WC_MUTE, bgColor);
  clk.drawString(g_wxRange, 222, 9);
  clk.pushSprite(8, 8);
  clk.deleteSprite();
  tft.fillRect(8, 46, 224, 1, WC_LINE);

  clk.createSprite(200, 30);
  clk.fillSprite(bgColor);
  clk.setTextColor(WC_DARK, bgColor);
  clk.drawString("正在获取...", 4, 5);
  clk.pushSprite(20, 100);
  clk.deleteSprite();
  clk.unloadFont();

  if (fetch) g_needWeek = true;   // 改为由 loop() 拉取，避免 WebServer handler 内嵌 HTTPS 崩溃
  weekDraw();
}

static void weekLoop(uint8_t ev) {
  if (ev == BEV_DOUBLE)      weekEnter(true);       // 刷新
  else if (ev == BEV_CLICK)  switchPage(2);          // 去番茄钟页
  else if (ev == BEV_LONG)   switchPage(0);          // 回桌面
}

#endif
