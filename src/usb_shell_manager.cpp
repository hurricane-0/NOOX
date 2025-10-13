/**
 * @file usb_shell_manager.cpp
 * @brief USB Shell管理器的实现文件
 * 
 * 该文件实现了与主机之间的双向通信，包括：
 * - JSON消息的序列化和反序列化
 * - USB复合设备的初始化和管理
 * - Shell命令和AI响应的处理
 * - 模拟键盘操作启动主机代理
 */

#include <ArduinoJson.h>    // JSON处理库
#include "usb_shell_manager.h"
#include "llm_manager.h"    // AI管理器
#include "wifi_manager.h"   // WiFi管理器
#include "USBHIDKeyboard.h" // HID键盘模拟

// 定义JSON文档缓冲区大小
const size_t JSON_DOC_SIZE = 1024; 

// 创建HID键盘实例
USBHIDKeyboard Keyboard;

/**
 * @brief 构造函数，初始化AI管理器和WiFi管理器
 */
UsbShellManager::UsbShellManager(LLMManager* llmManager, AppWiFiManager* wifiManager)
    : _llmManager(llmManager), _wifiManager(wifiManager) {
    // 初始化成员变量
}

/**
 * @brief 初始化USB设备和调试串口
 * 
 * 该方法完成：
 * 1. USB复合设备的初始化
 * 2. CDC串口通信的设置
 * 3. 调试串口的配置
 */
void UsbShellManager::begin() {
    USB.begin();           // 初始化USB复合设备
    _cdc.begin();         // 初始化CDC串口
    Serial.begin(115200); // 初始化调试串口
    Serial.println("UsbShellManager initialized. Waiting for USB connection...");
}

/**
 * @brief 更新AI管理器实例
 */
void UsbShellManager::setLLMManager(LLMManager* llmManager) {
    _llmManager = llmManager;
}

/**
 * @brief 主循环函数
 * 
 * 处理USB通信和其他周期性任务
 */
void UsbShellManager::loop() {
    handleUsbSerialData(); // 处理USB串口数据
    // 可以在此添加其他周期性任务
}

/**
 * @brief 处理USB串口接收到的数据
 * 
 * 该方法：
 * 1. 逐字符读取CDC串口数据
 * 2. 将数据累积到输入缓冲区
 * 3. 当接收到换行符时，处理完整的JSON消息
 */
void UsbShellManager::handleUsbSerialData() {
    if (_cdc.available()) {
        char c = _cdc.read();
        _inputBuffer += c;

        // 假定消息以换行符结尾，且为JSON格式
        if (c == '\n') {
            Serial.print("Received from host: ");
            Serial.println(_inputBuffer);
            processHostMessage(_inputBuffer);
            _inputBuffer = ""; // 处理完毕后清空缓冲区
        }
    }
}

/**
 * @brief 处理来自主机的JSON消息
 * 
 * 该方法解析和处理以下类型的消息：
 * - userInput: 用户输入，转发给AI处理
 * - linkTest: 链路测试请求
 * - connectToWifi: WiFi连接请求
 * - shellCommandResult: Shell命令执行结果
 * 
 * @param message JSON格式的消息字符串
 */
void UsbShellManager::processHostMessage(const String& message) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    // 检查JSON解析是否成功
    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        sendToHost("{\"type\":\"error\",\"content\":\"Invalid JSON\"}");
        return;
    }

    // 提取消息类型和请求ID
    String type = doc["type"].as<String>();
    String requestId = doc["requestId"] | ""; // 提取请求ID，如果不存在则为空字符串

    if (type == "userInput") {
        String payload = doc["payload"] | "";
        Serial.print("User input: ");
        Serial.println(payload);
        // Forward to LLMManager with requestId
        _llmManager->processUserInput(requestId, payload);
    } else if (type == "linkTest") {
        String payload = doc["payload"] | "";
        Serial.print("Received linkTest: ");
        Serial.println(payload);
        // Respond with linkTestResult
        sendLinkTestResultToHost(requestId, true, "pong");
    } else if (type == "connectToWifi") {
        String ssid = doc["payload"]["ssid"] | "";
        String password = doc["payload"]["password"] | "";
        Serial.print("Received connectToWifi for SSID: ");
        Serial.println(ssid);
        // Forward to WiFiManager
        bool success = _wifiManager->connectToWiFi(ssid, password); // Corrected case to connectToWiFi
        sendWifiConnectStatusToHost(requestId, success, success ? "Connected" : "Failed to connect");
    } else if (type == "shellCommandResult") { // Changed from "shellResult"
        String command = doc["payload"]["command"] | ""; // Assuming command is part of payload for context
        String shellStdout = doc["payload"]["stdout"] | "";
        String shellStderr = doc["payload"]["stderr"] | "";
        String status = doc["status"] | "error"; // New status field
        int exitCode = doc["exitCode"] | -1; // New exitCode field
        
        Serial.print("Shell output for '");
        Serial.print(command);
        Serial.print("':\nSTDOUT: ");
        Serial.println(shellStdout);
        Serial.print("STDERR: ");
        Serial.println(shellStderr);
        Serial.print("Status: ");
        Serial.println(status);
        Serial.print("Exit Code: ");
        Serial.println(exitCode);

        // Forward to LLMManager with context and requestId
        _llmManager->processShellOutput(requestId, command, shellStdout, shellStderr, status, exitCode);
    } else {
        Serial.print("Unknown message type: ");
        Serial.println(type);
        sendToHost(String("{\"type\":\"error\",\"payload\":\"Unknown message type\",\"requestId\":\"") + requestId + String("\"}"));
    }
}

/**
 * @brief 通过CDC串口向主机发送消息
 * 
 * @param message 要发送的消息字符串
 */
void UsbShellManager::sendToHost(const String& message) {
    _cdc.println(message);        // 通过CDC串口发送消息
    Serial.print("Sent to host: ");
    Serial.println(message);      // 同时在调试串口输出
}

/**
 * @brief 向主机发送Shell命令请求
 * 
 * 构造JSON格式：
 * {
 *   "requestId": "xxx",
 *   "type": "shellCommand",
 *   "payload": "command"
 * }
 * 
 * @param requestId 请求ID
 * @param command 要执行的Shell命令
 */
void UsbShellManager::sendShellCommandToHost(const String& requestId, const String& command) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "shellCommand";
    doc["payload"] = command;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

/**
 * @brief 向主机发送AI响应
 * 
 * 构造JSON格式：
 * {
 *   "requestId": "xxx",
 *   "type": "aiResponse",
 *   "payload": "AI generated response"
 * }
 * 
 * @param requestId 请求ID
 * @param response AI生成的响应文本
 */
void UsbShellManager::sendAiResponseToHost(const String& requestId, const String& response) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "aiResponse";
    doc["payload"] = response;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

/**
 * @brief 向主机发送链路测试结果
 * 
 * 构造JSON格式：
 * {
 *   "requestId": "xxx",
 *   "type": "linkTestResult",
 *   "status": "success/error",
 *   "payload": "pong"
 * }
 * 
 * @param requestId 请求ID
 * @param success 测试是否成功
 * @param payload 测试结果数据（通常是"pong"）
 */
void UsbShellManager::sendLinkTestResultToHost(const String& requestId, bool success, const String& payload) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "linkTestResult";
    doc["status"] = success ? "success" : "error";
    doc["payload"] = payload;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

void UsbShellManager::sendWifiConnectStatusToHost(const String& requestId, bool success, const String& message) {
    JsonDocument doc;
    doc["requestId"] = requestId;
    doc["type"] = "wifiConnectStatus";
    doc["status"] = success ? "success" : "error";
    doc["payload"] = message;
    String output;
    serializeJson(doc, output);
    sendToHost(output);
}

/**
 * @brief 模拟键盘操作启动主机代理程序（实验性功能）
 * 
 * 【重要说明】
 * 此功能目前存在以下问题，不建议使用：
 * 
 * 1. **路径问题**：agent.exe 存储在设备的 MSD（U盘）中，盘符由操作系统动态分配
 * 2. **时序问题**：USB设备枚举需要时间，设备接入后立即运行可能找不到U盘
 * 3. **权限问题**：某些系统可能需要管理员权限才能执行
 * 4. **兼容性问题**：仅支持Windows，且依赖PowerShell
 * 
 * 【推荐做法】
 * 1. 用户连接设备后，系统会显示"NOOX_Agent"或"NOOXDisk"U盘
 * 2. 用户从U盘复制 noox-host-agent.exe 到本地目录（如 C:\NOOX\）
 * 3. 用户手动运行：noox-host-agent.exe --wifi-status=disconnected
 * 4. 或设置开机自启动（通过快捷方式或任务计划程序）
 * 
 * 【改进方向】
 * 如果要实现自动启动，更好的方案是：
 * - 提供Windows安装程序，将agent复制到Program Files并添加到PATH
 * - 使用设备驱动程序自动安装（需要签名，成本较高）
 * 
 * @param wifiStatus 当前WiFi状态，作为代理程序的启动参数
 */
void UsbShellManager::simulateKeyboardLaunchAgent(const String& wifiStatus) {
    Serial.println("WARNING: simulateKeyboardLaunchAgent is experimental and may not work reliably.");
    Serial.println("Attempting to launch agent via keyboard simulation...");
    
    Keyboard.begin();
    delay(2000); // 给主机更多时间识别所有USB设备（HID、CDC、MSC）

    // 在Windows系统上：
    // 1. 按Win+R打开运行对话框
    // 2. 启动PowerShell
    // 3. 通过PowerShell脚本查找NOOX设备并运行agent.exe
    Keyboard.press(KEY_LEFT_GUI); // 按下Windows键
    Keyboard.press('r');
    Keyboard.releaseAll();
    delay(800); // 等待运行对话框打开

    // 启动PowerShell（使用-NoExit保持窗口打开以便查看错误）
    Keyboard.print("powershell -NoExit");
    Keyboard.press(KEY_RETURN);
    Keyboard.releaseAll();
    delay(4500); // 等待PowerShell窗口打开（增加延迟以适应较慢的系统）

    // PowerShell脚本：
    // 1. 查找卷标包含"NOOX"的驱动器
    // 2. 如果找到，从该驱动器运行 noox-host-agent.exe
    // 3. 如果未找到，提示用户手动运行
    String psCommand = "$ErrorActionPreference='Stop'; ";
    psCommand += "$drive = (Get-Volume | Where-Object {$_.FileSystemLabel -like '*NOOX*' -or $_.FileSystemLabel -like '*NOOXDisk*'} | Select-Object -First 1 -ExpandProperty DriveLetter); ";
    psCommand += "if ($drive) { ";
    psCommand += "Write-Host 'Found NOOX device on drive' $drive':'; ";
    psCommand += "$agentPath = \"${drive}:\\noox-host-agent.exe\"; ";
    psCommand += "if (Test-Path $agentPath) { ";
    psCommand += "Write-Host 'Launching agent...'; ";
    psCommand += "Start-Process $agentPath -ArgumentList '--wifi-status=" + wifiStatus + "'; ";
    psCommand += "} else { ";
    psCommand += "Write-Host 'Error: noox-host-agent.exe not found on NOOX device'; ";
    psCommand += "}; ";
    psCommand += "} else { ";
    psCommand += "Write-Host 'Error: NOOX device not found. Please run agent manually from NOOX Disk.'; ";
    psCommand += "}";
    
    Keyboard.print(psCommand);
    delay(200);
    Keyboard.press(KEY_RETURN);
    Keyboard.releaseAll();
    delay(500);
    
    Keyboard.end();
    Serial.println("Keyboard simulation complete. Check PowerShell window for results.");
    Serial.println("If launch failed, please manually run noox-host-agent.exe from the NOOX device.");
}
