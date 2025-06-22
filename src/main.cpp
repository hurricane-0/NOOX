#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <FastLED.h>
#include <Wire.h> // For I2C
#include <SD.h>   // For SD Card
#include <WiFi.h> // For WiFi connectivity
#include <HTTPClient.h> // For HTTP/HTTPS requests
#include <ArduinoJson.h> // For JSON parsing (will be used for Gemini API)
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <USB.h> // For USB functions
#include "USBHIDKeyboard.h" // For USB Keyboard HID (native ESP32)

// LCD 引脚定义 (根据 hardware.md)
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4 // 或者直接连接到 ESP32 的 RST 引脚
#define TFT_BL    45 // 背光控制

// ST7735s 屏幕尺寸
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// 初始化 Adafruit ST7735 库
Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);

// 声明 USBHIDKeyboard 对象
USBHIDKeyboard Keyboard;

// 按键引脚定义 (根据 hardware.md)
#define KEY1_PIN  47
#define KEY2_PIN  41
#define KEY3_PIN  33

// LED 引脚定义 (根据 hardware.md)
#define LED1_PIN  46
#define LED2_PIN  42

// WS2812 RGB LED 定义 (根据 hardware.md)
#define WS2812_PIN  48
#define NUM_LEDS    1 // 只有一个 WS2812 LED
CRGB leds[NUM_LEDS];

// SD 卡引脚定义 (根据 hardware.md)
#define SD_SCK    36
#define SD_MOSI   35
#define SD_MISO   37
#define SD_CS     34

// I2C 引脚定义 (根据 hardware.md)
#define I2C_SDA_PIN 7
#define I2C_SCL_PIN 17

// UART 引脚定义 (根据 hardware.md)
// UART1
#define UART1_TX_PIN 43
#define UART1_RX_PIN 44
// UART2
#define UART2_TX_PIN 39
#define UART2_RX_PIN 40


void setup() {
  // 初始化串口用于调试
  Serial.begin(115200);
  Serial.println("ESP32-S3 AI Hardware booting...");

  // 初始化 LCD 背光引脚
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // 开启背光

  // 初始化 SPI 总线
  // SCK=GPIO18, MOSI=GPIO23, MISO=GPIO17 (MISO在这里不使用，但需要占位)
  SPI.begin(18, 17, 23, -1); 

  // 初始化 LCD
  tft.initR(INITR_BLACKTAB); // 初始化 ST7735S 屏幕，使用黑色标签初始化序列
  tft.setRotation(1); // 设置屏幕方向，根据实际情况调整

  tft.fillScreen(ST77XX_BLACK); // 填充黑色背景
  tft.setTextWrap(false); // 不自动换行
  tft.setTextSize(2); // 设置字体大小
  tft.setTextColor(ST77XX_WHITE); // 设置字体颜色
  tft.setCursor(0, 0); // 设置光标位置
  tft.println("Hello, ESP32-S3!");
  tft.println("AI Hardware");

  // 初始化按键引脚为下拉输入
  pinMode(KEY1_PIN, INPUT_PULLDOWN);
  pinMode(KEY2_PIN, INPUT_PULLDOWN);
  pinMode(KEY3_PIN, INPUT_PULLDOWN);

  // 初始化 LED 引脚为输出
  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  // 初始化 WS2812
  FastLED.addLeds<WS2812B, WS2812_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64); // 设置亮度

  // 初始化 SD 卡
  if (!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    tft.setCursor(0, 100);
    tft.setTextColor(ST77XX_RED);
    tft.println("SD Card Failed!");
    // return; // 不返回，继续初始化其他硬件
  } else {
    Serial.println("SD Card Mounted.");
    tft.setCursor(0, 100);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("SD Card OK!");

    // 列出 SD 卡根目录文件 (示例)
    File root = SD.open("/");
    if (root) {
      Serial.println("Files on SD Card:");
      while (File entry = root.openNextFile()) {
        if (entry.isDirectory()) {
          Serial.print("  DIR : ");
          Serial.println(entry.name());
        } else {
          Serial.print("  FILE: ");
          Serial.print(entry.name());
          Serial.print("\tSIZE: ");
          Serial.println(entry.size());
        }
        entry.close();
      }
      root.close();
    } else {
      Serial.println("Failed to open root directory");
    }
  }

  // 初始化 I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("I2C Initialized.");
  tft.setCursor(0, 120);
  tft.setTextColor(ST77XX_BLUE);
  tft.println("I2C Init OK!");

  // 扫描 I2C 设备 (示例)
  byte error, address;
  int nDevices;
  Serial.println("Scanning I2C devices...");
  nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found.");
  } else {
    Serial.println("I2C scan complete.");
  }

  // 初始化 UART1
  Serial1.begin(115200, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  Serial.println("UART1 Initialized.");
  tft.setCursor(0, 140);
  tft.setTextColor(ST77XX_ORANGE);
  tft.println("UART1 Init OK!");

  // 初始化 UART2
  Serial2.begin(115200, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);
  Serial.println("UART2 Initialized.");

  // 所有硬件初始化完成
  Serial.println("All hardware initialized.");

  // 初始化 USB HID (键盘)
  USB.begin(); // 确保 USB 堆栈启动
  Keyboard.begin(); // 启动键盘模拟

  // WiFi 连接 (示例)
  const char* ssid = "Your_SSID";     // 替换为你的 WiFi SSID
  const char* password = "Your_PASSWORD"; // 替换为你的 WiFi 密码

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  tft.setCursor(0, 150);
  tft.setTextColor(ST77XX_WHITE);
  tft.println("Connecting WiFi...");

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 尝试连接20次
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    tft.setCursor(0, 150);
    tft.setTextColor(ST77XX_GREEN);
    tft.println("WiFi Connected!");
    tft.setCursor(0, 160); // 屏幕高度不够，这里只是为了演示
    tft.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection Failed!");
    tft.setCursor(0, 150);
    tft.setTextColor(ST77XX_RED);
    tft.println("WiFi Failed!");
  }
}

// Gemini API 配置
const char* GEMINI_API_KEY = "YOUR_GEMINI_API_KEY"; // 替换为你的 Gemini API Key
const char* GEMINI_API_URL = "https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=";

// 发送文本到 Gemini 并获取响应的函数骨架
String sendToGemini(String prompt) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected for Gemini API.");
    return "";
  }

  HTTPClient http;
  String url = String(GEMINI_API_URL) + GEMINI_API_KEY;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  String requestBody = "{\"contents\": [{\"parts\": [{\"text\": \"" + prompt + "\"}]}]}";
  Serial.print("Sending to Gemini: ");
  Serial.println(requestBody);

  int httpResponseCode = http.POST(requestBody);

  String response = "";
  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpResponseCode);
    response = http.getString();
    Serial.println(response);

    // 简单的 JSON 解析示例 (实际需要更健壮的解析)
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return "Error parsing Gemini response.";
    }

    // 假设响应结构为 { "candidates": [ { "content": { "parts": [ { "text": "..." } ] } } ] }
    if (doc.containsKey("candidates") && doc["candidates"][0].containsKey("content") && doc["candidates"][0]["content"].containsKey("parts")) {
      return doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    } else {
      return "No text content found in Gemini response.";
    }

  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    response = "HTTP Error: " + String(httpResponseCode);
  }

  http.end();
  return response;
}

// Web 服务器和 WebSocket 服务器实例
AsyncWebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); // WebSocket 端口

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
      // 发送欢迎消息
      webSocket.sendTXT(num, "Hello from ESP32-S3!");
    }
      break;
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
      // 在这里处理接收到的文本指令
      // 例如：如果收到 "chat: Hello", 则提取 "Hello" 并发送给 Gemini
      if (String((char*)payload).startsWith("chat:")) {
        String prompt = String((char*)payload).substring(5);
        Serial.print("Chat command received: ");
        Serial.println(prompt);
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_WHITE);
        tft.println("User:");
        tft.println(prompt);
        tft.println("Thinking...");

        String geminiResponse = sendToGemini(prompt);
        Serial.print("Gemini Response: ");
        Serial.println(geminiResponse);

        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(0, 0);
        tft.setTextColor(ST77XX_WHITE);
        tft.println("Gemini Says:");
        tft.println(geminiResponse);

        webSocket.sendTXT(num, "Gemini:" + geminiResponse); // 将 Gemini 响应发送回手机网页
      }
      break;
    case WStype_BIN:
      Serial.printf("[%u] get Binary length: %u\n", num, length);
      break;
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}


void loop() {
  // 主循环，在这里处理各种任务
  // 例如：聊天模式逻辑, 高级模式逻辑, UI更新, 通信处理等

  webSocket.loop(); // 处理 WebSocket 客户端事件

  // 示例：每隔一段时间向 Gemini 发送请求 (仅为演示，实际应由用户触发)
  static unsigned long lastGeminiRequestMillis = 0;
  if (WiFi.status() == WL_CONNECTED && millis() - lastGeminiRequestMillis > 30000) { // 每30秒发送一次请求
    Serial.println("Sending test prompt to Gemini...");
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_WHITE);
    tft.fillScreen(ST77XX_BLACK);
    tft.println("Asking Gemini...");
    String geminiResponse = sendToGemini("Hello, what is the capital of France?");
    Serial.print("Gemini Response: ");
    Serial.println(geminiResponse);
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(ST77XX_WHITE);
    tft.println("Gemini Says:");
    tft.println(geminiResponse);
    lastGeminiRequestMillis = millis();
  }

  // 保持 WiFi 连接
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    tft.setCursor(0, 150);
    tft.setTextColor(ST77XX_RED);
    tft.println("WiFi Disconnected!");
    WiFi.reconnect();
  }

  // WS2812 颜色变化示例
  static unsigned long lastColorChangeMillis = 0;
  if (millis() - lastColorChangeMillis > 500) { // 每500ms改变一次颜色
    static byte hue = 0;
    leds[0] = CHSV(hue++, 255, 255);
    FastLED.show();
    lastColorChangeMillis = millis();
  }

  // 示例：每秒打印一次心跳信息和LED闪烁
  static unsigned long lastMillis = 0;
  static bool ledState = false;
  if (millis() - lastMillis > 1000) {
    Serial.println("Heartbeat...");
    lastMillis = millis();
    ledState = !ledState;
    digitalWrite(LED1_PIN, ledState); // LED1 闪烁
    digitalWrite(LED2_PIN, !ledState); // LED2 交替闪烁
  }

  // 读取按键状态 (高电平触发)
  // 为了避免屏幕内容被快速覆盖，这里只在按键按下时打印到串口
  // 实际应用中需要更复杂的UI更新逻辑
  if (digitalRead(KEY1_PIN) == HIGH) {
    Serial.println("KEY1 Pressed!");
    tft.setCursor(0, 40);
    tft.setTextColor(ST77XX_YELLOW);
    tft.println("KEY1 Pressed!");
    delay(100); // 简单的消抖
    tft.fillRect(0, 40, TFT_WIDTH, 16, ST77XX_BLACK); // 清除显示 (textsize 2 * 8 pixels)
    // 模拟键盘按键 (示例)
    Keyboard.print("Hello from ESP32-S3!");
    Keyboard.press(KEY_LEFT_GUI); // Windows键
    Keyboard.press('r');
    Keyboard.releaseAll();
    delay(100);
    Keyboard.print("notepad");
    Keyboard.press(KEY_RETURN);
    Keyboard.releaseAll();
  }
  if (digitalRead(KEY2_PIN) == HIGH) {
    Serial.println("KEY2 Pressed!");
    tft.setCursor(0, 60);
    tft.setTextColor(ST77XX_CYAN);
    tft.println("KEY2 Pressed!");
    delay(100);
    tft.fillRect(0, 60, TFT_WIDTH, 16, ST77XX_BLACK); // 清除显示 (textsize 2 * 8 pixels)
  }
  if (digitalRead(KEY3_PIN) == HIGH) {
    Serial.println("KEY3 Pressed!");
    tft.setCursor(0, 80);
    tft.setTextColor(ST77XX_MAGENTA);
    tft.println("KEY3 Pressed!");
    delay(100);
    tft.fillRect(0, 80, TFT_WIDTH, 16, ST77XX_BLACK); // 清除显示 (textsize 2 * 8 pixels)
  }

  // 检查 UART1 接收数据 (示例)
  if (Serial1.available()) {
    String data = Serial1.readStringUntil('\n');
    Serial.print("UART1 Received: ");
    Serial.println(data);
  }

  // 检查 UART2 接收数据 (示例)
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n');
    Serial.print("UART2 Received: ");
    Serial.println(data);
  }
}
