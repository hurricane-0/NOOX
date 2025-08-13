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
    // JsonArray authorizedTools; // 暂时不传递，因为JsonArray不能直接在队列中传递，需要序列化
    // 考虑将 authorizedTools 序列化为 String 或在 LLMManager 内部管理
};

// 定义 LLM 响应结构体
struct LLMResponse {
    String response;
    // 可以添加一个标识符来关联请求，如果需要的话
};

/**
 * @brief LLMManager 类，用于管理与大型语言模型的交互。
 *
 * 该类封装了与不同 LLM 提供商通信的逻辑，包括 API 密钥管理、
 * 响应生成以及系统提示的构建。
 */
class LLMManager {
public:
    /**
     * @brief 构造函数。
     */
    LLMManager(); // 修改后的构造函数

    /**
     * @brief 初始化 LLM 管理器。
     *
     * 执行任何必要的设置或初始化任务。
     */
    void begin();

    /**
     * @brief 设置当前使用的 LLM 提供商。
     * @param provider 要设置的 LLM 提供商。
     */
    void setProvider(LLMProvider provider);

    /**
     * @brief 设置 Gemini LLM 的 API 密钥。
     * @param key Gemini API 密钥。
     */
    void setGeminiApiKey(const String& key);

    /**
     * @brief 设置 DeepSeek LLM 的 API 密钥。
     * @param key DeepSeek API 密钥。
     */
    void setDeepSeekApiKey(const String& key);

    /**
     * @brief 设置 ChatGPT LLM 的 API 密钥。
     * @param key ChatGPT API 密钥。
     */
    void setChatGPTApiKey(const String& key);

    /**
     * @brief 根据提示、模式和授权工具生成 LLM 响应。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式（聊天模式或高级模式）。
     * @param authorizedTools 授权使用的工具列表（JsonArray 格式）。
     * @return LLM 生成的响应字符串。
     */
    String generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    // 新增：启动 LLM 处理任务
    void startLLMTask();

    // 新增：LLM 请求和响应队列
    QueueHandle_t llmRequestQueue;
    QueueHandle_t llmResponseQueue;

private:
    LLMProvider currentProvider;    ///< 当前使用的 LLM 提供商
    String geminiApiKey;            ///< Gemini LLM 的 API 密钥
    String deepseekApiKey;          ///< DeepSeek LLM 的 API 密钥
    String chatGPTApiKey;           ///< ChatGPT LLM 的 API 密钥

    // 新增：LLM 任务函数
    static void llmTask(void* pvParameters);

    /**
     * @brief 从 Gemini LLM 获取响应。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权使用的工具列表。
     * @return Gemini LLM 生成的响应字符串。
     */
    String getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    /**
     * @brief 从 DeepSeek LLM 获取响应。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权使用的工具列表。
     * @return DeepSeek LLM 生成的响应字符串。
     */
    String getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    /**
     * @brief 从 ChatGPT LLM 获取响应。
     * @param prompt 用户输入的提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权使用的工具列表。
     * @return ChatGPT LLM 生成的响应字符串。
     */
    String getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools);

    /**
     * @brief 根据模式和授权工具生成系统提示。
     * @param mode LLM 的操作模式。
     * @param authorizedTools 授权使用的工具列表。
     * @return 生成的系统提示字符串。
     */
    String generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools);

    /**
     * @brief 获取授权工具的描述字符串。
     * @param authorizedTools 授权使用的工具列表。
     * @return 工具描述的字符串。
     */
    String getToolDescriptions(const JsonArray& authorizedTools);
};

#endif // LLM_MANAGER_H
