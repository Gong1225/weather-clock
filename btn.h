/* *****************************************************************
 * btn.h - 单击 / 双击 / 长按(2s) 手势识别
 *
 * 硬件：轻触开关一端 GPIO4，另一端 GND。INPUT_PULLUP，按下为 LOW。
 * 长按在按住满 2 秒时立即触发（无需松开）。
 * 单击需等待双击窗口（280ms）后才确认，因此单击有约 0.3s 延迟，
 * 属于手势识别的固有代价。
 *
 * 消抖采用"稳定电平过滤"：原始电平变化后需持续稳定 30ms 才被采纳。
 * 这样长按过程中触点瞬间抖动（廉价轻触开关常见）不会被判成"松开"，
 * 长按就能稳定触发（修复"一周天气长按时有时回不了桌面"）。
 * *****************************************************************/
#ifndef BTN_H
#define BTN_H

#include <Arduino.h>

#define BTN_PIN       4
#define BTN_LONG_MS   2000   // 长按阈值
#define BTN_DBL_MS    280    // 双击窗口
#define BTN_DEBOUNCE  30     // 消抖：电平稳定时长
#define BTN_LONG_GUARD 700   // 长按松手后多久内的单击视为误触并吞掉
#define BTN_DEBUG     1      // 1=串口打印按钮动作，排查完可改回 0

enum : uint8_t { BEV_NONE = 0, BEV_CLICK, BEV_DOUBLE, BEV_LONG, BEV_DOWN };

class BtnReader {
public:
  void begin() {
    pinMode(BTN_PIN, INPUT_PULLUP);
    st = S_IDLE;
    rawLevel = true; stableLevel = true; stableSince = 0;
  }

  // 当前是否处于按住状态
  bool held() const { return (st == S_DOWN || st == S_HOLD || st == S_DOWN2); }

  // 每圈 loop 调一次；事件就绪时返回一次 BEV_xxx
  uint8_t poll() {
    unsigned long now = millis();
    // 启动保护：上电后先忽略一切按键，直到首次检测到"松开"才解锁。
    // 否则 GPIO4 上电瞬间的电平毛刺只要凑成"按下→松开"，就会被判成单击，
    // 桌面页单击=跳到一周天气（你看到的"一开机就闪到第二页"）。
    bool raw = (digitalRead(BTN_PIN) == LOW);
    if (!armed) {
      if (!raw) armed = true;                 // 见到一次松开才解锁
      rawLevel = raw; stableLevel = raw; stableSince = now;
      lastLvl = raw;                          // 同步，避免解锁瞬间误报边沿
      return BEV_NONE;
    }
    if (raw != rawLevel) { rawLevel = raw; stableSince = now; }     // 电平变化，重新计时
    if (now - stableSince >= BTN_DEBOUNCE) stableLevel = rawLevel;  // 稳定后才采纳
    bool pressed = stableLevel;

#if BTN_DEBUG
    if (pressed != lastLvl) {
      lastLvl = pressed;
      Serial.printf("[btn] %s (GPIO%d=%d)\r\n",
                    pressed ? "PRESS" : "RELEASE", BTN_PIN, pressed ? 0 : 1);
    }
    if (now - lastReport >= 2000) {
      lastReport = now;
      Serial.printf("[btn] idle GPIO%d=%d st=%d\r\n",
                    BTN_PIN, digitalRead(BTN_PIN) == LOW ? 0 : 1, (int)st);
    }
#endif
    uint8_t evt = poll_inner(pressed, now);
#if BTN_DEBUG
    if (evt != BEV_NONE)
      Serial.printf("[btn] EVT=%s\r\n",
                    evt == BEV_CLICK ? "CLICK" : evt == BEV_DOUBLE ? "DOUBLE" : "LONG");
#endif
    return evt;
  }

private:
  uint8_t poll_inner(bool pressed, unsigned long now) {
    switch (st) {
      case S_IDLE:
        if (pressed) { st = S_DOWN; tDown = now; return BEV_DOWN; }  // 按下即触发
        break;

      case S_DOWN:                      // 第一次按住中
        if (!pressed) {
          if (now - tDown < BTN_DEBOUNCE + 20) { st = S_IDLE; break; } // 抖动
          st = S_UP_WAIT; tUp = now;    // 松开，等看是否双击
        } else if (now - tDown >= BTN_LONG_MS) {
          st = S_HOLD; return BEV_LONG; // 按住即触发
        }
        break;

      case S_HOLD:                      // 长按已触发，等松开
        if (!pressed) { st = S_IDLE; longRelAt = now; }  // 记录松手时刻，吞掉紧随的误触单击
        else if (now - tDown >= BTN_LONG_MS + 5000) {
          st = S_STUCK;                 // 按住超 7 秒：判定为粘连，不再重复触发
          Serial.println(F("[btn] STUCK - button held too long, check wiring"));
        }
        break;

      case S_STUCK:                     // 只等松开，松手即恢复
        if (!pressed) { st = S_IDLE; Serial.println(F("[btn] released, recovered")); }
        break;

      case S_UP_WAIT:                   // 第一次松开后，等双击
        if (pressed) { st = S_DOWN2; tDown2 = now; }
        else if (now - tUp >= BTN_DBL_MS) {
          if (now - longRelAt < BTN_LONG_GUARD) { st = S_IDLE; break; }  // 吞掉长按后的误触单击
          st = S_IDLE; return BEV_CLICK;
        }
        break;

      case S_DOWN2:                     // 第二次按下
        if (!pressed) {
          st = S_IDLE; return BEV_DOUBLE;
        } else if (now - tDown2 >= BTN_LONG_MS) {
          st = S_HOLD; return BEV_LONG; // 二按拖成长按：按长按算
        }
        break;
    }
    return BEV_NONE;
  }

private:
  enum : uint8_t { S_IDLE, S_DOWN, S_HOLD, S_UP_WAIT, S_DOWN2, S_STUCK };
  uint8_t st = S_IDLE;
  bool rawLevel = true, stableLevel = true, lastLvl = true, armed = false;
  unsigned long tDown = 0, tUp = 0, tDown2 = 0, lastReport = 0, stableSince = 0, longRelAt = 0;
};

#endif
