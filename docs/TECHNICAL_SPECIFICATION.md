# NOOX 智能硬件平台技术规格文档

## 文档信息

- **项目名称**: NOOX ESP32-S3 AI 智能硬件平台
- **版本**: 3.0
- **文档日期**: 2025-10-12
- **文档状态**: 最终版
- **维护人**: NOOX 开发团队

---

## 目录

1. [项目概述](#1-项目概述)
2. [系统架构](#2-系统架构)
3. [硬件规格](#3-硬件规格)
4. [软件架构](#4-软件架构)
5. [核心模块详解](#5-核心模块详解)
6. [通信协议](#6-通信协议)
7. [数据结构](#7-数据结构)
8. [内存管理](#8-内存管理)
9. [配置管理](#9-配置管理)
10. [安全性设计](#10-安全性设计)
11. [性能指标](#11-性能指标)
12. [开发环境](#12-开发环境)
13. [API参考](#13-api参考)

---

## 1. 项目概述

### 1.1 项目简介

NOOX 是一个基于 ESP32-S3 的智能硬件平台，集成了大语言模型（LLM）能力，提供多模态交互接口。设备可以通过 Web 界面、USB Shell 和 OLED 屏幕与用户交互，并通过 USB HID 控制主机计算机。

### 1.2 核心特性

- **双模式 LLM 集成**：支持聊天模式和高级模式，提供不同级别的 AI 交互能力
- **多通道交互**：Web 界面、USB Shell、OLED + 按键
- **USB 复合设备**：同时提供 HID、CDC、MSD 功能
- **跨平台 Shell 交互**：通过主机代理程序实现终端级别的 AI 交互
- **对话历史管理**：支持多轮对话上下文保持
- **动态配置管理**：支持运行时修改 LLM 提供商、API 密钥和 WiFi 配置

### 1.3 应用场景

- 智能桌面助手
- 计算机自动化控制
- 跨平台命令行 AI 交互
- IoT 设备集成平台
- 开发者工具链助手

---

## 2. 系统架构

### 2.1 总体架构

```
┌─────────────────────────────────────────────────────────────┐
│                        用户交互层                             │
├───────────────┬───────────────┬──────────────┬──────────────┤
│  Web浏览器    │  OLED+按键    │  USB Shell   │  USB HID     │
│  (WebSocket)  │  (本地显示)   │  (CDC串口)   │  (键盘/鼠标)  │
└───────┬───────┴───────┬───────┴───────┬──────┴──────┬───────┘
        │               │               │             │
        ▼               ▼               ▼             ▼
┌─────────────────────────────────────────────────────────────┐
│                      应用管理层                               │
├──────────────┬──────────────┬──────────────┬────────────────┤
│ WebManager   │ UIManager    │UsbShellMgr   │ HIDManager     │
└──────┬───────┴──────┬───────┴──────┬───────┴────────┬───────┘
       │              │              │                │
       └──────────────┴──────────────┴────────┬───────┘
                                              ▼
                                    ┌──────────────────┐
                                    │   LLMManager     │
                                    │  (AI核心引擎)    │
                                    └────────┬─────────┘
                                             │
                    ┌────────────────────────┼────────────────────────┐
                    ▼                        ▼                        ▼
          ┌─────────────────┐    ┌──────────────────┐    ┌──────────────────┐
          │  WiFiManager    │    │ ConfigManager    │    │ HardwareManager  │
          │  (网络连接)      │    │  (配置管理)      │    │  (硬件抽象层)    │
          └─────────────────┘    └──────────────────┘    └──────────────────┘
                    │                        │                        │
                    └────────────────────────┴────────────────────────┘
                                             ▼
                                    ┌──────────────────┐
                                    │   ESP32-S3硬件   │
                                    │ (Flash/PSRAM)    │
                                    └──────────────────┘
```

### 2.2 模块职责

| 模块 | 职责 | 依赖 |
|------|------|------|
| **ConfigManager** | 配置文件读写、LittleFS 管理 | LittleFS |
| **HardwareManager** | GPIO、I2C、LED、按键控制 | FastLED, U8g2 |
| **WiFiManager** | WiFi 连接管理、网络状态监控 | ConfigManager |
| **LLMManager** | LLM API 调用、对话历史、工具调用 | WiFiManager, ConfigManager |
| **UIManager** | OLED 显示、按键输入、菜单导航 | HardwareManager, WiFiManager, LLMManager |
| **HIDManager** | USB 键盘/鼠标模拟 | USB |
| **WebManager** | Web 服务器、WebSocket 通信 | LLMManager, WiFiManager, ConfigManager |
| **UsbShellManager** | CDC 串口通信、JSON 消息处理 | LLMManager, WiFiManager |

### 2.3 任务调度

项目使用 FreeRTOS 多任务调度，核心任务如下：

```cpp
// 任务配置
xTaskCreatePinnedToCore(webTask, "WebTask", 4096, NULL, 2, NULL, 0);    // Core 0
xTaskCreatePinnedToCore(uiTask, "UITask", 4096, NULL, 2, NULL, 1);      // Core 1
xTaskCreatePinnedToCore(usbTask, "USBTask", 4096, NULL, 2, NULL, 1);    // Core 1
xTaskCreatePinnedToCore(llmTask, "LLMTask", 32768, NULL, 2, NULL, 1);   // Core 1
```

**任务分配原则**：
- Core 0: 网络密集型任务（Web 服务器）
- Core 1: 计算密集型任务（LLM、UI、USB）

---

## 3. 硬件规格

### 3.1 主控制器

- **芯片型号**: ESP32-S3-WROOM-1
- **CPU**: Xtensa® 双核 32 位 LX7 @ 240 MHz
- **ROM**: 384 KB
- **SRAM**: 512 KB
- **RTC SRAM**: 16 KB
- **Flash**: 16 MB (模组内置)
- **PSRAM**: 8 MB (模组内置, QSPI)
- **WiFi**: 802.11 b/g/n (2.4 GHz)
- **USB**: USB 2.0 OTG (GPIO19/GPIO20)

### 3.2 外设接口

#### 3.2.1 显示器

| 参数 | 规格 |
|------|------|
| 类型 | OLED |
| 尺寸 | 0.96 英寸 |
| 分辨率 | 128x64 像素 |
| 驱动芯片 | SSD1315 |
| 接口 | I2C (GPIO4-SDA, GPIO5-SCL) |
| 地址 | 0x3C |

#### 3.2.2 输入设备

| 按键 | GPIO | 功能 | 触发电平 |
|------|------|------|----------|
| Button 1 (OK) | GPIO47 | 选择/确认 | HIGH |
| Button 2 (Up) | GPIO21 | 向上/滚动 | HIGH |
| Button 3 (Down) | GPIO38 | 向下/滚动 | HIGH |

**配置要求**: 内部下拉电阻 (INPUT_PULLDOWN)

#### 3.2.3 输出设备

| LED | GPIO | 用途 | 驱动电平 |
|-----|------|------|----------|
| LED 1 | GPIO41 | 状态指示 | HIGH |
| LED 2 | GPIO40 | 状态指示 | HIGH |
| LED 3 | GPIO39 | 状态指示 | HIGH |
| RGB LED | GPIO48 | WS2812 彩色指示 | PWM |

**限流电阻**: 220Ω-1kΩ (单色 LED)

#### 3.2.4 扩展接口

**I2C 预留** (Wire1):
- SDA: GPIO1
- SCL: GPIO2
- 上拉电阻: 4.7kΩ

**SPI (TF 卡)**:
- CS: GPIO6
- SCK: GPIO15
- MISO: GPIO16
- MOSI: GPIO7

**UART 扩展**:
- TX: GPIO17
- RX: GPIO18

**通用 GPIO**:
- GPIO8, GPIO9, GPIO10 (预留)

### 3.3 禁用引脚

**不可使用** (启动稳定性):
- Strapping Pins: GPIO0, GPIO3, GPIO45, GPIO46
- PSRAM Pins: GPIO35, GPIO36, GPIO37

### 3.4 电源规格

- **输入电压**: 5V (USB Type-C)
- **工作电流**: 
  - 空闲: ~80 mA
  - WiFi 活跃: ~150-250 mA
  - 峰值 (HTTPS 请求): ~300 mA
- **功耗**: 典型 1W，峰值 1.5W

---

## 4. 软件架构

### 4.1 开发环境

- **框架**: Arduino Framework
- **IDE**: PlatformIO
- **编译工具链**: Espressif32
- **文件系统**: LittleFS
- **RTOS**: FreeRTOS (ESP-IDF 内置)

### 4.2 核心库依赖

```ini
lib_deps =
  olikraus/U8g2@^2.35.8              # OLED 显示库
  fastled/FastLED@^3.6.0             # RGB LED 控制
  AsyncTCP@^1.1.1                    # 异步 TCP 库
  me-no-dev/ESPAsyncWebServer@^3.6.0 # 异步 Web 服务器
  WebSockets@^2.3.7                  # WebSocket 支持
  bblanchon/ArduinoJson@^7.0.4       # JSON 解析
```

### 4.3 编译标志

```ini
build_flags = 
  -Os                                     # 尺寸优化
  -DCONFIG_MBEDTLS_DYNAMIC_BUFFER=y       # 动态 TLS 缓冲
  -DCONFIG_MBEDTLS_SSL_MAX_CONTENT_LEN=16384
  -DCORE_DEBUG_LEVEL=4                    # 详细日志
  -DARDUINOJSON_USE_PSRAM=1               # JSON 使用 PSRAM
  -DARDUINO_USB_MSC_ON_BOOT=1             # USB MSC 启动
  -DARDUINO_USB_HID_ON_BOOT=1             # USB HID 启动
```

### 4.4 分区表

使用自定义分区表 `partitions_16MB_app_large.csv`，针对 16MB Flash 优化：

| 分区名 | 类型 | 子类型 | 起始地址 | 大小 | 用途 |
|--------|------|--------|----------|------|------|
| nvs | data | nvs | 0x9000 | 20K | NVS 存储 |
| otadata | data | ota | 0xE000 | 8K | OTA 数据 |
| app0 | app | ota_0 | 0x10000 | 6M | 应用程序 |
| app1 | app | ota_1 | 0x610000 | 6M | OTA 备份 |
| spiffs | data | spiffs | 0xC10000 | 3.9M | LittleFS 文件系统 |

---

## 5. 核心模块详解

### 5.1 ConfigManager (配置管理器)

#### 5.1.1 功能职责

- 管理设备所有配置参数
- 从 LittleFS 加载 `config.json`
- 运行时配置更新
- 配置持久化存储

#### 5.1.2 配置文件结构

```json
{
  "last_used": {
    "llm_provider": "deepseek",
    "model": "deepseek-chat",
    "wifi_ssid": "YourWiFi"
  },
  "llm_providers": {
    "deepseek": {
      "api_key": "sk-xxxxx",
      "models": ["deepseek-chat", "deepseek-reasoner"]
    },
    "openrouter": {
      "api_key": "sk-or-v1-xxxxx",
      "models": [
        "z-ai/glm-4.5-air:free",
        "qwen/qwen3-235b-a22b:free"
      ]
    },
    "openai": {
      "api_key": "sk-xxxxx",
      "models": ["gpt-4o", "gpt-3.5-turbo"]
    }
  },
  "wifi_networks": [
    {
      "ssid": "YourWiFi",
      "password": "YourPassword"
    }
  ]
}
```

#### 5.1.3 核心 API

```cpp
class ConfigManager {
public:
    ConfigManager();
    bool begin();                    // 初始化 LittleFS
    bool loadConfig();               // 加载配置
    bool saveConfig();               // 保存配置
    JsonDocument& getConfig();       // 获取配置文档引用
};
```

#### 5.1.4 配置加载流程

```
开始
  ↓
初始化 LittleFS
  ↓
尝试打开 /config.json
  ↓
  └─ 文件存在? ──否→ 创建默认配置 → 保存到文件
       │
      是
       ↓
  解析 JSON
       ↓
  验证配置
       ↓
    返回成功
```

---

### 5.2 HardwareManager (硬件管理器)

#### 5.2.1 功能职责

- GPIO 初始化和控制
- OLED 显示驱动
- 按键输入读取
- LED 状态控制
- RGB LED 颜色管理

#### 5.2.2 核心 API

```cpp
class HardwareManager {
public:
    void begin();                                          // 初始化所有硬件
    
    // OLED 控制
    U8G2_SSD1315_128X64_NONAME_F_HW_I2C& getDisplay();
    
    // LED 控制
    void setLedState(int ledPin, bool state);              // 单色 LED
    void setRgbColor(CRGB color);                          // RGB LED
    
    // GPIO 控制 (预留)
    void setGpio1State(bool state);
    void setGpio2State(bool state);
    
    // 按键输入
    bool getButtonA();                                     // OK/选择
    bool getButtonB();                                     // 向上
    bool getButtonC();                                     // 向下
    
    // LLM 集成接口
    bool setGpioOutput(const String& gpioName, bool state); // 通过名称控制
    String getAvailableGpios();                             // 获取可用 GPIO 列表
};
```

#### 5.2.3 按键消抖

```cpp
// 消抖实现（在 UIManager 中）
const unsigned long DEBOUNCE_DELAY = 200;  // 200ms
static unsigned long lastButtonPressTime = 0;

void UIManager::handleButtonInput() {
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        if (hardware.getButtonA()) {
            buttonA_event = true;
            lastButtonPressTime = currentTime;
        }
        // ... B, C 同理
    }
}
```

---

### 5.3 WiFiManager (WiFi 管理器)

#### 5.3.1 功能职责

- WiFi 连接管理
- 自动重连机制
- 多网络配置管理
- 连接状态监控

#### 5.3.2 核心 API

```cpp
class AppWiFiManager {
public:
    AppWiFiManager(ConfigManager& config);
    void begin();                                           // 初始化并连接到上次使用的网络
    void loop();                                            // 维护连接状态
    
    // 连接管理
    bool connectToWiFi(const String& ssid, const String& password);
    void disconnect();
    
    // 配置管理
    bool addWiFi(const String& ssid, const String& password);
    bool deleteWiFi(const String& ssid);
    JsonArray getSavedSSIDs();
    
    // 状态查询
    String getIPAddress();
    String getWiFiStatus();                                 // "Connected", "Connecting", "Failed", "Idle"
    String getSSID();
};
```

#### 5.3.3 连接状态机

```cpp
enum WiFiConnectionState {
    IDLE,        // 空闲
    CONNECTING,  // 连接中
    CONNECTED,   // 已连接
    FAILED       // 连接失败
};
```

**状态转换**:

```
IDLE ─────┐
    ↑     │ connectToWiFi()
    │     ↓
  超时  CONNECTING
    │     │
    │     ├──→ 成功 ──→ CONNECTED
    │     │               │
    └─────┴──→ 失败       │ 断线
                ↓         ↓
              FAILED → 重连 → CONNECTING
```

#### 5.3.4 自动重连机制

```cpp
void AppWiFiManager::handleWiFiConnection() {
    if (_connectionState == CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            _connectionState = CONNECTED;
            // 更新 config.json 中的 last_used.wifi_ssid
            configManager.getConfig()["last_used"]["wifi_ssid"] = WiFi.SSID();
            configManager.saveConfig();
        } else if (millis() - _connectionAttemptStartTime > WIFI_CONNECTION_TIMEOUT_MS) {
            _connectionState = FAILED;
            WiFi.disconnect();
        }
    } else if (_connectionState == CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi connection lost. Reconnecting...");
            connectToLastSSID();  // 自动重连
        }
    }
}
```

---

### 5.4 LLMManager (大语言模型管理器)

#### 5.4.1 功能职责

这是整个系统的核心模块，负责：

- LLM API 调用（支持 DeepSeek, OpenRouter, OpenAI）
- 对话历史管理（环形缓冲区）
- 工具调用解析和执行
- 双模式切换（聊天模式 / 高级模式）
- 系统提示词动态生成
- FreeRTOS 队列通信

#### 5.4.2 核心数据结构

**LLM 请求结构**：

```cpp
struct LLMRequest {
    char requestId[64];    // 请求 ID（固定大小，避免浅拷贝问题）
    char* prompt;          // 用户输入（PSRAM 分配）
    LLMMode mode;          // 模式：CHAT_MODE 或 ADVANCED_MODE
};
```

**LLM 响应结构**：

```cpp
struct LLMResponse {
    char requestId[64];                // 对应的请求 ID
    bool isToolCall;                   // 是否为工具调用
    char toolName[32];                 // 工具名称
    char* toolArgs;                    // 工具参数 JSON（PSRAM）
    char* naturalLanguageResponse;     // 自然语言回复（PSRAM）
};
```

**对话历史结构**：

```cpp
struct ConversationMessage {
    char* role;       // "user" 或 "assistant" (PSRAM)
    char* content;    // 消息内容 (PSRAM)
};

class ConversationHistory {
    ConversationMessage* messages;  // 环形缓冲区（PSRAM）
    size_t capacity;                // 容量（默认 40）
    size_t count;                   // 当前消息数
    size_t startIndex;              // 环形缓冲起始索引
};
```

#### 5.4.3 核心 API

```cpp
class LLMManager {
public:
    LLMManager(ConfigManager& config, AppWiFiManager& wifi, 
               UsbShellManager* usbShell, HIDManager* hid, 
               HardwareManager* hardware);
    
    void begin();                                       // 初始化
    void loop();                                        // 主循环（运行在独立任务）
    
    // 请求处理
    void processUserInput(const String& requestId, const String& userInput);
    void processShellOutput(const String& requestId, const String& cmd,
                           const String& output, const String& error,
                           const String& status, int exitCode);
    
    // 模式管理
    void setCurrentMode(LLMMode mode);
    String getCurrentMode() const;                      // "Chat" 或 "Advanced"
    String getCurrentModelName();
    
    // 对话历史
    void clearConversationHistory();
    
    // FreeRTOS 队列
    QueueHandle_t llmRequestQueue;   // 请求队列（深度 3）
    QueueHandle_t llmResponseQueue;  // 响应队列（深度 3）
};
```

#### 5.4.4 工作流程

**请求处理流程**：

```
用户输入 (Web/USB)
    ↓
创建 LLMRequest
    ↓
分配 PSRAM 内存
    ↓
发送到 llmRequestQueue
    ↓
LLM 任务接收请求
    ↓
构建系统提示词
    ↓
添加对话历史
    ↓
发送 HTTPS 请求到 LLM API
    ↓
接收并解析响应
    ↓
判断：工具调用 or 自然语言?
    ↓
├─ 工具调用 → 解析工具名称和参数 → 执行工具 → 获取结果 → 反馈给 LLM
│                                                         ↓
└─ 自然语言 ─────────────────────────────────────────────┘
                                                          ↓
                                             创建 LLMResponse
                                                          ↓
                                             发送到 llmResponseQueue
                                                          ↓
                                      WebManager/UsbShellManager 接收
                                                          ↓
                                              返回给用户
```

#### 5.4.5 系统提示词生成

**聊天模式**：

```cpp
String LLMManager::generateSystemPrompt(LLMMode mode) {
    if (mode == CHAT_MODE) {
        return "你是一个友善、热情、乐于助人的AI助手。"
               "请用简洁、自然的语言回答用户的问题。";
    }
    // ...
}
```

**高级模式**：

```cpp
// 高级模式会包含工具描述
String prompt = "你是一个高级AI助手，能够调用以下工具来完成任务：\n\n";

// 添加 Shell 命令工具
prompt += "## 工具：execute_shell_command\n";
prompt += "描述：在用户的计算机上执行 Shell 命令\n";
prompt += "参数：{ \"command\": \"要执行的命令\" }\n";
prompt += "使用时机：当用户要求操作文件、查看系统信息、运行程序时\n\n";

// 添加 HID 控制工具
prompt += "## 工具：control_keyboard\n";
prompt += "描述：模拟键盘按键，输入文本或快捷键\n";
prompt += "参数：{ \"action\": \"type|press\", \"value\": \"内容或按键组合\" }\n";
prompt += "示例：{ \"action\": \"press\", \"value\": \"Ctrl+C\" }\n\n";

return prompt;
```

#### 5.4.6 内存优化措施

**问题**: FreeRTOS 队列的浅拷贝导致堆损坏

**解决方案**:
1. 使用固定大小的 char 数组存储短字符串（requestId, toolName）
2. 使用 PSRAM 指针存储长字符串（prompt, toolArgs, naturalLanguageResponse）
3. 明确内存所有权：发送方分配，接收方释放

```cpp
// 发送方（WebManager）
char* LLMManager::allocateAndCopy(const String& str) {
    if (str.length() == 0) return nullptr;
    size_t len = str.length() + 1;
    char* buffer = (char*)ps_malloc(len);  // PSRAM 分配
    if (buffer) {
        strncpy(buffer, str.c_str(), len);
        buffer[len - 1] = '\0';
    }
    return buffer;
}

// 接收方（WebManager）
LLMResponse response;
if (xQueueReceive(llmManager.llmResponseQueue, &response, 0) == pdPASS) {
    // 使用 response...
    
    // 释放内存
    if (response.toolArgs) free(response.toolArgs);
    if (response.naturalLanguageResponse) free(response.naturalLanguageResponse);
}
```

#### 5.4.7 对话历史管理

**环形缓冲区实现**：

```cpp
void ConversationHistory::addMessage(const String& role, const String& content) {
    size_t index;
    if (count < capacity) {
        index = count++;        // 未满，直接追加
    } else {
        index = startIndex;     // 已满，覆盖最旧的消息
        
        // 释放被覆盖的消息内存
        if (messages[index].role) free(messages[index].role);
        if (messages[index].content) free(messages[index].content);
        
        startIndex = (startIndex + 1) % capacity;  // 移动起始索引
    }
    
    // 分配新消息内存（PSRAM）
    messages[index].role = allocateAndCopy(role);
    messages[index].content = allocateAndCopy(content);
}
```

**容量**: 40 条消息（约 20 轮对话）

---

### 5.5 UIManager (用户界面管理器)

#### 5.5.1 功能职责

- OLED 显示内容管理
- 多级菜单系统
- 按键输入处理
- 状态信息展示

#### 5.5.2 UI 状态机

```cpp
enum UIState {
    UI_STATE_STATUS,            // 状态显示屏幕
    UI_STATE_MAIN_MENU,         // 主菜单
    UI_STATE_WIFI_MENU,         // WiFi 菜单
    UI_STATE_SAVED_WIFI_LIST    // 已保存 WiFi 列表
};
```

**状态转换**：

```
UI_STATE_STATUS ←──────────────────────┐
    │                                   │
    │ 按下 Button A (OK)                │
    ↓                                   │
UI_STATE_MAIN_MENU                      │
    │                                   │
    ├─ 选择 "WiFi Settings" + OK        │
    │   ↓                               │
    │  UI_STATE_WIFI_MENU               │
    │   │                               │
    │   ├─ "Disconnect" + OK ───────────┘
    │   │
    │   ├─ "Connect Saved" + OK
    │   │   ↓
    │   │  UI_STATE_SAVED_WIFI_LIST
    │   │   │
    │   │   └─ 选择网络 + OK ───────────┘
    │   │
    │   └─ "Scan Networks" + OK ────────┘
    │
    └─ 选择 "System Info" + OK ─────────┘
```

#### 5.5.3 显示内容

**状态屏幕** (UI_STATE_STATUS):

```
┌────────────────────────┐
│ Mode: Chat             │ ← LLM 模式
│ Model: deepseek-chat   │ ← 当前模型
│ SSID: MyWiFi           │ ← WiFi 名称
│ IP: 192.168.1.100      │ ← IP 地址 (或 WiFi 状态)
│ Mem: 45% (280KB)       │ ← 内存使用率
└────────────────────────┘
```

**主菜单** (UI_STATE_MAIN_MENU):

```
┌────────────────────────┐
│ Main Menu              │
│                        │
│ > WiFi Settings        │ ← 当前选中项
│   System Info          │
└────────────────────────┘
```

#### 5.5.4 按键映射

| 按键 | 状态屏幕 | 菜单屏幕 |
|------|----------|----------|
| Button A (OK) | 进入主菜单 | 选择当前项 |
| Button B (Up) | 无操作 | 向上移动 |
| Button C (Down) | 无操作 | 向下移动 |

#### 5.5.5 核心代码

```cpp
void UIManager::update() {
    buttonA_event = false;
    buttonB_event = false;
    buttonC_event = false;
    
    handleButtonInput();  // 消抖处理
    
    switch (currentState) {
        case UI_STATE_STATUS:
            handleStateStatus();
            break;
        case UI_STATE_MAIN_MENU:
            handleStateMainMenu();
            break;
        // ... 其他状态
    }
}

void UIManager::drawStatusScreen() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        
        // 获取信息
        String mode = llmManager.getCurrentMode();
        String model = llmManager.getCurrentModelName();
        String ssid = wifi.getSSID();
        String ip = wifi.getIPAddress();
        
        // 内存信息
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t totalHeap = ESP.getHeapSize();
        int memPercent = (totalHeap - freeHeap) * 100 / totalHeap;
        
        // 显示
        hardware.getDisplay().drawStr(0, 10, ("Mode: " + mode).c_str());
        hardware.getDisplay().drawStr(0, 22, ("Model: " + model).c_str());
        hardware.getDisplay().drawStr(0, 34, ("SSID: " + ssid).c_str());
        hardware.getDisplay().drawStr(0, 46, ("IP: " + ip).c_str());
        hardware.getDisplay().drawStr(0, 58, 
            ("Mem: " + String(memPercent) + "% (" + String(freeHeap/1024) + "KB)").c_str());
        
    } while (hardware.getDisplay().nextPage());
}
```

---

### 5.6 HIDManager (人机接口设备管理器)

#### 5.6.1 功能职责

- USB HID 键盘模拟
- USB HID 鼠标模拟
- 快捷键组合
- 宏操作执行

#### 5.6.2 核心 API

```cpp
class HIDManager {
public:
    void begin();
    
    // 基础操作
    void sendKey(char key);                          // 发送单个字符
    void sendString(const String& str);              // 发送字符串
    void moveMouse(int x, int y);                    // 移动鼠标
    void clickMouse(int button);                     // 点击鼠标
    
    // 高级操作
    void openApplication(const String& appName);     // 打开应用 (Win+R)
    void runCommand(const String& command);          // 运行命令
    void takeScreenshot();                           // 截屏 (Print Screen)
    void simulateKeyPress(uint8_t key, uint8_t modifiers = 0);
    
    // LLM 集成
    bool pressKeyCombination(const String& keys);    // 按键组合："Ctrl+C"
    bool pressSpecialKey(const String& keyName);     // 特殊键："F1", "Home"
    bool pressMediaKey(const String& mediaKey);      // 媒体键："VolumeUp"
    bool executeMacro(const JsonArray& actions);     // 执行宏
    
    // 状态查询
    bool isReady();
    String getLastError();
};
```

#### 5.6.3 按键组合解析

**支持的修饰键**:
- Ctrl / Control
- Shift
- Alt
- Win / Meta / GUI / Cmd

**解析逻辑**:

```cpp
bool HIDManager::pressKeyCombination(const String& keys) {
    // 分割字符串：例如 "Ctrl+Shift+A" → ["Ctrl", "Shift", "A"]
    String parts[5];
    int partCount = 0;
    // ... 分割逻辑
    
    // 按下修饰键
    uint8_t modifiers = 0;
    for (int i = 0; i < partCount - 1; i++) {
        uint8_t mod = parseModifier(parts[i]);
        if (mod == 0) return false;  // 未知修饰键
        keyboard.press(mod);
    }
    
    // 按下主键
    String mainKey = parts[partCount - 1];
    uint8_t keyCode = parseSpecialKeyCode(mainKey);
    if (keyCode != 0) {
        keyboard.press(keyCode);
    } else if (mainKey.length() == 1) {
        keyboard.press(mainKey.charAt(0));
    } else {
        keyboard.releaseAll();
        return false;  // 未知按键
    }
    
    delay(50);
    keyboard.releaseAll();
    return true;
}
```

**支持的特殊键**:
- 功能键: F1-F12
- 导航键: Home, End, PageUp, PageDown, Insert, Delete
- 方向键: Up, Down, Left, Right
- 其他: Enter, Tab, Backspace, Escape, Space

#### 5.6.4 宏操作

**JSON 格式**:

```json
[
  { "action": "type", "value": "echo Hello" },
  { "action": "press", "key": "Enter" },
  { "action": "delay", "ms": 1000 },
  { "action": "click", "button": 1 },
  { "action": "move", "x": 100, "y": 50 }
]
```

**执行逻辑**:

```cpp
bool HIDManager::executeMacro(const JsonArray& actions) {
    for (JsonVariant action : actions) {
        JsonObject actionObj = action.as<JsonObject>();
        String actionType = actionObj["action"];
        
        if (actionType == "type") {
            keyboard.print(actionObj["value"].as<String>());
        } else if (actionType == "press") {
            pressKeyCombination(actionObj["key"].as<String>());
        } else if (actionType == "delay") {
            delay(actionObj["ms"] | 100);
        } else if (actionType == "click") {
            clickMouse(actionObj["button"] | MOUSE_BUTTON_LEFT);
        } else if (actionType == "move") {
            moveMouse(actionObj["x"] | 0, actionObj["y"] | 0);
        }
    }
    return true;
}
```

---

### 5.7 WebManager (Web 服务器管理器)

#### 5.7.1 功能职责

- 异步 HTTP 服务器
- WebSocket 双向通信
- 静态文件托管（HTML/CSS/JS）
- LLM 请求代理
- 配置更新处理

#### 5.7.2 核心 API

```cpp
class WebManager {
public:
    WebManager(LLMManager& llm, AppWiFiManager& wifi, 
               ConfigManager& config, HardwareManager& hardware);
    
    void begin();                               // 启动 Web 服务器
    void loop();                                // 处理响应和配置更新
    void broadcast(const String& message);      // WebSocket 广播
    void setLLMMode(LLMMode mode);             // 设置 LLM 模式
};
```

#### 5.7.3 HTTP 路由

| 路由 | 方法 | 功能 | 返回 |
|------|------|------|------|
| `/` | GET | 主页面 | `index.html` |
| `/style.css` | GET | 样式表 | `style.css` |
| `/script.js` | GET | JavaScript | `script.js` |
| `/ws` | WebSocket | 双向通信 | - |

**文件压缩**: 所有静态文件使用 gzip 压缩 (`.gz`)

#### 5.7.4 WebSocket 消息协议

**客户端 → 服务器**:

```json
// 发送聊天消息
{
  "type": "chat_message",
  "text": "你好，帮我列出当前目录的文件"
}

// 更新配置
{
  "type": "update_config",
  "config": { /* 完整的 config.json 内容 */ }
}

// 清除对话历史
{
  "type": "clear_history"
}

// 控制 GPIO
{
  "type": "gpio_control",
  "gpio": "led1",
  "state": true
}
```

**服务器 → 客户端**:

```json
// LLM 文本响应
{
  "type": "chat_message",
  "sender": "bot",
  "text": "当前目录的文件列表：..."
}

// 工具调用通知
{
  "type": "tool_call",
  "tool_name": "execute_shell_command",
  "tool_args": {
    "command": "ls -la"
  }
}

// 配置更新状态
{
  "type": "config_update_status",
  "status": "success",
  "message": "Configuration saved."
}

// 对话历史已清除
{
  "type": "history_cleared",
  "status": "success",
  "message": "对话历史已清除"
}
```

#### 5.7.5 配置更新流程

```
Web 客户端发送 update_config
    ↓
WebManager 接收
    ↓
设置 configUpdatePending = true
    ↓
存储到 pendingConfigDoc
    ↓
WebManager::loop() 检测到 pending
    ↓
应用配置到 configManager
    ↓
调用 configManager.saveConfig()
    ↓
重新初始化 LLMManager 和 WiFiManager
    ↓
广播更新状态给所有 WebSocket 客户端
```

**为什么需要延迟处理**？

WebSocket 事件回调在网络线程中执行，直接调用 `saveConfig()` 可能导致文件系统访问冲突。通过设置标志并在主循环中处理，确保线程安全。

---

### 5.8 UsbShellManager (USB Shell 管理器)

#### 5.8.1 功能职责

- CDC 虚拟串口通信
- JSON 消息序列化/反序列化
- Shell 命令转发
- AI 响应回传
- WiFi 连接协调

#### 5.8.2 核心 API

```cpp
class UsbShellManager {
public:
    UsbShellManager(LLMManager* llm, AppWiFiManager* wifi);
    
    void begin();                                     // 初始化 USB CDC
    void loop();                                      // 处理串口数据
    void setLLMManager(LLMManager* llm);             // 设置 LLM 引用
    
    // 发送消息到主机
    void sendShellCommandToHost(const String& requestId, const String& cmd);
    void sendAiResponseToHost(const String& requestId, const String& response);
    void sendLinkTestResultToHost(const String& requestId, bool success, const String& payload);
    void sendWifiConnectStatusToHost(const String& requestId, bool success, const String& message);
    
    // 模拟键盘启动代理
    void simulateKeyboardLaunchAgent(const String& wifiStatus);
};
```

#### 5.8.3 消息处理流程

```
主机通过 CDC 发送 JSON
    ↓
UsbShellManager::handleUsbSerialData() 接收
    ↓
累积到 _inputBuffer
    ↓
检测到换行符 '\n'
    ↓
processHostMessage(buffer)
    ↓
解析 JSON
    ↓
根据 type 字段分发：
    │
    ├─ "userInput" → 转发给 LLMManager::processUserInput()
    │
    ├─ "linkTest" → 发送 linkTestResult 回复
    │
    ├─ "connectToWifi" → 调用 WiFiManager::connectToWiFi()
    │
    └─ "shellCommandResult" → 转发给 LLMManager::processShellOutput()
```

#### 5.8.4 与主机代理的协作

**主机代理程序** (Go):
- 自动检测 ESP32 的 CDC 串口
- 启动时执行 linkTest 测试通信
- 如果 WiFi 未连接，获取主机 WiFi 信息并发送给 ESP32
- 接收用户输入，转发给 ESP32
- 接收 Shell 命令请求，在主机执行，回传结果
- 显示 AI 响应

**启动流程**:

```
1. ESP32 启动
    ↓
2. 检测 WiFi 状态
    ↓
3. 通过 HID 模拟键盘，运行 MSD U 盘中的代理程序
    ↓
    传参：agent.exe --wifi-status=disconnected
    ↓
4. 代理程序启动
    ↓
5. 发送 linkTest 到 ESP32
    ↓
6. ESP32 回复 linkTestResult
    ↓
7. 如果 wifiStatus == "disconnected"
    ↓
    代理获取主机 WiFi → 发送 connectToWifi → ESP32 连接
    ↓
8. 等待用户输入
```

---

## 6. 通信协议

### 6.1 WebSocket 协议

#### 6.1.1 客户端到服务器

| 消息类型 | 字段 | 说明 | 示例 |
|----------|------|------|------|
| `chat_message` | `text` | 用户聊天消息 | `{"type":"chat_message", "text":"Hello"}` |
| `update_config` | `config` | 更新配置 | `{"type":"update_config", "config":{...}}` |
| `clear_history` | 无 | 清除对话历史 | `{"type":"clear_history"}` |
| `gpio_control` | `gpio`, `state` | 控制 GPIO | `{"type":"gpio_control", "gpio":"led1", "state":true}` |

#### 6.1.2 服务器到客户端

| 消息类型 | 字段 | 说明 |
|----------|------|------|
| `chat_message` | `sender`, `text` | AI 响应 |
| `tool_call` | `tool_name`, `tool_args` | 工具调用通知 |
| `config_update_status` | `status`, `message` | 配置更新结果 |
| `history_cleared` | `status`, `message` | 历史清除确认 |

---

### 6.2 USB CDC 协议

#### 6.2.1 消息格式

所有消息为 **单行 JSON** + **换行符**：

```
{"requestId":"uuid","type":"userInput","payload":"hello"}\n
```

#### 6.2.2 主机到 ESP32

| 类型 | 字段 | 说明 |
|------|------|------|
| `userInput` | `requestId`, `payload` | 用户输入 |
| `linkTest` | `requestId`, `payload` | 通信测试 |
| `connectToWifi` | `requestId`, `payload:{ssid, password}` | WiFi 连接请求 |
| `shellCommandResult` | `requestId`, `payload:{command, stdout, stderr}, status, exitCode` | Shell 执行结果 |

#### 6.2.3 ESP32 到主机

| 类型 | 字段 | 说明 |
|------|------|------|
| `shellCommand` | `requestId`, `payload` | 请求执行 Shell 命令 |
| `aiResponse` | `requestId`, `payload` | AI 响应 |
| `linkTestResult` | `requestId`, `status`, `payload` | 测试结果 |
| `wifiConnectStatus` | `requestId`, `status`, `payload` | WiFi 连接结果 |

---

### 6.3 LLM API 协议

#### 6.3.1 支持的提供商

| 提供商 | API 端点 | 认证方式 |
|--------|----------|----------|
| DeepSeek | `https://api.deepseek.com/v1/chat/completions` | Bearer Token |
| OpenRouter | `https://openrouter.ai/api/v1/chat/completions` | Bearer Token |
| OpenAI | `https://api.openai.com/v1/chat/completions` | Bearer Token |

#### 6.3.2 请求格式 (OpenAI 兼容)

```json
{
  "model": "deepseek-chat",
  "messages": [
    {
      "role": "system",
      "content": "你是一个AI助手..."
    },
    {
      "role": "user",
      "content": "用户的问题"
    },
    {
      "role": "assistant",
      "content": "AI的上次回复"
    },
    {
      "role": "user",
      "content": "用户的新问题"
    }
  ],
  "temperature": 0.7,
  "max_tokens": 2000
}
```

**高级模式额外字段**:

```json
{
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "execute_shell_command",
        "description": "在用户计算机上执行Shell命令",
        "parameters": {
          "type": "object",
          "properties": {
            "command": {
              "type": "string",
              "description": "要执行的命令"
            }
          },
          "required": ["command"]
        }
      }
    }
  ],
  "tool_choice": "auto"
}
```

#### 6.3.3 响应格式

**普通响应**:

```json
{
  "choices": [
    {
      "message": {
        "role": "assistant",
        "content": "AI的回复内容"
      }
    }
  ]
}
```

**工具调用响应**:

```json
{
  "choices": [
    {
      "message": {
        "role": "assistant",
        "content": null,
        "tool_calls": [
          {
            "id": "call_abc123",
            "type": "function",
            "function": {
              "name": "execute_shell_command",
              "arguments": "{\"command\":\"ls -la\"}"
            }
          }
        ]
      }
    }
  ]
}
```

---

## 7. 数据结构

### 7.1 配置数据结构

```json
{
  "last_used": {
    "llm_provider": "string",    // 最后使用的 LLM 提供商
    "model": "string",            // 最后使用的模型
    "wifi_ssid": "string"         // 最后连接的 WiFi
  },
  "llm_providers": {
    "<provider_name>": {
      "api_key": "string",        // API 密钥
      "models": ["string", ...]   // 可用模型列表
    }
  },
  "wifi_networks": [
    {
      "ssid": "string",           // WiFi 名称
      "password": "string"        // WiFi 密码
    }
  ]
}
```

### 7.2 LLM 工具定义

```cpp
// Shell 命令工具
{
  "type": "function",
  "function": {
    "name": "execute_shell_command",
    "description": "在用户的计算机上执行Shell命令并返回结果",
    "parameters": {
      "type": "object",
      "properties": {
        "command": {
          "type": "string",
          "description": "要执行的Shell命令"
        }
      },
      "required": ["command"]
    }
  }
}

// HID 键盘控制工具
{
  "type": "function",
  "function": {
    "name": "control_keyboard",
    "description": "模拟键盘输入文本或按键",
    "parameters": {
      "type": "object",
      "properties": {
        "action": {
          "type": "string",
          "enum": ["type", "press"],
          "description": "动作类型：type=输入文本, press=按键组合"
        },
        "value": {
          "type": "string",
          "description": "文本内容或按键组合（如Ctrl+C）"
        }
      },
      "required": ["action", "value"]
    }
  }
}

// GPIO 控制工具
{
  "type": "function",
  "function": {
    "name": "control_gpio",
    "description": "控制ESP32的GPIO输出状态",
    "parameters": {
      "type": "object",
      "properties": {
        "gpio": {
          "type": "string",
          "enum": ["led1", "led2", "led3", "gpio1", "gpio2"],
          "description": "GPIO名称"
        },
        "state": {
          "type": "boolean",
          "description": "状态：true=高电平, false=低电平"
        }
      },
      "required": ["gpio", "state"]
    }
  }
}
```

---

## 8. 内存管理

### 8.1 内存布局

```
┌─────────────────────────────────────────────┐
│              ESP32-S3 内存架构               │
├─────────────────────────────────────────────┤
│ DRAM (512 KB)                               │
│ ├─ FreeRTOS 堆                              │
│ ├─ 全局变量                                 │
│ ├─ 任务栈                                   │
│ └─ 网络缓冲区                               │
├─────────────────────────────────────────────┤
│ PSRAM (8 MB)                                │
│ ├─ LLM 请求/响应队列                        │
│ ├─ 对话历史（ConversationHistory）          │
│ ├─ JSON 文档缓冲区（ArduinoJson）           │
│ ├─ HTTPS 响应缓冲区                         │
│ └─ 临时大对象                               │
├─────────────────────────────────────────────┤
│ Flash (16 MB)                               │
│ ├─ 程序代码（6 MB × 2，OTA）               │
│ ├─ LittleFS 文件系统（3.9 MB）             │
│ │  ├─ config.json                           │
│ │  ├─ index.html.gz                         │
│ │  ├─ style.css.gz                          │
│ │  └─ script.js.gz                          │
│ └─ NVS 存储（20 KB）                        │
└─────────────────────────────────────────────┘
```

### 8.2 PSRAM 使用策略

**启用 PSRAM 的编译标志**:

```ini
-DARDUINOJSON_USE_PSRAM=1
```

**手动 PSRAM 分配**:

```cpp
char* buffer = (char*)ps_malloc(size);  // 分配
// ... 使用
free(buffer);                           // 释放
```

**为什么使用 PSRAM**？

| 对象 | 大小 | 原因 |
|------|------|------|
| LLM 响应 | 最大 256 KB | API 响应可能很大 |
| 对话历史 | 约 100 KB (40 条消息) | 避免 DRAM 碎片化 |
| JSON 文档 | 256 KB | 解析 LLM 响应 |
| 队列消息 | 动态大小 | prompt 和 response 内容 |

### 8.3 内存泄漏防护

**问题**: FreeRTOS 队列浅拷贝导致的内存泄漏

**解决方案**:

1. **明确所有权**: 发送方分配 PSRAM，接收方释放
2. **避免浅拷贝**: 队列中存储指针，不存储对象
3. **统一分配函数**:

```cpp
char* LLMManager::allocateAndCopy(const String& str) {
    if (str.length() == 0) return nullptr;
    size_t len = str.length() + 1;
    char* buffer = (char*)ps_malloc(len);
    if (buffer) {
        strncpy(buffer, str.c_str(), len);
        buffer[len - 1] = '\0';
    } else {
        Serial.println("PSRAM allocation failed!");
    }
    return buffer;
}
```

4. **强制释放检查**:

```cpp
// 接收方
LLMResponse response;
if (xQueueReceive(queue, &response, 0) == pdPASS) {
    // 使用 response...
    
    // ✅ 必须释放！
    if (response.toolArgs) {
        free(response.toolArgs);
        response.toolArgs = nullptr;
    }
    if (response.naturalLanguageResponse) {
        free(response.naturalLanguageResponse);
        response.naturalLanguageResponse = nullptr;
    }
}
```

---

## 9. 配置管理

### 9.1 配置文件位置

**路径**: `/config.json` (LittleFS 根目录)

### 9.2 配置加载流程

```cpp
bool ConfigManager::loadConfig() {
    File configFile = LittleFS.open(configFilePath, "r");
    if (!configFile) {
        // 文件不存在，创建默认配置
        createDefaultConfig();
        return saveConfig();
    }
    
    DeserializationError error = deserializeJson(configDoc, configFile);
    configFile.close();
    
    if (error) {
        Serial.println("Failed to parse config file");
        return false;
    }
    
    Serial.println("Configuration loaded successfully.");
    return true;
}
```

### 9.3 配置保存流程

```cpp
bool ConfigManager::saveConfig() {
    File configFile = LittleFS.open(configFilePath, "w");
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }
    
    if (serializeJson(configDoc, configFile) == 0) {
        Serial.println("Failed to write to config file");
        configFile.close();
        return false;
    }
    
    configFile.close();
    Serial.println("Configuration saved.");
    return true;
}
```

### 9.4 运行时配置更新

**触发方式**:
1. Web 界面更新配置
2. USB Shell 命令
3. OLED 菜单操作

**更新流程**:

```
用户操作 → 修改 configDoc → saveConfig() → 重新初始化相关模块
```

**需要重启的模块**:
- LLMManager: 切换 LLM 提供商或模型时
- WiFiManager: 切换 WiFi 网络时

---

## 10. 安全性设计

### 10.1 网络安全

#### 10.1.1 HTTPS 支持

所有 LLM API 调用使用 HTTPS (TLS 1.2+)：

```cpp
WiFiClientSecure client;
client.setInsecure();  // 跳过证书验证（生产环境应配置 CA 证书）

HTTPClient https;
https.begin(client, apiUrl);
https.addHeader("Authorization", "Bearer " + apiKey);
```

**生产环境建议**: 配置 CA 证书进行服务器验证

#### 10.1.2 API 密钥管理

- 存储在 LittleFS 的 `config.json` 中
- 不通过 WebSocket 明文传输（除非更新配置）
- 建议：外部配置管理系统

### 10.2 输入验证

#### 10.2.1 JSON 解析

```cpp
JsonDocument doc;
DeserializationError error = deserializeJson(doc, message);
if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    sendToHost("{\"type\":\"error\",\"content\":\"Invalid JSON\"}");
    return;
}
```

#### 10.2.2 命令注入防护

Shell 命令通过 LLM 生成，但**不进行本地命令注入检查**（由主机代理执行）。

**主机代理的职责**:
- 限制可执行命令（可选）
- 记录所有执行的命令
- 沙箱执行（可选）

### 10.3 访问控制

#### 10.3.1 Web 界面

- 无身份验证（局域网访问）
- **生产环境建议**: 添加 HTTP Basic Auth 或 Token 认证

#### 10.3.2 USB Shell

- 物理访问控制（需要 USB 连接）
- 无额外认证机制

---

## 11. 性能指标

### 11.1 响应时间

| 操作 | 典型时间 | 说明 |
|------|----------|------|
| WiFi 连接 | 3-10 秒 | 取决于网络环境 |
| LLM API 调用 | 2-15 秒 | 取决于模型和网络延迟 |
| WebSocket 消息往返 | 10-50 ms | 局域网 |
| USB CDC 消息往返 | 5-20 ms | 直连 |
| OLED 刷新 | 50-100 ms | 取决于内容复杂度 |

### 11.2 并发能力

- **WebSocket 连接**: 最多 4 个客户端（AsyncWebServer 限制）
- **LLM 请求队列**: 深度 3（避免队列溢出）
- **FreeRTOS 任务**: 4 个核心任务

### 11.3 内存占用

| 模块 | DRAM 占用 | PSRAM 占用 |
|------|-----------|------------|
| 空闲 | ~80 KB | ~10 KB |
| WiFi 连接 | ~120 KB | ~20 KB |
| Web 服务器运行 | ~150 KB | ~50 KB |
| LLM 调用中 | ~180 KB | ~300 KB |
| **峰值** | ~200 KB | ~350 KB |

### 11.4 存储占用

- **程序代码**: 约 2.5 MB
- **静态文件**: 约 200 KB (压缩后)
- **配置文件**: 约 2 KB
- **剩余空间**: 约 3.7 MB (可用于日志、缓存等)

---

## 12. 开发环境

### 12.1 安装 PlatformIO

```bash
# 使用 VS Code 扩展（推荐）
# 或使用命令行
pip install platformio
```

### 12.2 克隆项目

```bash
git clone https://github.com/your-repo/NOOX.git
cd NOOX
```

### 12.3 配置硬件

1. 将自定义板定义添加到 PlatformIO：

```bash
cp boards/esp32s3_NOOX.json ~/.platformio/platforms/espressif32/boards/
```

2. 连接 ESP32-S3 开发板（USB Type-C）

### 12.4 编译和上传

```bash
# 编译
pio run

# 上传固件
pio run --target upload

# 上传文件系统（首次使用）
pio run --target uploadfs

# 打开串口监视器
pio device monitor
```

### 12.5 开发工具链

- **IDE**: Visual Studio Code + PlatformIO 扩展
- **串口工具**: PlatformIO Serial Monitor
- **调试工具**: ESP-IDF Monitor
- **Web 开发**: Chrome DevTools (WebSocket 调试)

---

## 13. API 参考

### 13.1 ConfigManager API

```cpp
// 初始化
ConfigManager configManager;
configManager.begin();
configManager.loadConfig();

// 访问配置
JsonDocument& config = configManager.getConfig();
String apiKey = config["llm_providers"]["deepseek"]["api_key"];

// 修改配置
config["last_used"]["model"] = "gpt-4o";
configManager.saveConfig();
```

### 13.2 WiFiManager API

```cpp
// 初始化
AppWiFiManager wifiManager(configManager);
wifiManager.begin();  // 自动连接到 last_used WiFi

// 连接到新网络
wifiManager.connectToWiFi("MyWiFi", "password");

// 查询状态
String status = wifiManager.getWiFiStatus();  // "Connected", "Connecting", etc.
String ip = wifiManager.getIPAddress();       // "192.168.1.100"

// 管理已保存网络
wifiManager.addWiFi("NewWiFi", "newpassword");
wifiManager.deleteWiFi("OldWiFi");
JsonArray networks = wifiManager.getSavedSSIDs();
```

### 13.3 LLMManager API

```cpp
// 初始化
LLMManager llmManager(configManager, wifiManager, usbShellManager, hidManager, hardwareManager);
llmManager.begin();

// 发送用户输入
llmManager.processUserInput("request-123", "你好，帮我列出文件");

// 设置模式
llmManager.setCurrentMode(ADVANCED_MODE);  // 或 CHAT_MODE

// 清除对话历史
llmManager.clearConversationHistory();

// 查询状态
String mode = llmManager.getCurrentMode();       // "Chat" 或 "Advanced"
String model = llmManager.getCurrentModelName(); // "deepseek-chat"
```

### 13.4 HIDManager API

```cpp
// 初始化
HIDManager hidManager;
hidManager.begin();

// 基础操作
hidManager.sendString("Hello World");
hidManager.moveMouse(100, 50);
hidManager.clickMouse(MOUSE_BUTTON_LEFT);

// 快捷键
hidManager.pressKeyCombination("Ctrl+C");
hidManager.pressSpecialKey("F5");
hidManager.pressMediaKey("VolumeUp");

// 宏
JsonDocument macroDoc;
JsonArray actions = macroDoc.to<JsonArray>();
JsonObject action1 = actions.add<JsonObject>();
action1["action"] = "type";
action1["value"] = "echo Hello";
JsonObject action2 = actions.add<JsonObject>();
action2["action"] = "press";
action2["key"] = "Enter";
hidManager.executeMacro(actions);
```

### 13.5 HardwareManager API

```cpp
// 初始化
HardwareManager hardwareManager;
hardwareManager.begin();

// OLED 显示
U8G2& display = hardwareManager.getDisplay();
display.clearBuffer();
display.drawStr(0, 20, "Hello");
display.sendBuffer();

// LED 控制
hardwareManager.setLedState(LED_1_PIN, true);  // 开启 LED 1
hardwareManager.setRgbColor(CRGB::Red);        // RGB LED 设置为红色

// GPIO 控制
hardwareManager.setGpioOutput("led1", true);
hardwareManager.setGpioOutput("gpio1", false);

// 按键读取
if (hardwareManager.getButtonA()) {
    // Button A 被按下
}
```

### 13.6 WebManager API

```cpp
// 初始化
WebManager webManager(llmManager, wifiManager, configManager, hardwareManager);
webManager.begin();  // 启动 Web 服务器在端口 80

// 广播消息到所有 WebSocket 客户端
webManager.broadcast("{\"type\":\"notification\",\"text\":\"Hello\"}");

// 设置 LLM 模式
webManager.setLLMMode(ADVANCED_MODE);
```

### 13.7 UsbShellManager API

```cpp
// 初始化
UsbShellManager usbShellManager(llmManager, wifiManager);
usbShellManager.begin();

// 发送消息到主机
usbShellManager.sendShellCommandToHost("req-123", "ls -la");
usbShellManager.sendAiResponseToHost("req-124", "当前目录的文件列表：...");

// 模拟键盘启动代理
usbShellManager.simulateKeyboardLaunchAgent("disconnected");
```

---

## 附录

### A. 常见问题

#### Q1: 设备启动后无法连接 WiFi

**排查步骤**:
1. 检查串口输出，确认 `WiFi.begin()` 被调用
2. 验证 `config.json` 中的 WiFi 配置是否正确
3. 确认路由器是否支持 2.4GHz（ESP32-S3 不支持 5GHz）
4. 尝试手动重启设备

#### Q2: LLM API 调用失败

**可能原因**:
- API 密钥无效
- 网络连接不稳定
- API 端点变更
- 请求超时（默认 40 秒）

**解决方案**:
- 检查串口输出中的 HTTPS 错误码
- 验证 API 密钥在提供商网站上是否有效
- 测试网络连接：`ping api.deepseek.com`

#### Q3: OLED 显示无内容

**排查步骤**:
1. 检查 I2C 接线（SDA=GPIO4, SCL=GPIO5）
2. 验证 OLED 地址（0x3C）
3. 测试 I2C 扫描代码
4. 确认 U8g2 库版本

#### Q4: USB HID 无响应

**可能原因**:
- USB 驱动未正确安装
- 编译标志未启用 HID
- 主机未识别设备

**解决方案**:
- 检查设备管理器（Windows）或 `lsusb`（Linux）
- 确认 `platformio.ini` 中包含 `-DARDUINO_USB_HID_ON_BOOT=1`
- 重新上传固件

---

### B. 术语表

| 术语 | 全称 | 说明 |
|------|------|------|
| ESP32-S3 | Espressif ESP32-S3 | 乐鑫科技的双核 MCU |
| LLM | Large Language Model | 大语言模型 |
| PSRAM | Pseudo Static RAM | 伪静态随机存取存储器 |
| CDC | Communication Device Class | USB 通信设备类（虚拟串口） |
| HID | Human Interface Device | USB 人机接口设备 |
| MSD | Mass Storage Device | USB 大容量存储设备 |
| OLED | Organic Light-Emitting Diode | 有机发光二极管显示器 |
| GPIO | General Purpose Input/Output | 通用输入输出 |
| I2C | Inter-Integrated Circuit | 集成电路间总线 |
| SPI | Serial Peripheral Interface | 串行外设接口 |
| FreeRTOS | Free Real-Time Operating System | 免费实时操作系统 |
| WebSocket | Web Socket Protocol | Web 套接字协议 |
| JSON | JavaScript Object Notation | JavaScript 对象表示法 |
| API | Application Programming Interface | 应用程序编程接口 |
| HTTPS | HTTP Secure | 超文本传输安全协议 |
| TLS | Transport Layer Security | 传输层安全协议 |

---

### C. 参考资料

- [ESP32-S3 技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_cn.pdf)
- [PlatformIO 文档](https://docs.platformio.org/)
- [Arduino-ESP32 文档](https://docs.espressif.com/projects/arduino-esp32/)
- [U8g2 库文档](https://github.com/olikraus/u8g2/wiki)
- [ArduinoJson 文档](https://arduinojson.org/)
- [ESPAsyncWebServer 文档](https://github.com/me-no-dev/ESPAsyncWebServer)
- [OpenAI API 文档](https://platform.openai.com/docs/api-reference)
- [DeepSeek API 文档](https://platform.deepseek.com/api-docs/)

---

## 变更历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 1.0 | 2025-01-15 | 初始版本 |
| 2.0 | 2025-10-10 | 添加对话历史功能、内存优化 |
| 3.0 | 2025-10-12 | 完善技术规格文档、添加 API 参考 |

---

**文档结束**

