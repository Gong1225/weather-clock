# 手机遥控 Web API

设备通电连网后，在局域网内以 `ESP8266WebServer` 监听 **80 端口**，固定 IP `192.168.1.10`。手机或任意同一 WiFi 下的程序均可调用以下接口。

> 控制面板页面（`GET /`）已内置上述全部功能，一般用户直接用浏览器打开 `http://192.168.1.10` 即可，无需手写请求。

## 页面与状态

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/` | 返回手机控制面板 HTML |
| GET | `/api/status` | 返回 JSON：`{page, ip, flash, countdowns:[{name,date,days}]}` |

## 控制接口

| 方法 | 路径 | 参数 | 说明 |
| --- | --- | --- | --- |
| GET | `/api/bright` | `b=0..100` | 设置屏幕亮度，立即生效并写入 EEPROM |
| GET | `/api/page` | `p=0\|1\|2\|3` | 切换页面（0 桌面 / 1 一周天气 / 2 番茄钟 / 3 倒数日） |
| GET | `/api/weather` | — | 触发刷新桌面天气（实际拉取在 `loop()` 执行，避免阻塞 handler） |
| GET | `/api/pomostart` | `m=分钟` `b=0\|1` | 启动番茄钟；`b=1` 为休息模式。例：`/api/pomostart?m=25&b=0` |
| GET | `/api/pomostop` | — | 停止番茄钟 |
| GET | `/api/cdadd` | `name=名称` `date=YYYY-MM-DD` | 新增倒数日事项（最多 4 条） |
| GET | `/api/cddel` | `i=序号` | 删除倒数日事项（序号来自 `/api/cdlist` 顺序，0 起） |
| GET | `/api/cdlist` | — | 返回 JSON 数组：`[{name,date,days}]` |
| GET | `/api/shutdown` | — | 进入深度睡眠（等同关机），按 RST 或重插 USB 唤醒 |

## 示例

```bash
# 切到一周天气页
curl "http://192.168.1.10/api/page?p=1"

# 启动 25 分钟专注番茄钟
curl "http://192.168.1.10/api/pomostart?m=25&b=0"

# 添加一个倒数日
curl "http://192.168.1.10/api/cdadd?name=%E6%9C%9F%E4%B8%AD&date=2026-11-01"

# 查看状态
curl "http://192.168.1.10/api/status"
```

## 实现注意（开发者）

- 所有会发起外网请求的刷新（天气 / 一周天气）**不在 WebServer 的 handler 内直接执行**，而是置 `g_needWeather` / `g_needWeek` 标志，由 `loop()` 在 handler 返回后再发请求。原因：ESP8266 在 `handleClient` 回调里再开 `WiFiClient` 会破坏 WiFi 协议栈导致重启。
- 一周天气的 `WiFiClientSecure` 走 HTTPS，每次用完必须 `stop()`，否则每次进页泄漏约 6KB 堆内存。
- 固定 IP 通过 `WiFi.config(local_IP, gateway, subnet, dns1, dns2)` 设置，务必带上 DNS，否则域名解析失败。
