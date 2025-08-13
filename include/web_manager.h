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
#include "wifi_manager.h" // 包含 WiFiManager 以用于 WebManager 构造函数
#include <ArduinoJson.h> // 包含 ArduinoJson 以支持 JsonArray
#include <freertos/queue.h> // 引入 FreeRTOS 队列

/**
 * @brief WebManager 类，用于管理设备上的 Web 服务器和 WebSocket 通信。
 *
 * 该类负责初始化 Web 服务器、设置路由、处理 WebSocket 事件，
 * 并与 LLMManager 和 TaskManager 交互以提供 Web 界面功能。
 */
class WebManager {
public:
    /**
     * @brief 构造函数。
     * @param llm 对 LLMManager 实例的引用。
     * @param task 对 TaskManager 实例的引用。
     * @param wifi 对 AppWiFiManager 实例的引用。
     */
    WebManager(LLMManager& llm, TaskManager& task, AppWiFiManager& wifi);

    /**
     * @brief 初始化 Web 管理器。
     *
     * 设置 Web 服务器路由和 WebSocket 事件处理程序。
     */
    void begin();

    /**
     * @brief Web 管理器的主循环函数。
     *
     * 处理 WebSocket 客户端的清理等周期性任务。
     */
    void loop();

    /**
     * @brief 向所有连接的 WebSocket 客户端广播消息。
     * @param message 要广播的消息字符串。
     */
    void broadcast(const String& message);

    /**
     * @brief 设置 LLM 的当前模式（聊天模式或高级模式）。
     * @param mode 要设置的 LLM 模式。
     */
    void setLLMMode(LLMMode mode); // 设置 LLM 模式的新方法

    // 新增：获取 LLM 请求队列
    QueueHandle_t getLLMRequestQueue() { return llmManager.llmRequestQueue; }
    // 新增：获取 LLM 响应队列
    QueueHandle_t getLLMResponseQueue() { return llmManager.llmResponseQueue; }

    // 处理 API 密钥设置的新方法
    /**
     * @brief 处理设置 Gemini API 密钥的 Web 请求。
     * @param request 异步 Web 服务器请求对象。
     */
    void handleSetGeminiApiKey(AsyncWebServerRequest *request);

    /**
     * @brief 处理设置 DeepSeek API 密钥的 Web 请求。
     * @param request 异步 Web 服务器请求对象。
     */
    void handleSetDeepSeekApiKey(AsyncWebServerRequest *request);

    /**
     * @brief 处理设置 ChatGPT API 密钥的 Web 请求。
     * @param request 异步 Web 服务器请求对象。
     */
    void handleSetChatGPTApiKey(AsyncWebServerRequest *request);

    /**
     * @brief 处理设置 LLM 提供商的 Web 请求。
     * @param request 异步 Web 服务器请求对象。
     */
    void handleSetLLMProvider(AsyncWebServerRequest *request);

private:
    LLMManager& llmManager;             ///< 对 LLMManager 实例的引用
    TaskManager& taskManager;           ///< 对 TaskManager 实例的引用
    AppWiFiManager& wifiManager;        ///< 对 AppWiFiManager 实例的引用
    AsyncWebServer server;              ///< 异步 Web 服务器实例
    AsyncWebSocket ws;                  ///< 异步 WebSocket 实例
    LLMMode currentLLMMode;             ///< 存储当前的 LLM 模式（聊天模式或高级模式）
    JsonDocument authorizedToolsDoc;     ///< 存储授权工具的 JsonDocument
    JsonArray authorizedToolsArray;     ///< 存储授权工具的 JsonArray

    /**
     * @brief WebSocket 事件处理回调函数。
     * @param server 触发事件的 WebSocket 服务器。
     * @param client 触发事件的 WebSocket 客户端。
     * @param type 事件类型。
     * @param arg 事件参数。
     * @param data 接收到的数据（如果适用）。
     * @param len 接收到的数据长度。
     */
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    /**
     * @brief 处理接收到的 WebSocket 数据。
     * @param client 接收到数据的 WebSocket 客户端。
     * @param arg 事件参数。
     * @param data 接收到的数据。
     * @param len 接收到的数据长度。
     */
    void handleWebSocketData(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len); // 修改后的签名

    /**
     * @brief 设置 Web 服务器的路由。
     *
     * 定义处理不同 URL 请求的函数。
     */
    void setupRoutes();

    /**
     * @brief 检查工具是否已授权。
     * @param toolName 要检查的工具名称。
     * @return 如果工具已授权则返回 true，否则返回 false。
     */
    bool isToolAuthorized(const String& toolName); // 检查工具是否授权的辅助函数
};

#endif // WEB_MANAGER_H
