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
 */
struct LLMRequest {
    String requestId;           ///< 请求ID，用于关联响应
    String prompt;              ///< 用户输入的提示或上下文
    LLMMode mode;               ///< LLM 的操作模式
};

/**
 * @brief 定义从 LLM 任务队列接收的响应结构体。
 */
struct LLMResponse {
    String requestId;           ///< 请求ID，用于关联响应
    String response; ///< LLM 生成的原始响应字符串
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
     * @brief 启动 LLM 后台处理任务。
     *        创建一个 FreeRTOS 任务来异步处理 LLM 请求队列。
     */
    void startLLMTask();

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
     * @brief FreeRTOS 任务函数，在后台运行。
     *        它不断地从 llmRequestQueue 中等待并获取请求，
     *        调用 generateResponse 处理请求，然后将结果放入 llmResponseQueue。
     * @param pvParameters 指向 LLMManager 实例的指针。
     */
    static void llmTask(void* pvParameters);

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
     * @brief 获取工具的描述字符串。
     *        将可用的工具列表格式化成一段文本，供系统提示使用。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return 工具描述字符串。
     */
    String getToolDescriptions();

    /**
     * @brief 处理 LLM 的原始响应，解析工具调用或自然语言回复。
     * @param requestId 请求ID。
     * @param llmRawResponse LLM 返回的原始 JSON 字符串。
     */
    void handleLLMRawResponse(const String& requestId, const String& llmRawResponse);
};

#endif // LLM_MANAGER_H
