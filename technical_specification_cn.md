# NOOX 智能硬件平台技术规格书

## 1. 项目概述

本文档旨在详细阐述 NOOX 智能硬件平台的技术规格。该平台基于 ESP32-S3 微控制器，深度集成大型语言模型（LLM），旨在提供一个强大的人机交互和任务自动化解决方案。平台支持通过 Web UI 和创新的 USB Shell & AI 统一接口与用户进行交互，并能通过模拟 HID 设备控制主机PC。

## 2. 系统架构

### 2.1 硬件架构

- **主控芯片**: ESP32-S3，提供强大的计算能力和丰富的外设接口。
- **显示**: 0.96" OLED (I2C, SSD1315 驱动)，用于实时显示设备状态。
- **输入**: 三枚物理按键（OK/上/下），用于本地 GUI 交互。
- **通信**:
    - Wi-Fi (2.4GHz)，用于连接互联网和本地网络 Web UI。
    - USB Type-C，支持复合设备模式 (CDC/HID/MSC)。

### 2.2 软件架构

软件系统采用模块化设计，基于 PlatformIO 和 Arduino 框架开发。核心模块包括：

- **主程序 (`main.cpp`)**: 系统入口，负责初始化所有管理器模块并启动 FreeRTOS 任务调度。
- **配置管理器 (`ConfigManager`)**: 负责 `config.json` 的读写，实现配置持久化。
- **硬件管理器 (`HardwareManager`)**: 封装底层硬件（OLED、按键）的初始化和控制。
- **UI 管理器 (`UIManager`)**: 负责在 OLED 屏幕上渲染图形用户界面。
- **Wi-Fi 管理器 (`WiFiManager`)**: 管理 Wi-Fi 连接、扫描和凭据存储。
- **Web 管理器 (`WebManager`)**: 提供基于 ESPAsyncWebServer 的 Web 服务和 WebSocket 通信。
- **LLM 管理器 (`LLMManager`)**: 封装与 LLM API 的交互逻辑，支持多种模型。
- **USB Shell 管理器 (`UsbShellManager`)**: 管理与主机的 USB CDC 通信，处理 JSON 消息。
- **HID 管理器 (`HidManager`)**: 负责模拟键盘和鼠标事件。
- **任务管理器 (`TaskManager`)**: (在此项目中，任务调度主要通过 FreeRTOS 实现，`TaskManager` 可能用于协调高级任务)。

## 3. 核心模块详细设计

### 3.1 LLM 管理器 (`llm_manager`)

- **功能**: 负责与 LLM API（如 DeepSeek, OpenAI）进行通信，获取模型响应。
- **设计**:
    - 使用 FreeRTOS 队列 (`llmRequestQueue`, `llmResponseQueue`) 实现异步请求处理。
    - 支持“聊天模式”和“高级模式”，后者允许 LLM 通过工具调用（Function Calling）与系统其他部分交互。
    - 动态生成系统提示词，根据当前模式和授权工具指导 LLM 的行为。
- **关键优化**:
    - **内存管理**: 为了解决在 PSRAM 环境下 `ArduinoJson` 的 `JsonDocument` 对象在析构时引发的堆损坏问题，本模块实现了一个关键的内存管理模式：**对象重用**。
    - **实现方式**: 在 `LLMManager` 类中定义了一个可重用的 `JsonDocument` 成员变量 (`reusableJsonDoc`)。在所有需要处理 JSON 的函数（如 `getOpenAILikeResponse` 和 `handleLLMRawResponse`）中，不再创建局部的 `JsonDocument` 实例，而是统一使用这个成员变量，并在每次使用前调用 `.clear()` 方法。
    - **效果**: 此重构彻底避免了因局部对象生命周期结束而导致的内存冲突，显著提升了系统的稳定性和可靠性。

### 3.2 USB Shell & AI 交互管理器 (`usb_shell_manager`)

- **功能**: 建立一个统一的 USB 接口，允许用户在 PC 终端上同时执行 Shell 命令和与设备内置的 AI 对话。
- **设计**:
    - **USB 复合设备**: 将 ESP32-S3 配置为 CDC (虚拟串口) + HID (键盘/鼠标) + MSC (U盘) 的复合设备。
    - **主机代理**: 通过 MSC 模式向主机提供一个轻量级的 Go 语言编写的代理程序，实现即插即用。
    - **通信协议**: 设备与主机代理之间通过 CDC 串口进行通信，采用基于 JSON 的消息格式。
- **关键优化**:
    - **内存管理**: 与 `LLMManager` 类似，`UsbShellManager` 在处理频繁的 JSON 消息时也面临同样的堆损坏风险。
    - **实现方式**: 同样采用了**对象重用**模式。在 `UsbShellManager` 类中增加了一个 `JsonDocument` 成员变量 (`reusableJsonDoc`)。所有需要序列化或反序列化 JSON 的方法（如 `processHostMessage`, `sendShellCommandToHost` 等）均使用此共享实例。
    - **效果**: 统一了项目中的 JSON 内存管理策略，确保了高频 USB 通信下的系统稳定性。

### 3.3 Web 管理器 (`web_manager`)

- **功能**: 提供 Web 界面，用于设备配置和与 LLM 交互。
- **设计**:
    - 使用 `ESPAsyncWebServer` 库搭建 HTTP 服务器，托管 `data` 目录下的 Gzip 压缩后的静态资源 (HTML, CSS, JS)。
    - 实现 WebSocket 服务器，用于设备与 Web UI 之间的实时双向通信。
    - 提供 RESTful API 接口，用于处理来自 Web UI 的配置更新请求（如 WiFi、LLM 设置）。

### 3.4 配置管理器 (`config_manager`)

- **功能**: 管理设备的持久化配置。
- **设计**:
    - 使用 LittleFS 文件系统。
    - 所有配置项（WiFi凭据、LLM API密钥、模型选择等）存储在根目录的 `config.json` 文件中。
    - 提供加载 (`loadConfig`) 和保存 (`saveConfig`) 配置的接口。

## 4. 通信协议

### 4.1 USB CDC 消息协议

设备与主机代理程序之间通过 JSON 字符串进行通信，消息体包含 `type`, `requestId`, 和 `payload` 字段。

- **主机 -> 设备**:
    - `userInput`: 用户在终端输入的内容。
    - `shellCommandResult`: 主机执行 Shell 命令后的结果。
    - `connectToWifi`: 请求设备连接到指定 WiFi。
    - `linkTest`: 链路测试。

- **设备 -> 主机**:
    - `shellCommand`: 请求主机执行一条 Shell 命令。
    - `aiResponse`: LLM 生成的自然语言回复。
    - `wifiConnectStatus`: WiFi 连接状态更新。
    - `linkTestResult`: 链路测试响应。

### 4.2 WebSocket 消息协议

设备与 Web UI 之间通过 WebSocket 传递 JSON 消息，用于实时更新状态和传递用户输入。

## 5. 结论

NOOX 平台通过模块化的软件设计，结合强大的 ESP32-S3 硬件，实现了功能丰富的 AIoT 应用。特别是通过对核心通信模块 (`LLMManager` 和 `UsbShellManager`) 进行内存管理重构，采用 `JsonDocument` 对象重用策略，成功解决了关键的系统稳定性问题，为平台的长期可靠运行奠定了坚实基础。
