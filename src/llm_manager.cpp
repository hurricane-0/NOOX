#include "llm_manager.h"
#include <HttpClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// 定义 API 端点
const char* GEMINI_API_HOST = "generativelanguage.googleapis.com";
const int GEMINI_API_PORT = 443;
const char* GEMINI_API_PATH = "/v1beta/models/gemini-pro:generateContent?key=";

const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const int DEEPSEEK_API_PORT = 443;
const char* DEEPSEEK_API_PATH = "/chat/completions";

const char* CHATGPT_API_HOST = "api.openai.com";
const int CHATGPT_API_PORT = 443;
const char* CHATGPT_API_PATH = "/v1/chat/completions";

// 构造函数
LLMManager::LLMManager() : currentProvider(DEEPSEEK) { // 初始化当前模型提供商为 DeepSeek
    // 创建 LLM 请求和响应队列
    llmRequestQueue = xQueueCreate(5, sizeof(LLMRequest)); // 队列深度为 5
    llmResponseQueue = xQueueCreate(5, sizeof(LLMResponse)); // 队列深度为 5

    if (llmRequestQueue == NULL || llmResponseQueue == NULL) {
        Serial.println("Error creating LLM queues!");
    }
}

// begin 方法用于初始化 API Key
void LLMManager::begin() {
    // 初始化 API Key，用户需通过 set 方法设置
    geminiApiKey = "";
    deepseekApiKey = "sk-1a45bc2364a243c8ab477392509645bb";
    chatGPTApiKey = "";
    Serial.println("LLM API Keys initialized. Please set them using the appropriate set functions.");
}

// 启动 LLM 处理任务
void LLMManager::startLLMTask() {
    xTaskCreatePinnedToCore(
        llmTask,            // 任务函数
        "LLMTask",          // 任务名称
        8192 * 4,           // 堆栈大小 (根据需要调整，LLM请求可能需要更多)
        this,               // 传递 LLMManager 实例指针
        5,                  // 任务优先级
        NULL,               // 任务句柄
        0                   // 运行在核心 0
    );
    Serial.println("LLM processing task started.");
}

// 设置当前使用的 LLM 提供商
void LLMManager::setProvider(LLMProvider provider) {
    currentProvider = provider;
}

// 设置 Gemini API Key
void LLMManager::setGeminiApiKey(const String& key) {
    geminiApiKey = key;
    Serial.println("Gemini API Key updated in memory.");
}

// 设置 DeepSeek API Key
void LLMManager::setDeepSeekApiKey(const String& key) {
    deepseekApiKey = key;
    Serial.println("DeepSeek API Key updated in memory.");
}

// 设置 ChatGPT API Key
void LLMManager::setChatGPTApiKey(const String& key) {
    chatGPTApiKey = key;
    Serial.println("ChatGPT API Key updated in memory.");
}

// 根据当前模型提供商生成回复
String LLMManager::generateResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    Serial.printf("Largest Free Heap Block before LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    // 检查当前模型提供商的 API Key 是否已设置
    String response;
    switch (currentProvider) {
        case GEMINI:
            if (geminiApiKey.length() == 0) response = "Error: Gemini API Key is not set.";
            else response = getGeminiResponse(prompt, mode, authorizedTools);
            break;
        case DEEPSEEK:
            if (deepseekApiKey.length() == 0) response = "Error: DeepSeek API Key is not set.";
            else response = getDeepSeekResponse(prompt, mode, authorizedTools);
            break;
        case CHATGPT:
            if (chatGPTApiKey.length() == 0) response = "Error: ChatGPT API Key is not set.";
            else response = getChatGPTResponse(prompt, mode, authorizedTools);
            break;
        default:
            response = "Error: Invalid LLM provider.";
            break;
    }
    Serial.printf("Largest Free Heap Block after LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return response;
}

// 获取 Gemini 回复
String LLMManager::getGeminiResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    // 建立到 Gemini API 的连接
    if (!client.connect(GEMINI_API_HOST, GEMINI_API_PORT)) {
        return "Error: Connection to Gemini API failed.";
    }
    
    String url = String(GEMINI_API_PATH) + geminiApiKey;
    JsonDocument doc;

    // 生成系统提示词
    String systemPrompt = generateSystemPrompt(mode, authorizedTools);
    if (systemPrompt.length() > 0) {
        doc["contents"][0]["parts"][0]["text"] = systemPrompt;
        doc["contents"][1]["parts"][0]["text"] = prompt;
    } else {
        doc["contents"][0]["parts"][0]["text"] = prompt;
    }
    
    String requestBody;
    serializeJson(doc, requestBody);

    HttpClient http(client, GEMINI_API_HOST, GEMINI_API_PORT);
    http.post(url, "application/json", requestBody);
    
    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    // 解析 Gemini API 返回内容
    if (statusCode == 200) {
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        return responseDoc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
    } else {
        return "Error: Gemini API request failed with status code " + String(statusCode) + " " + response;
    }
}

// 获取 DeepSeek 回复
String LLMManager::getDeepSeekResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    // 建立到 DeepSeek API 的连接
    if (!client.connect(DEEPSEEK_API_HOST, DEEPSEEK_API_PORT)) {
        return "Error: Connection to DeepSeek API failed.";
    }

    JsonDocument doc;
    doc["model"] = "deepseek-chat";

    // 生成系统提示词
    String systemPrompt = generateSystemPrompt(mode, authorizedTools);
    if (systemPrompt.length() > 0) {
        doc["messages"][0]["role"] = "system";
        doc["messages"][0]["content"] = systemPrompt;
        doc["messages"][1]["role"] = "user";
        doc["messages"][1]["content"] = prompt;
    } else {
        doc["messages"][0]["role"] = "user";
        doc["messages"][0]["content"] = prompt;
    }
    
    String requestBody;
    serializeJson(doc, requestBody);

    HttpClient http(client, DEEPSEEK_API_HOST, DEEPSEEK_API_PORT);
    http.beginRequest();
    http.post(DEEPSEEK_API_PATH);
    http.sendHeader("Authorization", "Bearer " + deepseekApiKey);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(requestBody.length()));
    http.write((const uint8_t*)requestBody.c_str(), requestBody.length());
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    // 解析 DeepSeek API 返回内容
    if (statusCode == 200) {
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        return responseDoc["choices"][0]["message"]["content"].as<String>();
    } else {
        return "Error: DeepSeek API request failed with status code " + String(statusCode) + " " + response;
    }
}

// 获取 ChatGPT 回复
String LLMManager::getChatGPTResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client;
    client.setInsecure();
    // 建立到 ChatGPT API 的连接
    if (!client.connect(CHATGPT_API_HOST, CHATGPT_API_PORT)) {
        return "Error: Connection to ChatGPT API failed.";
    }

    JsonDocument doc;
    doc["model"] = "gpt-3.5-turbo";

    // 生成系统提示词
    String systemPrompt = generateSystemPrompt(mode, authorizedTools);
    if (systemPrompt.length() > 0) {
        doc["messages"][0]["role"] = "system";
        doc["messages"][0]["content"] = systemPrompt;
        doc["messages"][1]["role"] = "user";
        doc["messages"][1]["content"] = prompt;
    } else {
        doc["messages"][0]["role"] = "user";
        doc["messages"][0]["content"] = prompt;
    }
    
    String requestBody;
    serializeJson(doc, requestBody);

    HttpClient http(client, CHATGPT_API_HOST, CHATGPT_API_PORT);
    http.beginRequest();
    http.post(CHATGPT_API_PATH);
    http.sendHeader("Authorization", "Bearer " + chatGPTApiKey);
    http.sendHeader("Content-Type", "application/json");
    http.sendHeader("Content-Length", String(requestBody.length()));
    http.write((const uint8_t*)requestBody.c_str(), requestBody.length());
    http.endRequest();

    int statusCode = http.responseStatusCode();
    String response = http.responseBody();

    // 解析 ChatGPT API 返回内容
    if (statusCode == 200) {
        JsonDocument responseDoc;
        deserializeJson(responseDoc, response);
        return responseDoc["choices"][0]["message"]["content"].as<String>();
    } else {
        return "Error: ChatGPT API request failed with status code " + String(statusCode) + " " + response;
    }
}

// 生成系统提示词，根据模式和授权工具
String LLMManager::generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools) {
    String prompt = "";
    if (mode == CHAT_MODE) {
        // 普通聊天模式提示词
        prompt += "You are a helpful and friendly AI assistant. Respond concisely and accurately to user queries.";
    } else if (mode == ADVANCED_MODE) {
        // 高级模式提示词
        prompt += "You are an intelligent assistant capable of controlling a hardware device. ";
        prompt += "In advanced mode, you can call specific tools to interact with the device's peripherals or execute automation scripts. ";
        prompt += "Your response MUST be a JSON object with a 'mode' field set to 'advanced' and an 'action' field. ";
        prompt += "The 'action' field should contain 'type' (always 'tool_call'), 'tool_name', and 'parameters'. ";
        prompt += "If you cannot fulfill the request with available tools, respond with a text message. ";
        prompt += "Here are the tools you are authorized to use:\n";
        prompt += getToolDescriptions(authorizedTools);
        prompt += "\nExample of a tool call:\n";
        prompt += "```json\n";
        prompt += "{\n";
        prompt += "  \"mode\": \"advanced\",\n";
        prompt += "  \"action\": {\n";
        prompt += "    \"type\": \"tool_call\",\n";
        prompt += "    \"tool_name\": \"usb_hid_keyboard_type\",\n";
        prompt += "    \"parameters\": {\n";
        prompt += "      \"text\": \"Hello World\"\n";
        prompt += "    }\n";
        prompt += "  }\n";
        prompt += "}\n";
        prompt += "```\n";
        // 已移除自动化脚本示例
    }
    return prompt;
}

// 获取工具描述信息
String LLMManager::getToolDescriptions(const JsonArray& authorizedTools) {
    String descriptions = "";
    // 已知工具的描述定义。应与 TaskManager 的能力保持同步。
    // 高级模式下所有工具都可用，因此不再进行授权检查。

    // 示例工具描述（应全面且准确反映 LLM 能力）
    // HID 工具
    descriptions += "- **usb_hid_keyboard_type**: Types a given string on the connected computer via USB HID. Parameters: `{\"text\": \"string_to_type\"}`\n";
    descriptions += "- **usb_hid_mouse_click**: Clicks the mouse at the current cursor position. Parameters: `{\"button\": \"left\"}` (or \"right\", \"middle\")\n";
    descriptions += "- **usb_hid_mouse_move**: Moves the mouse cursor by a specified offset. Parameters: `{\"x\": 10, \"y\": 20}`\n";
    // GPIO
    descriptions += "- **gpio_set_level**: Sets the digital level of a specified GPIO pin. Parameters: `{\"pin\": 1, \"level\": 1}` (0 for LOW, 1 for HIGH)\n";

    if (descriptions.length() == 0) {
        descriptions = "No specific tools are available for use in this session, or no descriptions are available for the authorized tools.";
    }

    return descriptions;
}

// LLM 任务函数
void LLMManager::llmTask(void* pvParameters) {
    LLMManager* llmManager = static_cast<LLMManager*>(pvParameters);
    LLMRequest request;
    LLMResponse response;

    for (;;) {
        // 等待 LLM 请求
        if (xQueueReceive(llmManager->llmRequestQueue, &request, portMAX_DELAY) == pdPASS) {
            Serial.printf("LLMTask: Received request for prompt: %s\n", request.prompt.c_str());
            // 调用实际的 LLM 响应生成函数
            // 注意：这里 authorizedTools 暂时为空 JsonArray，因为队列中不方便传递复杂对象
            // 如果需要，可以考虑将 authorizedTools 序列化为字符串在请求中传递
            response.response = llmManager->generateResponse(request.prompt, request.mode, JsonArray());
            Serial.printf("LLMTask: Generated response: %s\n", response.response.c_str());

            // 将响应发送回 WebManager
            if (xQueueSend(llmManager->llmResponseQueue, &response, portMAX_DELAY) != pdPASS) {
                Serial.println("LLMTask: Failed to send response to queue.");
            }
        }
    }
}
