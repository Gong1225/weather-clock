/* *****************************************************************
 * 小型桌面显示器（ESP8266）
 * 引脚： SCK=14  MOSI=13  RES=2  DC=0  LCDBL=5  按键=GPIO4
 * 页面： 0=桌面(天气时钟)  1=一周天气  2=番茄钟  3=倒数日
 * 控制： 手机连 JiQiDao WiFi 后浏览器打开本机 IP（默认 192.168.1.10）遥控
 * *****************************************************************/
#include "ArduinoJson.h"
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h> 
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <EEPROM.h>
#include "qr1b.h"   // 配网二维码（1bpp 压缩版，7.2KB，替代原 qr.h 的 115KB RGB565）
#include "number.h"
#include "weathernum.h"

/* ===== 多页面扩展 + 网页遥控 ===== */
#include <ESP8266WebServer.h>
#include "btn.h"          // 按钮手势（GPIO4）
#include "page_week.h"    // 页面：一周天气
#include "page_pomo.h"    // 页面：番茄钟
#include "page_countdown.h" // 页面：倒数日

/* 网页遥控服务（手机连 JiQiDao WiFi 后访问本机 IP） */
static ESP8266WebServer server(80);

/* 闪屏提醒（无蜂鸣器时的到点提示） */
bool g_flashing = false;
int  g_flashLeft = 0;
unsigned long g_flashNext = 0;
bool g_flashOn = false;
unsigned long g_heapReport = 0;   // 堆内存自检：每 30s 打印一次可用堆，长期观察泄漏是否根治

/* 手机端控制面板 HTML（单引号属性，避免 C 字符串转义） */
const char WEB_HTML[] PROGMEM = "<!DOCTYPE html><html lang='zh'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>时钟遥控</title><style>body{font-family:-apple-system,Segoe UI,Roboto,sans-serif;background:#111;color:#eee;margin:0;padding:16px}h1{font-size:18px;margin:0 0 4px}.sub{color:#888;font-size:12px;margin-bottom:16px}.card{background:#1c1c1c;border-radius:12px;padding:14px;margin-bottom:14px}.card h2{font-size:14px;margin:0 0 10px;color:#7fb069}.row{display:flex;gap:8px;flex-wrap:wrap}.btn{flex:1;min-width:64px;background:#2e7fd0;color:#fff;border:0;border-radius:8px;padding:10px;font-size:14px}.btn.o{background:#e0802a}.btn.g{background:#4a4a4a}.in{width:100%;background:#000;border:1px solid #444;color:#fff;border-radius:8px;padding:8px;margin:6px 0;font-size:14px}.hint{color:#888;font-size:11px;margin-top:8px}input[type=range]{width:100%}.del{color:#e0802a;cursor:pointer}#toast{position:fixed;left:50%;top:14px;transform:translateX(-50%);background:#2e7d32;color:#fff;padding:6px 14px;border-radius:8px;font-size:13px;opacity:0;transition:opacity .25s;pointer-events:none}</style></head><body><h1>桌面时钟遥控</h1><div class='sub'>连家里 JiQiDao WiFi · 浏览器打开 192.168.1.10</div><div class='card'><h2>亮度</h2><input type='range' min='0' max='100' id='b' value='50'><div class='hint'>拖动即时生效</div></div><div class='card'><h2>页面</h2><div class='row'><button class='btn' id='pg0'>桌面</button><button class='btn' id='pg1'>天气</button><button class='btn' id='pg2'>番茄</button><button class='btn' id='pg3'>倒数日</button></div><div class='row' style='margin-top:8px'><button class='btn o' id='w'>刷新天气</button></div></div><div class='card'><h2>番茄钟</h2><div class='row'><button class='btn' id='pf25'>专注25</button><button class='btn' id='pf45'>专注45</button><button class='btn' id='pb5'>休息5</button><button class='btn g' id='ps'>停止</button></div></div><div class='card'><h2>倒数日</h2><input class='in' id='nm' placeholder='名称，如 期中'><input class='in' id='dt' type='date'><button class='btn o' style='width:100%' id='add'>添加</button><div id='cdlist' class='hint'>加载中…</div></div><div id='toast'></div><script>function toast(m){var t=document.getElementById('toast');t.textContent=m;t.style.opacity=1;clearTimeout(t._t);t._t=setTimeout(function(){t.style.opacity=0;},900);}function cmd(u){fetch(u).then(function(){toast('已发送');}).catch(function(){toast('失败');});}function add(){var n=document.getElementById('nm').value,d=document.getElementById('dt').value;if(!n||!d)return;fetch('/api/cdadd?name='+encodeURIComponent(n)+'&date='+d).then(loadCd);}function loadCd(){fetch('/api/cdlist').then(function(r){return r.json();}).then(function(j){var h='';j.forEach(function(c,i){h+=c.name+'  '+c.date+'  '+c.days+'天  <span class=del data-i='+i+'>删</span><br>';});document.getElementById('cdlist').innerHTML=h;});}document.getElementById('b').oninput=function(){fetch('/api/bright?b='+this.value);};document.getElementById('pg0').onclick=function(){cmd('/api/page?p=0');};document.getElementById('pg1').onclick=function(){cmd('/api/page?p=1');};document.getElementById('pg2').onclick=function(){cmd('/api/page?p=2');};document.getElementById('pg3').onclick=function(){cmd('/api/page?p=3');};document.getElementById('w').onclick=function(){cmd('/api/weather');};document.getElementById('pf25').onclick=function(){cmd('/api/pomostart?m=25&b=0');};document.getElementById('pf45').onclick=function(){cmd('/api/pomostart?m=45&b=0');};document.getElementById('pb5').onclick=function(){cmd('/api/pomostart?m=5&b=1');};document.getElementById('ps').onclick=function(){cmd('/api/pomostop');};document.getElementById('add').onclick=add;document.getElementById('cdlist').onclick=function(e){if(e.target.classList.contains('del')){fetch('/api/cddel?i='+e.target.dataset.i).then(loadCd);}};loadCd();</script></body></html>";

/* 页面编号：0=桌面(天气时钟) 1=一周天气 2=番茄钟 3=倒数日 */
uint8_t curPage = 0;

const uint16_t C_BLACK = 0x0000;
const uint16_t C_WHITE = 0xFFFF;
const uint16_t C_GRAY  = 0x630C;
const uint16_t C_GREEN = 0x07E0;

/* 天气 UI 数据缓存（回桌面重绘用，不依赖网络） */
String g_city = "", g_tempS = "", g_humS = "", g_aqiTxt = "", g_wcode = "";
uint16_t g_pm25Color = 0x07E0;
int      g_pm25V = 0;
bool     g_weatherShown = false;
bool     g_needWeather  = false;   // 网页/事件请求刷新天气，由 loop() 在 handler 外执行（避免 WebServer 内嵌 HTTP 崩溃）

/* 按钮与时钟刷新标记（提前声明，供页面切换调用） */
BtnReader btn;
unsigned char Hour_sign   = 60;
unsigned char Minute_sign = 60;
unsigned char Second_sign = 60;

void switchPage(uint8_t p);   // 页面切换（定义在文件末尾）

#define LCD_BL_PIN 5


//---------------修改此处""内的信息（开源版本请改成你自己的 WiFi）--------------------
const char ssid[] = "你的WiFi名称";      // WIFI 名称（仅支持 2.4GHz）
const char pass[] = "你的WiFi密码";      // WIFI 密码
//----------------------------------------------------



#include "font/UIFont_20.h"  // UI 平滑字体（桌面/一周天气/番茄钟/倒数日框架）
#include "img/temperature.h"
#include "img/humidity.h"
#include "img/pangzi/i0.h"
#include "img/pangzi/i1.h"
#include "img/pangzi/i2.h"
#include "img/pangzi/i3.h"
#include "img/pangzi/i4.h"
#include "img/pangzi/i5.h"
#include "img/pangzi/i6.h"
#include "img/pangzi/i7.h"
#include "img/pangzi/i8.h"
#include "img/pangzi/i9.h"

TFT_eSPI tft = TFT_eSPI();  // 引脚请自行配置tft_espi库中的 User_Setup.h文件
TFT_eSprite clk = TFT_eSprite(&tft);

/*** Component objects ***/
Number      dig;
WeatherNum  wrat;


uint32_t targetTime = 0;   
uint16_t bgColor = 0x0000;
String cityCode = "101110101";  //天气城市代码 西安:101110101 长沙:101250101 株洲:101250301 衡阳:101250401
int LCD_BL_PWM = 50;//屏幕亮度0-100（背光低电平点亮，值越大越亮；EEPROM(BL_addr)有有效值时以 EEPROM 为准）
int tempnum = 0;   //温度百分比
int huminum = 0;   //湿度百分比
int tempcol =0xffff;
int humicol =0xffff;
int Anim = 0;
int prevTime = 0;
int AprevTime = 0;
int BL_addr = 1;//被写入数据的EEPROM地址编号  0亮度
int CC_addr = 10;//被写入数据的EEPROM地址编号  10城市

//NTP服务器
static const char ntpServerName[] = "ntp6.aliyun.com";
const int timeZone = 8;     //东八区


WiFiUDP Udp;
WiFiClient wificlient;
unsigned int localPort = 8000;
float duty=0;
time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
String num2str(int digits);
void sendNTPpacket(IPAddress &address);


bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  // Return 1 to decode next block
  return 1;
}

byte loadNum = 6;
void loading(byte delayTime)//绘制进度条
{
  clk.setColorDepth(8);
  
  clk.createSprite(200, 100);//创建窗口
  clk.fillSprite(0x0000);   //填充率

  clk.drawRoundRect(0,0,200,16,8,0xFFFF);       //空心圆角矩形
  clk.fillRoundRect(3,3,loadNum,10,5,0xFFFF);   //实心圆角矩形
  clk.setTextDatum(CC_DATUM);   //设置文本数据
  clk.setTextColor(TFT_GREEN, 0x0000); 
  clk.drawString("Connecting to WiFi......",100,40,2);
  clk.setTextColor(TFT_WHITE, 0x0000); 
  clk.pushSprite(20,110);  //窗口位置
  
  //clk.setTextDatum(CC_DATUM);
  //clk.setTextColor(TFT_WHITE, 0x0000); 
  //clk.pushSprite(130,180);
  
  clk.deleteSprite();
  loadNum += 1;
  delay(delayTime);
}

void humidityWin()
{
  clk.setColorDepth(8);
  
  huminum = huminum/2;
  clk.createSprite(52, 6);  //创建窗口
  clk.fillSprite(0x0000);    //填充率
  clk.drawRoundRect(0,0,52,6,3,0xFFFF);  //空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1,1,huminum,4,2,humicol);   //实心圆角矩形
  clk.pushSprite(45,222);  //窗口位置
  clk.deleteSprite();
}
void tempWin()
{
  clk.setColorDepth(8);
  
  clk.createSprite(52, 6);  //创建窗口
  clk.fillSprite(0x0000);    //填充率
  clk.drawRoundRect(0,0,52,6,3,0xFFFF);  //空心圆角矩形  起始位x,y,长度，宽度，圆弧半径，颜色
  clk.fillRoundRect(1,1,tempnum,4,2,tempcol);   //实心圆角矩形
  clk.pushSprite(45,192);  //窗口位置
  clk.deleteSprite();
}

// 绘制 1bpp 配网二维码（qr1b.h，7.2KB）
static void drawQR()
{
  static uint16_t rowbuf[240];
  for (int r = 0; r < 240; r++) {
    const uint8_t *src = qr1b + r * 30;
    // ESP8266 XIP 只支持 32 位对齐字访问，禁止 l8ui 直接索引 flash
    for (int bi = 0; bi < 30; bi++) {
      uint8_t bits = pgm_read_byte(src + bi);
      for (int b = 0; b < 8; b++)
        rowbuf[bi * 8 + b] = (bits & (0x80 >> b)) ? 0xFFFF : 0x0000;
    }
    tft.pushImage(0, r, 240, 1, rowbuf);
  }
}

void SmartConfig(void)//微信配网
{
  WiFi.mode(WIFI_STA);    //设置STA模式
  drawQR();               //显示配网二维码（1bpp）
  Serial.println("\r\nWait for Smartconfig...");    //打印log信息
  WiFi.beginSmartConfig();      //开始SmartConfig，等待手机端发出用户名和密码
  while (1)
  {
    Serial.print(".");
    delay(100);                   // wait for a second
    if (WiFi.smartConfigDone())//配网成功，接收到SSID和密码
    {
    Serial.println("SmartConfig Success");
    Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
    Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
    break;
    }
  }
  loadNum = 194;
}

String SMOD = "";//0亮度

void Serial_set()//串口设置
{
  String incomingByte = "";
  if(Serial.available()>0)
  {
    
    while(Serial.available()>0)//监测串口缓存，当有数据输入时，循环赋值给incomingByte
    {
      incomingByte += char(Serial.read());//读取单个字符值，转换为字符，并按顺序一个个赋值给incomingByte
      delay(2);//不能省略，因为读取缓冲区数据需要时间
    }    
    if(SMOD=="0x01")//设置1亮度设置
    {
      int LCDBL = atoi(incomingByte.c_str());//int n = atoi(xxx.c_str());//String转int
      if(LCDBL>=0 && LCDBL<=100)
      {
        EEPROM.write(BL_addr, LCDBL);//亮度地址写入亮度值
        EEPROM.commit();//保存更改的数据
        delay(5);
        LCD_BL_PWM = EEPROM.read(BL_addr); 
        delay(5);
        SMOD = "";
        Serial.printf("亮度调整为：");
        analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM*10));
        Serial.println(LCD_BL_PWM);
        Serial.println("");
      }
      else
        Serial.println("亮度调整错误，请输入0-100");
    } 
    if(SMOD=="0x02")//设置2地址设置
    {
      int CityCODE = 0;
      int CityC = atoi(incomingByte.c_str());//int n = atoi(xxx.c_str());//String转int
      if(CityC>=101000000 && CityC<=102000000 || CityC == 0)
      {
        for(int cnum=0;cnum<5;cnum++)
        {
          EEPROM.write(CC_addr+cnum,CityC%100);//城市地址写入城市代码
          EEPROM.commit();//保存更改的数据
          CityC = CityC/100;
          delay(5);
        }
        for(int cnum=5;cnum>0;cnum--)
        {          
          CityCODE = CityCODE*100;
          CityCODE += EEPROM.read(CC_addr+cnum-1); 
          delay(5);
        }
        
        cityCode = CityCODE;
        
        if(cityCode == "0")
        {
          Serial.println("城市代码调整为：自动");
          getCityCode();  //获取城市代码
        }
        else
        {
          Serial.printf("城市代码调整为：");
          Serial.println(cityCode);
        }
        Serial.println("");
        getCityWeater();//更新城市天气  
        SMOD = "";
      }
      else
        Serial.println("城市调整错误，请输入9位城市代码，自动获取请输入0");
    }   
    else
    {
      SMOD = incomingByte;
      delay(2);
      if(SMOD=="0x01")
        Serial.println("请输入亮度值，范围0-100");
      else if(SMOD=="0x02")
        Serial.println("请输入9位城市代码，自动获取请输入0"); 
      else
      {
        Serial.println("");
        Serial.println("请输入需要修改的代码：");
        Serial.println("亮度设置输入    0x01");
        Serial.println("地址设置输入    0x02");
        Serial.println("");
      }
    }


  }
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(1024);
  btn.begin();          // 按键：GPIO4 + GND

  if(EEPROM.read(BL_addr)>0&&EEPROM.read(BL_addr)<100)
    LCD_BL_PWM = EEPROM.read(BL_addr); 
  
  pinMode(LCD_BL_PIN, OUTPUT);
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM*10));
  
  tft.begin(); /* TFT init */
  tft.invertDisplay(1);//反转所有显示颜色：1反转，0正常
  tft.fillScreen(0x0000);
  tft.setTextColor(TFT_BLACK, bgColor);

  targetTime = millis() + 1000; 

  Serial.print("正在连接WIFI ");
  Serial.println(ssid);
  // 固定本机 IP，方便手机遥控器始终访问 192.168.1.10（不依赖 DHCP 分配）
  IPAddress local_IP(192,168,1,10), gateway_IP(192,168,1,1), subnet_IP(255,255,255,0);
  IPAddress dns1(192,168,1,1), dns2(223,5,5,5);   // 网关兼 DNS + 公共 DNS 兜底
  WiFi.config(local_IP, gateway_IP, subnet_IP, dns1, dns2);
  WiFi.begin(ssid, pass);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  while (WiFi.status() != WL_CONNECTED) 
  {
    loading(70);  
      
    if(loadNum>=194)
    {
      SmartConfig();   
      break;
    }
  }
  delay(10); 
  while(loadNum < 194) //让动画走完
  { 
    loading(1);
  }

  Serial.print("本地IP： ");
  Serial.println(WiFi.localIP());
  Serial.println("启动UDP");
  Udp.begin(localPort);
  Serial.println("等待同步...");
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);
  
  int CityCODE = 0;
  for(int cnum=5;cnum>0;cnum--)
  {          
    CityCODE = CityCODE*100;
    CityCODE += EEPROM.read(CC_addr+cnum-1); 
    delay(5);
  }
  if(CityCODE>=101000000 && CityCODE<=102000000) 
    cityCode = CityCODE;  
  else
    getCityCode();  //获取城市代码
   
  tft.fillScreen(TFT_BLACK);//清屏
  
  TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
  TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));  //湿度图标

  getCityWeater();

  // 网页遥控服务 + 倒数日加载
  setupWeb();
  cdLoad();
}
time_t prevDisplay = 0; // 显示时间
unsigned long weaterTime = 0;

/* ================= 多页面调度 ================= */

// 桌面页（天气时钟）：保留原有全部行为
static void clockLoop(uint8_t ev)
{
  if (ev == BEV_CLICK) { switchPage(1); return; }     // 进一周天气页
  if (ev == BEV_DOUBLE) { getCityWeater(); return; }  // 手动刷新天气
  // 长按在桌面无动作

  if (now() != prevDisplay) {
    prevDisplay = now();
    digitalClockDisplay();
    prevTime=0;
  }

  if(millis() - weaterTime > 300000){ //5分钟更新一次天气
    weaterTime = millis();
    getCityWeater();
  }
  scrollBanner();
  imgAnim();
  Serial_set();
}

// 回桌面：重绘整个天气时钟界面
static void redrawDesktop()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_BLACK, bgColor);
  TJpgDec.drawJpg(15,183,temperature, sizeof(temperature));  //温度图标
  TJpgDec.drawJpg(15,213,humidity, sizeof(humidity));        //湿度图标
  if (g_weatherShown) drawWeatherUI();     // 用缓存重绘
  else                g_needWeather = true; // 无缓存：交给 loop() 联网拉，避免 WebServer 内嵌 HTTP 崩溃
  Hour_sign = 60; Minute_sign = 60; Second_sign = 60;   // 强制重画数字
  prevDisplay = 0;
}

void switchPage(uint8_t p)
{
  if (p == curPage) return;
  curPage = p;
  // 反色框架：所有页面固定 invertDisplay(1)，颜色按"显示值取反"写入，对漂移免疫。
  tft.invertDisplay(1);
  if (p == 0)      redrawDesktop();
  else if (p == 1) weekEnter(true);
  else if (p == 2) pomoEnter();
  else if (p == 3) cdEnter(true);
}

/* ================= 网页遥控 ================= */
void startFlash(int times) {
  g_flashing = true; g_flashLeft = times * 2; g_flashNext = 0; g_flashOn = false;
}
void tickFlash() {
  if (!g_flashing) return;
  unsigned long n = millis();
  if (n >= g_flashNext) {
    g_flashOn = !g_flashOn;
    tft.invertDisplay(g_flashOn ? 0 : 1);
    g_flashNext = n + 130;
    g_flashLeft--;
    if (g_flashLeft <= 0) { tft.invertDisplay(1); g_flashing = false; }
  }
}

static void sendJson(const String& s) { server.send(200, "application/json", s); }
static void handleRoot() { server.send(200, "text/html; charset=utf-8", String(FPSTR(WEB_HTML))); }
static void handleBright() {
  int b = server.arg("b").toInt();
  if (b < 0) b = 0; if (b > 100) b = 100;
  LCD_BL_PWM = b;
  analogWrite(LCD_BL_PIN, 1023 - (LCD_BL_PWM * 10));
  EEPROM.write(BL_addr, b); EEPROM.commit();
  sendJson("{\"ok\":1,\"bright\":" + String(b) + "}");
}
static void handlePage() {
  int p = server.arg("p").toInt();
  if (p < 0) p = 0; if (p > 3) p = 3;
  switchPage(p);
  sendJson("{\"ok\":1,\"page\":" + String(p) + "}");
}
static void handleWeather() { g_needWeather = true; sendJson("{\"ok\":1}"); }
static void handleCdAdd() {
  String name = server.arg("name"); String date = server.arg("date");
  if (name.length() && date.length() == 10) { cdAdd(name.c_str(), date.c_str()); cdLoad(); }
  handleCdList();
}
static void handleCdDel() { int i = server.arg("i").toInt(); cdDel(i); cdLoad(); handleCdList(); }
static void handleCdList() {
  String s = "["; bool first = true;
  for (int i = 0; i < CD_MAX; i++) {
    if (g_cd[i].name[0] == 0) continue;
    if (!first) s += ","; first = false;
    s += "{\"name\":\"" + String(g_cd[i].name) + "\",\"date\":\"" + String(g_cd[i].date) + "\",\"days\":" + String(cdUntil(g_cd[i].date)) + "}";
  }
  s += "]";
  sendJson(s);
}
static void handlePomoStart() {
  int m = server.arg("m").toInt(); int b = server.arg("b").toInt();
  pomoStart(m, b == 1);
  if (curPage != 2) switchPage(2); else pomoDraw();
  sendJson("{\"ok\":1,\"mode\":" + String((int)g_pomoMode) + "}");
}
static void handlePomoStop() {
  pomoStop();
  if (curPage == 2) pomoDraw();
  sendJson("{\"ok\":1}");
}
static void handleStatus() {
  String s = "{\"page\":" + String(curPage) + ",\"bright\":" + String(LCD_BL_PWM)
           + ",\"pomo\":{\"mode\":" + String((int)g_pomoMode) + ",\"remain\":" + String(g_pomoRemain)
           + ",\"total\":" + String(g_pomoTotal) + "},\"countdowns\":[";
  bool first = true;
  for (int i = 0; i < CD_MAX; i++) {
    if (g_cd[i].name[0] == 0) continue;
    if (!first) s += ","; first = false;
    s += "{\"name\":\"" + String(g_cd[i].name) + "\",\"date\":\"" + String(g_cd[i].date) + "\",\"days\":" + String(cdUntil(g_cd[i].date)) + "}";
  }
  s += "]}";
  sendJson(s);
}
static void handleShutdown() {
  server.send(200, "text/plain", "bye");
  delay(200);
  ESP.deepSleep(0);   // 深度睡眠 = 关机，直到 RST 键 / 重插 USB 唤醒（明天看机用）
}
static void handleNotFound() { server.send(404, "text/plain", "404"); }
void setupWeb() {
  server.on("/", handleRoot);
  server.on("/api/bright", handleBright);
  server.on("/api/page", handlePage);
  server.on("/api/weather", handleWeather);
  server.on("/api/cdadd", handleCdAdd);
  server.on("/api/cddel", handleCdDel);
  server.on("/api/cdlist", handleCdList);
  server.on("/api/pomostart", handlePomoStart);
  server.on("/api/pomostop", handlePomoStop);
  server.on("/api/status", handleStatus);
  server.on("/api/shutdown", handleShutdown);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop()
{
  uint8_t ev = btn.poll();
  switch (curPage) {
    case 0: clockLoop(ev);  break;
    case 1: weekLoop(ev);   break;
    case 2: pomoLoop(ev);   break;
    case 3: cdLoop(ev);     break;
  }
  // 天气刷新统一在这里执行（绝不在 WebServer handler 内发 HTTP，否则会崩）
  if (g_needWeather) {
    g_needWeather = false;
    getCityWeater();
    if (curPage == 0) redrawDesktop();   // 桌面页：拉完即重绘（此时已有缓存，不会再触发嵌套请求）
  }
  // 一周天气拉取同样在这里执行（HTTPS 不能嵌在 WebServer handler 内）
  if (g_needWeek) {
    g_needWeek = false;
    if (weekFetch()) { if (curPage == 1) weekDraw(); }
  }
  server.handleClient();   // 网页遥控
  tickFlash();             // 闪屏提醒（如有）
  if (millis() - g_heapReport > 30000) {  // 每 30s 上报一次可用堆（开机应 ~20KB+，长期不跌即泄漏已修复）
    g_heapReport = millis();
    Serial.printf("[heap] free=%u page=%d\r\n", ESP.getFreeHeap(), (int)curPage);
  }
}


// 发送HTTP请求并且将服务器响应通过串口输出
void getCityCode(){
 String URL = "http://wgeo.weather.com.cn/ip/?_="+String(now());
  //创建 HTTPClient 对象
  HTTPClient httpClient;
  httpClient.setTimeout(8000);
  //配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(wificlient,URL); 
  
  //设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");
 
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.print("Send GET request to URL: ");
  Serial.println(URL);
  
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    String str = httpClient.getString();
    
    int aa = str.indexOf("id=");
    if(aa>-1)
    {
       //cityCode = str.substring(aa+4,aa+4+9).toInt();
       cityCode = str.substring(aa+4,aa+4+9);
       Serial.println(cityCode); 
       getCityWeater();
    }
    else
    {
      Serial.println("获取城市代码失败");  
    }
    
    
  } else {
    Serial.println("请求城市代码错误：");
    Serial.println(httpCode);
  }
 
  //关闭ESP8266与服务器连接
  httpClient.end();
}



// 获取城市天气
void getCityWeater(){
 //String URL = "http://d1.weather.com.cn/dingzhi/" + cityCode + ".html?_="+String(now());//新
 String URL = "http://d1.weather.com.cn/weather_index/" + cityCode + ".html?_="+String(now());//原来
  //创建 HTTPClient 对象
  HTTPClient httpClient;
  httpClient.setTimeout(8000);          // 数据/连接总超时 8s，避免服务器无响应时卡死整圈 loop
  httpClient.begin(URL); 
  
  //设置请求头中的User-Agent
  httpClient.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 11_0 like Mac OS X) AppleWebKit/604.1.38 (KHTML, like Gecko) Version/11.0 Mobile/15A372 Safari/604.1");
  httpClient.addHeader("Referer", "http://www.weather.com.cn/");
 
  //启动连接并发送HTTP请求
  int httpCode = httpClient.GET();
  Serial.println("正在获取天气数据");
  Serial.println(URL);
  
  //如果服务器响应OK则从服务器获取响应体信息并通过串口输出
  if (httpCode == HTTP_CODE_OK) {

    String str = httpClient.getString();
    int indexStart = str.indexOf("weatherinfo\":");
    int indexEnd = str.indexOf("};var alarmDZ");

    String jsonCityDZ = str.substring(indexStart+13,indexEnd);
    //Serial.println(jsonCityDZ);

    indexStart = str.indexOf("dataSK =");
    indexEnd = str.indexOf(";var dataZS");
    String jsonDataSK = str.substring(indexStart+8,indexEnd);
    //Serial.println(jsonDataSK);

    
    indexStart = str.indexOf("\"f\":[");
    indexEnd = str.indexOf(",{\"fa");
    String jsonFC = str.substring(indexStart+5,indexEnd);
    //Serial.println(jsonFC);
    
    weaterData(&jsonCityDZ,&jsonDataSK,&jsonFC);
    Serial.println("获取成功");
    
  } else {
    Serial.println("请求城市天气错误：");
    Serial.print(httpCode);
  }
 
  //关闭ESP8266与服务器连接
  httpClient.end();
}


String scrollText[7];
//int scrollTextWidth = 0;
//天气信息写到屏幕上（数据来自 g_* 缓存；weaterData() 和 redrawDesktop() 都会调它）
void drawWeatherUI()
{
  clk.setColorDepth(8);
  clk.loadFont(UIFont_20);

  //温度
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(g_tempS + "℃",28,13);
  clk.pushSprite(100,184);
  clk.deleteSprite();
  tempnum = atoi(g_tempS.c_str());
  tempnum = tempnum+10;
  if(tempnum<10)
    tempcol=0x00FF;
  else if(tempnum<28)
    tempcol=0x0AFF;
  else if(tempnum<34)
    tempcol=0x0F0F;
  else if(tempnum<41)
    tempcol=0xFF0F;
  else if(tempnum<49)
    tempcol=0xF00F;
  else
  {
    tempcol=0xF00F;
    tempnum=50;
  }
  tempWin();

  //湿度
  clk.createSprite(58, 24);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(g_humS,28,13);
  clk.pushSprite(100,214);
  clk.deleteSprite();
  huminum = atoi(g_humS.substring(0,2).c_str());

  if(huminum>90)
    humicol=0x00FF;
  else if(huminum>70)
    humicol=0x0AFF;
  else if(huminum>40)
    humicol=0x0F0F;
  else if(huminum>20)
    humicol=0xFF0F;
  else
    humicol=0xF00F;
  humidityWin();


  //城市名称
  clk.createSprite(94, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(g_city,44,16);
  clk.pushSprite(15,15);
  clk.deleteSprite();

  //PM2.5空气指数
  clk.createSprite(56, 24);
  clk.fillSprite(bgColor);
  clk.fillRoundRect(0,0,50,24,4,g_pm25Color);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(0x0000);
  clk.drawString(g_aqiTxt,25,13);
  clk.pushSprite(104,18);
  clk.deleteSprite();

  //天气图标
  wrat.printfweather(170,15,atoi(g_wcode.substring(1,3).c_str()));

  clk.unloadFont();
  g_weatherShown = true;
}

//天气数据解析（网络）→ 写入缓存 → 绘制
void weaterData(String *cityDZ,String *dataSK,String *dataFC)
{
  //解析第一段JSON
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, *dataSK);
  JsonObject sk = doc.as<JsonObject>();

  g_tempS = sk["temp"].as<String>();
  g_humS  = sk["SD"].as<String>();
  // 接口按 IP 定位返回区级地名（实测返回"雁塔"），"雁"不在点阵字库里会显示成方框
  // （2026-10-16 用户看到左上角"□塔"即此原因）。天气数据本来就是按城市代码
  // 101110113（西安）拉的，左上角统一显示"西安"。
  g_city  = "西安";
  g_wcode = sk["weathercode"].as<String>();
  g_pm25V = sk["aqi"] | 0;

  if(g_pm25V>200){
    g_pm25Color = tft.color565(136,11,32);//重度
    g_aqiTxt = "重度";
  }else if(g_pm25V>150){
    g_pm25Color = tft.color565(186,55,121);//中度
    g_aqiTxt = "中度";
  }else if(g_pm25V>100){
    g_pm25Color = tft.color565(242,159,57);//轻
    g_aqiTxt = "轻度";
  }else if(g_pm25V>50){
    g_pm25Color = tft.color565(247,219,100);//良
    g_aqiTxt = "良";
  }else{
    g_pm25Color = tft.color565(156,202,127);//优
    g_aqiTxt = "优";
  }

  scrollText[0] = "实时天气 "+sk["weather"].as<String>();
  scrollText[1] = "空气质量 "+g_aqiTxt;
  scrollText[2] = "风向 "+sk["WD"].as<String>()+sk["WS"].as<String>();

  //scrollText[6] = atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()) ;

  //天气图标
  wrat.printfweather(170,15,atoi((sk["weathercode"].as<String>()).substring(1,3).c_str()));


  //左上角滚动字幕
  //解析第二段JSON
  deserializeJson(doc, *cityDZ);
  JsonObject dz = doc.as<JsonObject>();
  //Serial.println(sk["ws"].as<String>());
  //横向滚动方式
  //String aa = "今日天气:" + dz["weather"].as<String>() + "，温度:最低" + dz["tempn"].as<String>() + "，最高" + dz["temp"].as<String>() + " 空气质量:" + aqiTxt + "，风向:" + dz["wd"].as<String>() + dz["ws"].as<String>();
  //scrollTextWidth = clk.textWidth(scrollText);
  //Serial.println(aa);
  scrollText[3] = "今日"+dz["weather"].as<String>();

  deserializeJson(doc, *dataFC);
  JsonObject fc = doc.as<JsonObject>();

  scrollText[4] = "最低温度"+fc["fd"].as<String>()+"℃";
  scrollText[5] = "最高温度"+fc["fc"].as<String>()+"℃";

  //Serial.println(scrollText[0]);

  drawWeatherUI();
}

int currentIndex = 0;
TFT_eSprite clkb = TFT_eSprite(&tft);

void scrollBanner(){
  //if(millis() - prevTime > 2333) //3秒切换一次
  if(second()%2 ==0&& prevTime == 0)
  { 
    if(scrollText[currentIndex])
    {
      clkb.setColorDepth(8);
      clkb.loadFont(UIFont_20);
      clkb.createSprite(150, 30); 
      clkb.fillSprite(bgColor);
      clkb.setTextWrap(false);
      clkb.setTextDatum(CC_DATUM);
      clkb.setTextColor(TFT_WHITE, bgColor); 
      clkb.drawString(scrollText[currentIndex],74, 16);
      clkb.pushSprite(10,45);
       
      clkb.deleteSprite();
      clkb.unloadFont();
      
      if(currentIndex>=5)
        currentIndex = 0;  //回第一个
      else
        currentIndex += 1;  //准备切换到下一个        
    }
    prevTime = 1;
  }
}
/*
void imgAnim()
{
  int x=160,y=160,dt=43;

  TJpgDec.drawJpg(x,y,i0, sizeof(i0));
  delay(dt);
  TJpgDec.drawJpg(x,y,i1, sizeof(i1));
  delay(dt);
  TJpgDec.drawJpg(x,y,i2, sizeof(i2));
  delay(dt);
  TJpgDec.drawJpg(x,y,i3, sizeof(i3));
  delay(dt);  
  TJpgDec.drawJpg(x,y,i4, sizeof(i4));
  delay(dt);  
  TJpgDec.drawJpg(x,y,i5, sizeof(i5));
  delay(dt);  
  TJpgDec.drawJpg(x,y,i6, sizeof(i6));
  delay(dt);  
  TJpgDec.drawJpg(x,y,i7, sizeof(i7));
  delay(dt);  
  TJpgDec.drawJpg(x,y,i8, sizeof(i8));
  delay(dt);  
  TJpgDec.drawJpg(x,y,i9, sizeof(i9));
  delay(dt);  
}
*/


void imgAnim()
{
  int x=160,y=160;
  if(millis() - AprevTime > 37) //x ms切换一次
  {
    Anim++;
    AprevTime = millis();
  }
  if(Anim==10)
    Anim=0;

  switch(Anim)
  {
    case 0:
      TJpgDec.drawJpg(x,y,i0, sizeof(i0));
      break;
    case 1:
      TJpgDec.drawJpg(x,y,i1, sizeof(i1));
      break;
    case 2:
      TJpgDec.drawJpg(x,y,i2, sizeof(i2));
      break;
    case 3:
      TJpgDec.drawJpg(x,y,i3, sizeof(i3));
      break;
    case 4:
      TJpgDec.drawJpg(x,y,i4, sizeof(i4));
      break;
    case 5:
      TJpgDec.drawJpg(x,y,i5, sizeof(i5));
      break;
    case 6:
      TJpgDec.drawJpg(x,y,i6, sizeof(i6));
      break;
    case 7:
      TJpgDec.drawJpg(x,y,i7, sizeof(i7));
      break;
    case 8: 
      TJpgDec.drawJpg(x,y,i8, sizeof(i8));
      break;
    case 9: 
      TJpgDec.drawJpg(x,y,i9, sizeof(i9));
      break;
    default:
      Serial.println("显示Anim错误");
      break;
  }
}

void digitalClockDisplay()
{ 
  int timey=82;
  if(hour()!=Hour_sign)//时钟刷新
  {
    dig.printfW3660(20,timey,hour()/10);
    dig.printfW3660(60,timey,hour()%10);
    Hour_sign = hour();
  }
  if(minute()!=Minute_sign)//分钟刷新
  {
    dig.printfO3660(101,timey,minute()/10);
    dig.printfO3660(141,timey,minute()%10);
    Minute_sign = minute();
  }
  if(second()!=Second_sign)//分钟刷新
  {
    dig.printfW1830(182,timey+30,second()/10);
    dig.printfW1830(202,timey+30,second()%10);
    Second_sign = second();
  }
  
  /***日期****/
  clk.setColorDepth(8);
  clk.loadFont(UIFont_20);
  
  //星期
  clk.createSprite(58, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);
  clk.drawString(week(),29,16);
  clk.pushSprite(102,150);
  clk.deleteSprite();
  
  //月日
  clk.createSprite(95, 30);
  clk.fillSprite(bgColor);
  clk.setTextDatum(CC_DATUM);
  clk.setTextColor(TFT_WHITE, bgColor);  
  clk.drawString(monthDay(),49,16);
  clk.pushSprite(5,150);
  clk.deleteSprite();
  
  clk.unloadFont();
  /***日期****/
}

//星期
String week()
{
  String wk[7] = {"日","一","二","三","四","五","六"};
  String s = "周" + wk[weekday()-1];
  return s;
}

//月日
String monthDay()
{
  String s = String(month());
  s = s + "月" + day() + "日";
  return s;
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP时间在消息的前48字节中
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  // 双服务器重试：WiFi 刚连上时 DNS 常未就绪，hostByName 失败会让 IP 停在
  // 0.0.0.0，包发到空地址必超时（2026-10-16 抓到的 "No NTP Response" 即此）。
  // 主服务器（阿里）解析失败/无响应时换备选（腾讯），并把解析结果打到串口。
  const char *ntpBackup = "ntp.tencent.com";
  for (int attempt = 0; attempt < 2; attempt++) {
    const char *host = (attempt == 0) ? ntpServerName : ntpBackup;
    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    int dns = WiFi.hostByName(host, ntpServerIP);
    Serial.printf("NTP %s -> %s (dns=%d)\r\n", host, ntpServerIP.toString().c_str(), dns);
    if (dns != 1 || ntpServerIP == IPAddress(0, 0, 0, 0)) { delay(200); continue; }
    // 每服务器发 2 个包（间隔 400ms）：NAT 后单包 UDP 偶发被路由器丢掉
    // （2026-10-16 抓到启动同步成功、5 分钟后重试全失败的丢包特征）
    for (int pkt = 0; pkt < 2; pkt++) {
      sendNTPpacket(ntpServerIP);
      uint32_t beginWait = millis();
      while (millis() - beginWait < 2500) {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE) {
          Serial.println("Receive NTP Response");
          Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
          unsigned long secsSince1900;
          // convert four bytes starting at location 40 to a long integer
          secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
          secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
          secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
          secsSince1900 |= (unsigned long)packetBuffer[43];
          return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }
      }
      if (pkt == 0) delay(400);
    }
    Serial.printf("NTP %s no response\r\n", host);
  }
  Serial.println("No NTP Response :-(");
  return 0; // 无法获取时间时返回0
}

// 向NTP服务器发送请求
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
