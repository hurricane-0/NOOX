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
#include "USBHIDKeyboard.h" // HID键盘模拟

// 定义JSON文档缓冲区大小
const size_t JSON_DOC_SIZE = 1024; 

// 创建HID键盘实例
USBHIDKeyboard Keyboard;

/**
 * @brief 构造函数，初始化AI管理器和WiFi管理器
 */
UsbShellManager::UsbShellManager(LLMManager* llmManager)
    : _llmManager(llmManager) {
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
 * @brief 设置WiFi管理器实例
 */
void UsbShellManager::setWiFiManager(AppWiFiManager* wifiManager) {
    _wifiManager = wifiManager;
}

/**
 * @brief 处理WiFi凭证信息（格式：SSID|Password）
 * 
 * 该方法解析来自PowerShell的WiFi配置信息并尝试连接WiFi
 * 
 * @param message WiFi凭证字符串，格式为"SSID|Password"
 */
void UsbShellManager::processWiFiCredentials(const String& message) {
    // 检查消息是否符合格式：SSID|Password
    int separatorIndex = message.indexOf('|');
    if (separatorIndex > 0) {
        String ssid = message.substring(0, separatorIndex);
        String password = message.substring(separatorIndex + 1);
        
        Serial.printf("[CDC] Received WiFi credentials: SSID='%s', Password='%s'\n", ssid.c_str(), password.c_str());
        
        // 连接WiFi并保存
        if (_wifiManager) {
            Serial.println("[CDC] Attempting to connect to WiFi...");
            _wifiManager->addWiFi(ssid, password);
            bool success = _wifiManager->connectToWiFi(ssid, password);
            if (success) {
                Serial.println("[CDC] WiFi connection initiated successfully");
            } else {
                Serial.println("[CDC] Failed to initiate WiFi connection");
            }
        } else {
            Serial.println("[CDC] Error: WiFi manager not available");
        }
    } else {
        Serial.printf("[CDC] Invalid WiFi credentials format: %s\n", message.c_str());
        Serial.println("[CDC] Expected format: SSID|Password");
    }
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
 * 3. 当接收到换行符时，处理完整的消息（JSON或简单文本）
 */
void UsbShellManager::handleUsbSerialData() {
    if (_cdc.available()) {
        char c = _cdc.read();
        _inputBuffer += c;

        // 假定消息以换行符结尾
        if (c == '\n') {
            String message = _inputBuffer;
            message.trim();
            Serial.print("Received from host: ");
            Serial.println(message);
            
            // 如果消息以 '{' 开头则视为 JSON 格式并处理
            if (message.startsWith("{")) {
                processHostMessage(message);
            } else {
                // 检查是否是 WiFi 配置信息（格式：SSID|Password）
                processWiFiCredentials(message);
            }
            
            _inputBuffer = ""; // Clear buffer after processing
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
