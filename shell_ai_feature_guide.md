# NOOX Shell & AI 交互功能指南

## 1. 功能概述 (Feature Overview)

此功能为 NOOX 设备提供一个统一的跨平台 USB 接口，允许用户在计算机终端上同时实现两大核心操作：

1.  **计算机 Shell 操作：** 直接在终端中执行标准的 Shell 命令，并实时查看输出。
2.  **AI 对话交互：** 在同一终端窗口中，与 NOOX 内置的 AI 模型进行自然语言对话，获取智能分析和回复。

该方案通过将 ESP32-S3 配置为 **USB 复合设备**，并结合一个轻量级的 **主机代理程序**，实现了即插即用的体验，无需在主机上预装任何依赖或配置环境。

## 2. 技术架构 (Technical Architecture)

### 2.1 ESP32 USB 复合设备

ESP32-S3 将被配置为一个 USB 复合设备，同时提供以下三个关键接口：

*   **`HID` (Human Interface Device):** 保留现有的模拟键盘/鼠标功能。
*   **`CDC` (Communication Device Class):** 创建一个虚拟串口，作为 ESP32 与主机代理程序之间双向数据通信的桥梁。
*   **`MSD` (Mass Storage Device):** 模拟一个 U 盘，用于存放主机代理程序的可执行文件，方便用户直接运行。

### 2.2 主机代理程序

这是一个运行在用户计算机上的 **自包含原生可执行文件** (无需安装任何解释器或依赖)。其核心职责包括：

*   **启动与连接：** 通过 ESP32 的 `HID` 模拟键盘操作，在主机上运行 MSD 模拟的 U 盘内的 Go 代理程序，并传入 WiFi 状态参数。随后，自动检测并连接到 ESP32 提供的 `CDC` 虚拟串口。
*   **转发用户输入：** 捕获终端用户的输入，并通过 `CDC` 串口转发给 ESP32。
*   **执行 Shell 命令：** 接收来自 ESP32 的 Shell 命令，并在本地计算机的 Shell 环境中执行。
*   **回传结果：** 捕获 Shell 命令的输出，并连同 AI 的回复，统一通过 `CDC` 串口回传给 ESP32。
*   **显示输出：** 将从 ESP32 接收到的所有信息（包括 Shell 输出和 AI 回复）打印到终端，呈现给用户。
*   **WiFi 状态处理：** 根据启动参数中的 WiFi 状态，执行 `linktest`。如果 WiFi 未连接，尝试获取主机连接的 WiFi 信息并回传给设备进行连接。

### 2.3 通信协议

ESP32 与主机代理程序之间通过 `CDC` 虚拟串口进行通信，采用基于 **JSON** 的消息格式。这种格式结构清晰，易于扩展。

**JSON 消息示例 (优化后):**

*   **主机 -> ESP32 (用户输入):**
    ```json
    {
      "requestId": "uuid-user-1",
      "type": "userInput",
      "payload": "list all files in the current directory"
    }
    ```
*   **主机 -> ESP32 (测试通信):**
    ```json
    {
      "requestId": "uuid-linktest-1",
      "type": "linkTest",
      "payload": "ping"
    }
    ```
*   **主机 -> ESP32 (连接 WiFi):**
    ```json
    {
      "requestId": "uuid-wifi-1",
      "type": "connectToWifi",
      "payload": {
        "ssid": "YourWifiSSID",
        "password": "YourWifiPassword"
      }
    }
    ```
*   **主机 -> ESP32 (Shell 命令执行结果):**
    ```json
    {
      "requestId": "uuid-shell-1",
      "type": "shellCommandResult",
      "status": "success",
      "payload": "file1.txt\nfile2.txt",
      "stderr": "",
      "exitCode": 0
    }
    ```

*   **ESP32 -> 主机 (请求执行 Shell 命令):**
    ```json
    {
      "requestId": "uuid-shell-1",
      "type": "shellCommand",
      "payload": "ls -l"
    }
    ```
*   **ESP32 -> 主机 (发送 AI 回复):**
    ```json
    {
      "requestId": "uuid-ai-1",
      "type": "aiResponse",
      "payload": "Here are the files in the current directory:"
    }
    ```
*   **ESP32 -> 主机 (测试通信结果):**
    ```json
    {
      "requestId": "uuid-linktest-1",
      "type": "linkTestResult",
      "status": "success",
      "payload": "pong"
    }
    ```
*   **ESP32 -> 主机 (WiFi 连接状态):**
    ```json
    {
      "requestId": "uuid-wifi-1",
      "type": "wifiConnectStatus",
      "status": "success",
      "payload": "Connected to YourWifiSSID"
    }
    ```

### 2.4 输入路由与 AI 集成

ESP32 固件中的 **`UsbShellManager`** 是整个交互流程的核心路由组件。

1.  **接收输入:** `UsbShellManager` 通过 `CDC` 串口接收来自主机的所有数据，包括用户的初始请求和 Shell 命令的执行结果。
2.  **AI 处理与决策:**
    *   所有接收到的信息（`userInput` 和 `shellCommandResult`）被统一传递给 **`llmManager`** 进行智能处理。
    *   `llmManager` 会根据用户的提示和完整的上下文，决策并调用内部的 `sendtoshell` 工具。
    *   `sendtoshell` 工具的参数将明确指示是需要主机执行的 **Shell 命令** (`type: "command"`)，还是 **AI 的自然语言回复** (`type: "text"`)。
3.  **统一输出:** `UsbShellManager` 根据 `llmManager` 对 `sendtoshell` 工具的调用结果，将其封装为 `shellCommand` 或 `aiResponse` JSON 消息，通过 `CDC` 串口统一发送回主机代理程序，最终由代理程序在终端上显示给用户。

## 3. 开发指南 (Development Guide)

### 3.1 ESP32 固件开发

*   **USB 复合设备配置:**
    *   在项目中启用 `USB_HID`, `USB_CDC`, 和 `USB_MSC` 功能。
*   **`UsbShellManager` 实现:**
    *   创建 `UsbShellManager` 类，负责处理 `CDC` 通信逻辑。
    *   接收用户请求和 Shell 输出，并将其转发给 `llmManager`。
    *   将 `llmManager` 生成的 Shell 命令和 AI 回复统一发送回主机。
*   **与 `LLMManager` 集成:**
    *   确保 `UsbShellManager` 能够正确调用 `llmManager` 的接口来处理 AI 查询。
*   **MSD 文件系统管理:**
    *   配置并挂载一个文件系统 (如 LittleFS) 到 MSD。
    *   将预编译好的主机代理可执行文件存放在 `data` 目录下，以便用户访问。

### 3.2 主机代理程序开发

*   **语言选择与跨平台编译:**
    *   推荐使用 **Go** 或 **Rust**，因为它们能轻松生成无依赖的单一可执行文件。
    *   需要为 Windows, macOS, 和 Linux 三大平台进行交叉编译。
*   **串口通信:**
    *   使用标准库或第三方库来实现与 ESP32 `CDC` 虚拟串口的稳定连接。
*   **Shell 命令执行:**
    *   调用操作系统的标准接口来执行 Shell 命令，并可靠地捕获其标准输出 (stdout) 和标准错误 (stderr)。
*   **输入/输出处理:**
    *   实现一个循环来监听终端的标准输入 (stdin) 并转发给 ESP32。
    *   同时，异步监听来自 ESP32 的数据，并将其打印到终端的标准输出 (stdout)。

### 3.3 用户工作流程

1.  **连接设备:** 用户通过 USB 线将 NOOX 设备连接到计算机。
2.  **自动识别:** 计算机自动识别出三个设备：一个标准的键盘/鼠标，一个虚拟串口，以及一个 U 盘。
3.  **启动代理:** ESP32 通过 `HID` 模拟键盘操作，在主机上自动运行 MSD 模拟的 U 盘内的 Go 代理程序 (例如 `agent.exe` 或 `agent-macos`)，并传入当前 WiFi 状态参数。
4.  **WiFi 状态处理与通信测试:**
    *   Go 代理程序启动后，首先发送 `linktest` 消息测试与设备的双向通信。
    *   如果 ESP32 初始 WiFi 未连接，Go 代理程序会尝试获取主机当前连接的 WiFi 信息，并通过 `connectToWifi` 消息回传给设备进行连接。
5.  **开始交互:** 代理程序完成初始化后，用户即可在当前终端窗口中输入自然语言与 AI 对话。所有交互（包括 Shell 命令的执行和 AI 的回复）都会在这个窗口中无缝进行。
