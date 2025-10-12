/**
 * @file llm_manager.h
 * @brief 负责管理与大型语言模型 (LLM) 的交互。
 *
 * 该文件定义了 LLMManager 类，用于处理与不同 LLM 提供商（如 DeepSeek、OpenRouter、OpenAI）的通信，
 * 并根据用户提示和授权工具生成响应。它使用 FreeRTOS 队列和任务来实现异步处理，
 * 避免在等待网络响应时阻塞主程序流程。
 */
#ifndef LLM_MANAGER_H
#define LLM_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config_manager.h"
#include "wifi_manager.h" // Include AppWiFiManager header

// Forward declaration for UsbShellManager
class UsbShellManager;

/**
 * @brief 定义支持的 LLM 提供商。
 */
enum LLMProvider {
    DEEPSEEK,   ///< DeepSeek LLM 提供商
    OPENROUTER, ///< OpenRouter LLM 提供商
    OPENAI      ///< OpenAI LLM 提供商
};

/**
 * @brief 定义 LLM 的操作模式。
 */
enum LLMMode {
    CHAT_MODE,      ///< 聊天模式，用于一般对话
    ADVANCED_MODE   ///< 高级模式，用于需要工具调用的复杂任务，如Shell通信
};

/**
 * @brief 定义发送到 LLM 任务队列的请求结构体。
 * 使用固定大小char数组和PSRAM指针以避免String浅拷贝导致的堆损坏。
 */
struct LLMRequest {
    char requestId[64];         ///< 请求ID，用于关联响应（固定大小）
    char* prompt;               ///< 用户输入的提示或上下文（PSRAM指针，接收方需释放）
    LLMMode mode;               ///< LLM 的操作模式
};

/**
 * @brief 定义从 LLM 任务队列接收的响应结构体。
 * 使用固定大小char数组和PSRAM指针以避免String浅拷贝导致的堆损坏。
 */
struct LLMResponse {
    char requestId[64];         ///< 请求ID，用于关联响应（固定大小）
    bool isToolCall;            ///< 指示响应是否为工具调用
    char toolName[32];          ///< 如果是工具调用，则为工具名称（固定大小）
    char* toolArgs;             ///< 如果是工具调用，则为工具参数的JSON字符串（PSRAM指针，接收方需释放）
    char* naturalLanguageResponse; ///< 如果是自然语言回复，则为回复内容（PSRAM指针，接收方需释放）
};

/**
 * @brief LLMManager 类，用于管理与大型语言模型的交互。
 *
 * 此类封装了与 LLM API 通信的所有逻辑，包括构建请求、发送HTTP请求、
 * 解析响应以及通过 FreeRTOS 任务进行异步处理。
 */
class LLMManager {
public:
    /**
     * @brief 构造函数。
     * @param config 对 ConfigManager 的引用，用于访问和管理配置信息。
     * @param wifiManager 对 AppWiFiManager 的引用，用于检查网络状态。
     * @param usbShellManager 对 UsbShellManager 的指针，用于发送消息到主机。
     */
    LLMManager(ConfigManager& config, AppWiFiManager& wifiManager, UsbShellManager* usbShellManager);

    /**
     * @brief 初始化 LLM 管理器。
     *        从配置文件中加载 LLM 提供商、模型和 API 密钥。
     */
    void begin();

    /**
     * @brief LLM aysnc loop.
     *        It constantly waits for and retrieves requests from llmRequestQueue,
     *        calls generateResponse to process the request, and then puts the result into llmResponseQueue.
     */
    void loop();

    /**
     * @brief 处理来自主机的用户输入。
     *        此方法将用户输入打包成一个 LLMRequest，发送到请求队列。
     * @param requestId 请求ID。
     * @param userInput 用户输入的字符串。
     */
    void processUserInput(const String& requestId, const String& userInput);

    /**
     * @brief 处理来自主机的 Shell 命令执行结果。
     *        此方法将命令的输出作为上下文，打包成一个新的 LLMRequest 发送到队列。
     * @param requestId 请求ID。
     * @param cmd 已执行的 Shell 命令。
     * @param output 命令的标准输出。
     * @param error 命令的标准错误。
     * @param status 命令执行状态（"success"或"error"）。
     * @param exitCode 命令的退出码。
     */
    void processShellOutput(const String& requestId, const String& cmd, const String& output, const String& error, const String& status, int exitCode);

    String getCurrentModelName(); // Added to get current LLM model name

    QueueHandle_t llmRequestQueue;  ///< LLM 请求队列的句柄。
    QueueHandle_t llmResponseQueue; ///< LLM 响应队列的句柄。


private:
    ConfigManager& configManager; ///< ConfigManager 的引用。
    AppWiFiManager& wifiManager;   ///< AppWiFiManager 的引用。
    UsbShellManager* _usbShellManager; ///< UsbShellManager 的指针。
    String currentProvider;       ///< 当前使用的 LLM 提供商名称。
    String currentModel;          ///< 当前使用的模型名称。
    String currentApiKey;         ///< 当前提供商的 API 密钥。


    /**
     * @brief 根据提示、模式和授权工具生成 LLM 响应。
     *        这是实际执行与 LLM API 通信的核心函数。
     * @param requestId 请求ID。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式。
     * @return LLM API 返回的原始响应字符串。
     */
    String generateResponse(const String& requestId, const String& prompt, LLMMode mode);

    /**
     * @brief 获取类 OpenAI 模型的响应 (适用于 DeepSeek, OpenRouter, OpenAI)。
     *        负责构建 HTTP 请求并与 API 端点通信。
     * @param requestId 请求ID。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return LLM API 返回的原始响应字符串。
     */
    String getOpenAILikeResponse(const String& requestId, const String& prompt, LLMMode mode);

    /**
     * @brief 根据模式和授权工具生成系统提示 (System Prompt)。
     *        系统提示用于指导 LLM 的行为和响应格式。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return 生成的系统提示字符串。
     */
    String generateSystemPrompt(LLMMode mode);

    /**
     * @brief 处理 LLM 的原始响应，解析工具调用或自然语言回复。
     * @param requestId 请求ID。
     * @param llmContentString LLM 返回的原始 JSON 字符串。
     */
    void handleLLMRawResponse(const String& requestId, const String& llmContentString);

    /**
     * @brief 安全地分配PSRAM内存并拷贝字符串。
     * @param str 要拷贝的String对象。
     * @return 分配的内存指针，失败返回nullptr。
     */
    char* allocateAndCopy(const String& str);

    /**
     * @brief 创建并发送LLM请求到队列的通用方法。
     * @param requestId 请求ID。
     * @param prompt 提示内容。
     * @param mode LLM模式。
     * @return 成功返回true，失败返回false。
     */
    bool createAndSendRequest(const String& requestId, const String& prompt, LLMMode mode);

    /**
     * @brief 分配响应字符串内存的辅助方法。
     * @param dest 目标指针引用。
     * @param src 源字符串。
     */
    void allocateResponseString(char*& dest, const String& src);
};

#endif // LLM_MANAGER_H
