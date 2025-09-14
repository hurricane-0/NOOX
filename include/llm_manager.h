/**
 * @file llm_manager.h
 * @brief 负责管理与大型语言模型 (LLM) 的交互。
 *
 * 该文件定义了 LLMManager 类，用于处理与不同 LLM 提供商（如 DeepSeek、OpenRouter、OpenAI）的通信，
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
    DEEPSEEK,   ///< DeepSeek LLM 提供商
    OPENROUTER, ///< OpenRouter LLM 提供商
    OPENAI      ///< OpenAI LLM 提供商
    // 在此处添加其他提供商
};

/**
 * @brief 定义 LLM 的操作模式。
 */
enum LLMMode {
    CHAT_MODE,      ///< 聊天模式，用于一般对话
    ADVANCED_MODE   ///< 高级模式，可能涉及工具使用或更复杂的逻辑
};

/**
 * @brief 定义 LLM 请求结构体。
 */
struct LLMRequest {
    String prompt; ///< 用户输入的提示
    LLMMode mode;  ///< LLM 的操作模式
    String authorizedToolsJson; ///< 授权工具的 JSON 字符串
};

/**
 * @brief 定义 LLM 响应结构体。
 */
struct LLMResponse {
    String response; ///< LLM 生成的响应
};

/**
 * @brief LLMManager 类，用于管理与大型语言模型的交互。
 */
class LLMManager {
public:
    /**
     * @brief 构造函数。
     * @param config ConfigManager 的引用，用于访问配置信息。
     */
    LLMManager(ConfigManager& config);

    /**
     * @brief 初始化 LLM 管理器。
     *        加载配置并准备 LLM 交互。
     */
    void begin();

    /**
     * @brief 根据提示、模式和授权工具生成 LLM 响应。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式 (CHAT_MODE 或 ADVANCED_MODE)。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return LLM 生成的响应字符串。
     */
    String generateResponse(const String& prompt, LLMMode mode, const String& authorizedToolsJson);

    /**
     * @brief 处理用户输入，并生成 LLM 响应。
     * @param userInput 用户输入的字符串。
     * @return 包含 AI 回复和/或 Shell 命令的 JSON 字符串。
     */
    String processUserInput(const String& userInput);

    /**
     * @brief 处理 Shell 命令的输出，并生成 LLM 响应。
     * @param command 执行的 Shell 命令。
     * @param std_out 命令的标准输出。
     * @param std_err 命令的标准错误输出。
     * @return 包含 AI 回复和/或 Shell 命令的 JSON 字符串。
     */
    String processShellOutput(const String& cmd, const String& output, const String& error);

    /**
     * @brief 启动 LLM 处理任务。
     *        该任务会在后台异步处理 LLM 请求。
     */
    void startLLMTask();

    QueueHandle_t llmRequestQueue;  ///< LLM 请求队列句柄
    QueueHandle_t llmResponseQueue; ///< LLM 响应队列句柄

private:
    ConfigManager& configManager; ///< ConfigManager 的引用
    String currentProvider;       ///< 当前使用的 LLM 提供商
    String currentModel;          ///< 当前使用的模型
    String currentApiKey;         ///< 当前提供商的 API 密钥

    /**
     * @brief FreeRTOS 任务函数，用于处理 LLM 请求。
     * @param pvParameters 指向 LLMManager 实例的指针。
     */
    static void llmTask(void* pvParameters);

    /**
     * @brief 获取类 OpenAI 模型的响应 (DeepSeek, OpenRouter, OpenAI)。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return LLM 生成的响应字符串。
     */
    String getOpenAILikeResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    /**
     * @brief 根据模式和授权工具生成系统提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return 生成的系统提示字符串。
     */
    String generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools);

    /**
     * @brief 获取工具描述。
     * @param authorizedTools 授权工具的 JSON 数组。
     * @return 工具描述字符串。
     */
    String getToolDescriptions(const JsonArray& authorizedTools);
};

#endif // LLM_MANAGER_H
