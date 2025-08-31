/**
 * @file web_manager.h
 * @brief 负责管理设备上的 Web 服务器和 WebSocket 通信。
 *
 * 该文件定义了 WebManager 类，用于设置和管理异步 Web 服务器，
 * 处理客户端请求，并通过 WebSocket 实现双向通信。
 */
#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <ESPAsyncWebServer.h>
#include "llm_manager.h"
#include "task_manager.h"
#include "wifi_manager.h"
#include "config_manager.h"
#include <ArduinoJson.h>
#include <freertos/queue.h>

/**
 * @brief WebManager 类，用于管理设备上的 Web 服务器和 WebSocket 通信。
 */
class WebManager {
public:
    /**
     * @brief 构造函数。
     */
    WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi, ConfigManager& config);

    /**
     * @brief 初始化 Web 管理器。
     */
    void begin();

    /**
     * @brief Web 管理器的主循环函数。
     */
    void loop();

    /**
     * @brief 向所有连接的 WebSocket 客户端广播消息。
     */
    void broadcast(const String& message);

    /**
     * @brief 设置 LLM 的当前模式（聊天模式或高级模式）。
     */
    void setLLMMode(LLMMode mode);

private:
    LLMManager& llmManager;
    TaskManager& taskManager;
    AppWiFiManager& wifiManager;
    ConfigManager& configManager;
    AsyncWebServer server;
    AsyncWebSocket ws;
    LLMMode currentLLMMode;

    /**
     * @brief WebSocket 事件处理回调函数。
     */
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    /**
     * @brief 处理接收到的 WebSocket 数据。
     */
    void handleWebSocketData(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len);

    /**
     * @brief 设置 Web 服务器的路由。
     */
    void setupRoutes();
};

#endif // WEB_MANAGER_H
