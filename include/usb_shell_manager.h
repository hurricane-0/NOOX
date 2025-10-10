#ifndef USB_SHELL_MANAGER_H
#define USB_SHELL_MANAGER_H

/**
 * @file usb_shell_manager.h
 * @brief USB Shell管理器的头文件，负责处理ESP32与主机之间的USB通信
 * 
 * 该类实现了以下功能：
 * 1. USB复合设备功能（CDC串口、HID键盘、MSC存储）
 * 2. JSON格式的消息处理
 * 3. 与主机代理程序的双向通信
 * 4. Shell命令和AI响应的转发
 */

#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>
#include <USBCDC.h>
#include <USBMSC.h>
#include <ArduinoJson.h> // JSON解析库

// 前向声明LLMManager类（AI管理器）
class LLMManager;
// 前向声明WiFiManager类（WiFi管理器）
class AppWiFiManager;

class UsbShellManager {
public:
    /**
     * @brief 构造函数
     * @param llmManager AI管理器指针
     * @param wifiManager WiFi管理器指针
     */
    UsbShellManager(LLMManager* llmManager, AppWiFiManager* wifiManager);

    /**
     * @brief 初始化USB设备和串口通信
     */
    void begin();

    /**
     * @brief 主循环函数，处理USB通信
     */
    void loop();

    /**
     * @brief 设置AI管理器
     * @param llmManager 新的AI管理器指针
     */
    void setLLMManager(LLMManager* llmManager);

    // 向主机发送消息的方法
    /**
     * @brief 向主机发送Shell命令
     * @param requestId 请求ID，用于追踪请求-响应对
     * @param command 要执行的Shell命令
     */
    void sendShellCommandToHost(const String& requestId, const String& command);

    /**
     * @brief 向主机发送AI响应
     * @param requestId 请求ID
     * @param response AI生成的响应内容
     */
    void sendAiResponseToHost(const String& requestId, const String& response);

    /**
     * @brief 向主机发送链路测试结果
     * @param requestId 请求ID
     * @param success 测试是否成功
     * @param payload 测试结果负载（通常是"pong"）
     */
    void sendLinkTestResultToHost(const String& requestId, bool success, const String& payload);

    /**
     * @brief 向主机发送WiFi连接状态
     * @param requestId 请求ID
     * @param success 连接是否成功
     * @param message 状态消息
     */
    void sendWifiConnectStatusToHost(const String& requestId, bool success, const String& message);

    /**
     * @brief 模拟键盘输入来启动主机代理程序
     * @param wifiStatus 当前WiFi状态，将作为启动参数传递
     */
    void simulateKeyboardLaunchAgent(const String& wifiStatus);

private:
    LLMManager* _llmManager;        // AI管理器指针
    AppWiFiManager* _wifiManager;   // WiFi管理器指针
    USBCDC _cdc;                    // USB CDC（串口）实例
    String _inputBuffer;            // 串口数据接收缓冲区

    /**
     * @brief 处理USB串口接收到的数据
     */
    void handleUsbSerialData();

    /**
     * @brief 处理来自主机的JSON消息
     * @param message JSON格式的消息字符串
     */
    void processHostMessage(const String& message);

    /**
     * @brief 通过CDC串口向主机发送消息的通用方法
     * @param message 要发送的JSON消息
     */
    void sendToHost(const String& message);
};

#endif // USB_SHELL_MANAGER_H
