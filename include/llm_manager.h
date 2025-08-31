/**
 * @file llm_manager.h
 * @brief 负责管理与大型语言模型 (LLM) 的交互。
 *
 * 该文件定义了 LLMManager 类，用于处理与不同 LLM 提供商（如 Gemini、DeepSeek、ChatGPT）的通信，
 * 并根据用户提示和授权工具生成响应。
 */
#ifndef LLM_MANAGER_H
#define LLM_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h> // 包含 ArduinoJson 以支持结构化响应
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config_manager.h"

/**
 * @brief 定义支持的 LLM 提供商。
 */
enum LLMProvider {
    GEMINI,     ///< Gemini LLM 提供商
    DEEPSEEK,   ///< DeepSeek LLM 提供商
    CHATGPT     ///< ChatGPT LLM 提供商
    // 在此处添加其他提供商
};

/**
 * @brief 定义 LLM 的操作模式。
 */
enum LLMMode {
    CHAT_MODE,      ///< 聊天模式，用于一般对话
    ADVANCED_MODE   ///< 高级模式，可能涉及工具使用或更复杂的逻辑
};

// 定义 LLM 请求结构体
struct LLMRequest {
    String prompt;
    LLMMode mode;
};

// 定义 LLM 响应结构体
struct LLMResponse {
    String response;
};

/**
 * @brief LLMManager 类，用于管理与大型语言模型的交互。
 */
class LLMManager {
public:
    /**
     * @brief 构造函数。
     */
    LLMManager(ConfigManager& config);

    /**
     * @brief 初始化 LLM 管理器。
     */
    void begin();

    /**
     * @brief 根据提示、模式和授权工具生成 LLM 响应。
     */
    String generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    void startLLMTask();

    QueueHandle_t llmRequestQueue;
    QueueHandle_t llmResponseQueue;

private:
    ConfigManager& configManager;
    String currentProvider;
    String currentModel;
    String currentApiKey;

    static void llmTask(void* pvParameters);

    String getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);
    String generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools);
    String getToolDescriptions(const JsonArray& authorizedTools);
};

#endif // LLM_MANAGER_H
