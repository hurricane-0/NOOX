#include "llm_manager.h"
#include <HttpClient.h>
#include <WiFiClientSecure.h>

// Define the size of the JSON document buffer for LLM responses
const size_t JSON_DOC_SIZE = 2048; // Increased size for potentially larger LLM responses

// 定义 API 端点 - 某些提供商可能共享主机，可以进行整合
const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const char* OPENROUTER_API_HOST = "openrouter.ai";
const char* OPENAI_API_HOST = "api.openai.com"; // 明确定义 OpenAI 主机

// 构造函数
LLMManager::LLMManager(ConfigManager& config) : configManager(config) {
    // 创建 LLM 请求队列，最多可存储 5 个请求
    llmRequestQueue = xQueueCreate(5, sizeof(LLMRequest));
    // 创建 LLM 响应队列，最多可存储 5 个响应
    llmResponseQueue = xQueueCreate(5, sizeof(LLMResponse));

    // 检查队列是否成功创建
    if (llmRequestQueue == NULL || llmResponseQueue == NULL) {
        Serial.println("Error creating LLM queues!"); // 打印错误信息
    }
}

// begin 方法用于加载配置
void LLMManager::begin() {
    configManager.loadConfig(); // Load config into the internal JsonDocument
    JsonDocument& config = configManager.getConfig(); // Get reference to the loaded config
    // 从配置中获取当前使用的 LLM 提供商
    currentProvider = config["last_used"]["llm_provider"].as<String>();
    // 从配置中获取当前使用的模型
    currentModel = config["last_used"]["model"].as<String>();
    // 从配置中获取当前提供商的 API 密钥
    currentApiKey = config["llm_providers"][currentProvider]["api_key"].as<String>();

    // 打印 LLMManager 初始化信息
    Serial.printf("LLMManager initialized. Provider: %s, Model: %s\n", currentProvider.c_str(), currentModel.c_str());
}

// 启动 LLM 处理任务
void LLMManager::startLLMTask() {
    // 创建一个 FreeRTOS 任务，将其固定在核心 0 上
    xTaskCreatePinnedToCore(
        llmTask, "LLMTask", 8192 * 2, this, 5, NULL, 0);
    Serial.println("LLM processing task started."); // 打印任务启动信息
}

// 根据当前提供商生成响应
String LLMManager::generateResponse(const String& prompt, LLMMode mode, const String& authorizedToolsJson) {
    // 打印 LLM 调用前最大的空闲堆内存块大小
    Serial.printf("Largest Free Heap Block before LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    
    // 检查 API 密钥是否已设置
    if (currentApiKey.length() == 0) {
        return "Error: API Key is not set for the current provider."; // 返回错误信息
    }

    // Deserialize authorizedToolsJson back to JsonArray
    JsonDocument toolsDoc;
    DeserializationError toolsError = deserializeJson(toolsDoc, authorizedToolsJson);
    if (toolsError) {
        Serial.printf("Failed to deserialize authorizedToolsJson: %s\n", toolsError.c_str());
        // Handle error, perhaps return an empty JsonArray or an error message
        return "Error: Failed to parse authorized tools.";
    }
    JsonArray authorizedTools = toolsDoc.as<JsonArray>();

    String response;
    // 如果当前提供商是 DeepSeek、OpenRouter 或 OpenAI，则调用 getOpenAILikeResponse
    if (currentProvider == "deepseek" || currentProvider == "openrouter" || currentProvider == "openai") {
        response = getOpenAILikeResponse(prompt, mode, authorizedTools);
    } else {
        response = "Error: Invalid LLM provider selected."; // 返回错误信息
    }

    // 打印 LLM 调用后最大的空闲堆内存块大小
    Serial.printf("Largest Free Heap Block after LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return response;
}

String LLMManager::processUserInput(const String& userInput) {
    String prompt = "User input: " + userInput;

    JsonDocument emptyToolsDoc;
    JsonArray authorizedTools = emptyToolsDoc.to<JsonArray>();
    String authorizedToolsJson;
    serializeJson(authorizedTools, authorizedToolsJson);

    LLMRequest request;
    request.prompt = prompt;
    request.mode = ADVANCED_MODE;
    request.authorizedToolsJson = authorizedToolsJson;

    if (xQueueSend(llmRequestQueue, &request, portMAX_DELAY) != pdPASS) {
        Serial.println("processUserInput: Failed to send request to queue.");
        return "Error: Failed to send request to LLM task.";
    }

    LLMResponse response;
    if (xQueueReceive(llmResponseQueue, &response, portMAX_DELAY) != pdPASS) {
        Serial.println("processUserInput: Failed to receive response from queue.");
        return "Error: Failed to receive response from LLM task.";
    }
    String llmRawResponse = response.response;

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, llmRawResponse);

    JsonDocument hostResponseDoc;
    String formattedResponse;

    // If the LLM response is not valid JSON, treat it as a simple AI response.
    if (error) {
        hostResponseDoc["type"] = "aiResponse";
        hostResponseDoc["payload"] = llmRawResponse;
        ArduinoJson::serializeJson(hostResponseDoc, formattedResponse);
        return formattedResponse;
    }

    // Check if the response is a tool call for executing a shell command.
    if (responseDoc["tool_calls"].is<JsonArray>()) {
        JsonArray toolCalls = responseDoc["tool_calls"].as<JsonArray>();
        if (toolCalls.size() > 0 && toolCalls[0]["name"].is<String>() && toolCalls[0]["name"].as<String>() == "execute_shell_command") {
            hostResponseDoc["type"] = "shellCommand";
            hostResponseDoc["payload"] = toolCalls[0]["args"]["command"].as<String>();
        } else {
            // Otherwise, treat it as a standard AI response.
            hostResponseDoc["type"] = "aiResponse";
            hostResponseDoc["payload"] = llmRawResponse;
        }
    } else {
        // Otherwise, treat it as a standard AI response.
        hostResponseDoc["type"] = "aiResponse";
        hostResponseDoc["payload"] = llmRawResponse;
    }

    serializeJson(hostResponseDoc, formattedResponse);
    return formattedResponse;
}

String LLMManager::processShellOutput(const String& cmd, const String& output, const String& err) {
    String prompt = "Previous shell command: " + cmd + "\n";
    prompt += "STDOUT: " + output + "\n";
    prompt += "STDERR: " + err + "\n";
    prompt += "Based on the above shell output, what should be the next action or response?";

    JsonDocument emptyToolsDoc;
    JsonArray authorizedTools = emptyToolsDoc.to<JsonArray>();
    String authorizedToolsJson;
    serializeJson(authorizedTools, authorizedToolsJson);

    LLMRequest request;
    request.prompt = prompt;
    request.mode = ADVANCED_MODE;
    request.authorizedToolsJson = authorizedToolsJson;

    if (xQueueSend(llmRequestQueue, &request, portMAX_DELAY) != pdPASS) {
        Serial.println("processShellOutput: Failed to send request to queue.");
        return "Error: Failed to send request to LLM task.";
    }

    LLMResponse response;
    if (xQueueReceive(llmResponseQueue, &response, portMAX_DELAY) != pdPASS) {
        Serial.println("processShellOutput: Failed to receive response from queue.");
        return "Error: Failed to receive response from LLM task.";
    }
    String llmRawResponse = response.response;

    JsonDocument responseDoc;
    DeserializationError error = deserializeJson(responseDoc, llmRawResponse);

    JsonDocument hostResponseDoc;
    String formattedResponse;

    // If the LLM response is not valid JSON, treat it as a simple AI response.
    if (error) {
        hostResponseDoc["type"] = "aiResponse";
        hostResponseDoc["payload"] = llmRawResponse;
        serializeJson(hostResponseDoc, formattedResponse);
        return formattedResponse;
    }

    // Check if the response is a tool call for executing a shell command.
    if (responseDoc["tool_calls"].is<JsonArray>()) {
        JsonArray toolCalls = responseDoc["tool_calls"].as<JsonArray>();
        if (toolCalls.size() > 0 && toolCalls[0]["name"].is<String>() && toolCalls[0]["name"].as<String>() == "execute_shell_command") {
            hostResponseDoc["type"] = "shellCommand";
            hostResponseDoc["payload"] = toolCalls[0]["args"]["command"].as<String>();
        } else {
            // Otherwise, treat it as a standard AI response.
            hostResponseDoc["type"] = "aiResponse";
            hostResponseDoc["payload"] = llmRawResponse;
        }
    } else {
        // Otherwise, treat it as a standard AI response.
        hostResponseDoc["type"] = "aiResponse";
        hostResponseDoc["payload"] = llmRawResponse;
    }

    serializeJson(hostResponseDoc, formattedResponse);
    return formattedResponse;
}


// 获取类 OpenAI 响应 (DeepSeek, OpenRouter, OpenAI)
String LLMManager::getOpenAILikeResponse(const String& prompt, LLMMode mode, const JsonArray& authorizedTools) {
    WiFiClientSecure client; // 创建安全 WiFi 客户端
    client.setInsecure(); // 允许不安全的连接
    
    const char* apiHost; // API 主机
    String apiPath; // API 路径

    // 根据当前提供商设置 API 主机和路径
    if (currentProvider == "openrouter") {
        apiHost = OPENROUTER_API_HOST;
        apiPath = "/api/v1/chat/completions";
    } else if (currentProvider == "deepseek") {
        apiHost = DEEPSEEK_API_HOST;
        apiPath = "/chat/completions"; // DeepSeek 使用 /chat/completions
    } else if (currentProvider == "openai") {
        apiHost = OPENAI_API_HOST;
        apiPath = "/v1/chat/completions";
    } else {
        return "Error: Invalid OpenAI-like provider selected."; // 返回错误信息
    }

    // 连接到 API 主机
    if (!client.connect(apiHost, 443)) {
        return "Error: Connection to API failed."; // 返回连接失败错误
    }

    JsonDocument doc; // 创建 JSON 文档
    doc["model"] = currentModel; // 设置模型

    String systemPrompt = generateSystemPrompt(mode, authorizedTools); // 生成系统提示
    // 根据系统提示的长度构建消息数组
    if (systemPrompt.length() > 0) {
        doc["messages"].add<JsonObject>(); // Add a new object for system message
        doc["messages"][0]["role"] = "system";
        doc["messages"][0]["content"] = systemPrompt;
        doc["messages"].add<JsonObject>(); // Add a new object for user message
        doc["messages"][1]["role"] = "user";
        doc["messages"][1]["content"] = prompt;
    } else {
        doc["messages"].add<JsonObject>(); // Add a new object for user message
        doc["messages"][0]["role"] = "user";
        doc["messages"][0]["content"] = prompt;
    }
    
    String requestBody; // 请求体字符串
    serializeJson(doc, requestBody); // 将 JSON 文档序列化为字符串

    HttpClient http(client, apiHost, 443); // 创建 HTTP 客户端
    http.beginRequest(); // 开始 HTTP 请求
    http.post(apiPath); // 发送 POST 请求到指定路径
    http.sendHeader("Authorization", "Bearer " + currentApiKey); // 发送授权头
    http.sendHeader("Content-Type", "application/json"); // 发送内容类型头
    http.sendHeader("Content-Length", String(requestBody.length())); // 发送内容长度头
    // 如果是 OpenRouter，需要额外的 HTTP-Referer 头
    if (currentProvider == "openrouter") {
        http.sendHeader("HTTP-Referer", "http://localhost"); // 替换为您的实际站点 URL
    }
    http.write((const uint8_t*)requestBody.c_str(), requestBody.length()); // 写入请求体
    http.endRequest(); // 结束请求

    int statusCode = http.responseStatusCode(); // 获取响应状态码
    String response = http.responseBody(); // 获取响应体

    // 处理响应
    if (statusCode == 200) {
        JsonDocument responseDoc; // 创建响应 JSON 文档
        deserializeJson(responseDoc, response); // 反序列化响应体
        return responseDoc["choices"][0]["message"]["content"].as<String>(); // 返回 LLM 生成的内容
    } else {
        return "Error: API request failed with status code " + String(statusCode) + " " + response; // 返回错误信息
    }
}


// 根据模式和工具生成系统提示
String LLMManager::generateSystemPrompt(LLMMode mode, const JsonArray& authorizedTools) {
    String prompt = "";
    // 聊天模式下的系统提示
    if (mode == CHAT_MODE) {
        prompt += "You are a helpful and friendly AI assistant. Respond concisely and accurately to user queries.";
    } 
    // 高级模式下的系统提示 (涉及工具使用)
    else if (mode == ADVANCED_MODE) {
        prompt += "You are an intelligent assistant that helps users by executing shell commands on their computer. ";
        prompt += "Based on the user's request and the output of previous commands, decide the next step. ";
        prompt += "You can either execute another shell command or provide a natural language response. ";
        prompt += "Your response MUST be a JSON object representing a tool call. ";
        prompt += "If you need to run a command, use the 'execute_shell_command' tool. ";
        prompt += "If you need to respond to the user, wrap your text in a standard string. ";
        prompt += "Here are the tools you are authorized to use:\n";
        prompt += getToolDescriptions(authorizedTools); // 获取工具描述
        prompt += "\nExample of executing a shell command:\n";
        prompt += "```json\n";
        prompt += "{\n";
        prompt += "  \"tool_calls\": [\n";
        prompt += "    {\n";
        prompt += "      \"name\": \"execute_shell_command\",\n";
        prompt += "      \"args\": {\n";
        prompt += "        \"command\": \"ls -l\"\n";
        prompt += "      }\n";
        prompt += "    }\n";
        prompt += "  ]\n";
        prompt += "}\n";
        prompt += "```\n";
    }
    return prompt;
}

// 获取工具描述
String LLMManager::getToolDescriptions(const JsonArray& authorizedTools) {
    String descriptions = "";
    // 添加 Shell 执行工具的描述
    descriptions += "- **execute_shell_command**: Executes a command on the host computer's shell and returns the output. Parameters: `{\"command\": \"command_to_execute\"}`\n";
    
    // 保留其他工具的描述
    descriptions += "- **usb_hid_keyboard_type**: Types a given string on the connected computer via USB HID. Parameters: `{\"text\": \"string_to_type\"}`\n";
    descriptions += "- **usb_hid_mouse_click**: Clicks the mouse at the current cursor position. Parameters: `{\"button\": \"left\"}` (or \"right\", \"middle\")\n";
    descriptions += "- **usb_hid_mouse_move**: Moves the mouse cursor by a specified offset. Parameters: `{\"x\": 10, \"y\": 20}`\n";
    descriptions += "- **gpio_set_level**: Sets the digital level of a specified GPIO pin. Parameters: `{\"pin\": 1, \"level\": 1}` (0 for LOW, 1 for HIGH)\n";

    // 如果没有可用的工具，则返回默认消息
    if (descriptions.length() == 0) {
        descriptions = "No specific tools are available for use in this session.";
    }
    return descriptions;
}

// LLM 任务函数
void LLMManager::llmTask(void* pvParameters) {
    LLMManager* llmManager = static_cast<LLMManager*>(pvParameters); // 将参数转换为 LLMManager 指针
    LLMRequest request; // LLM 请求结构体
    LLMResponse response; // LLM 响应结构体

    for (;;) { // 无限循环
        // 从请求队列接收请求，如果队列为空则阻塞
        if (xQueueReceive(llmManager->llmRequestQueue, &request, portMAX_DELAY) == pdPASS) {
            Serial.printf("LLMTask: Received request for prompt: %s\n", request.prompt.c_str()); // 打印接收到的提示
            // 生成响应
            response.response = llmManager->generateResponse(request.prompt, request.mode, request.authorizedToolsJson);
            Serial.printf("LLMTask: Generated response: %s\n", response.response.c_str()); // 打印生成的响应

            // 将响应发送到响应队列
            if (xQueueSend(llmManager->llmResponseQueue, &response, portMAX_DELAY) != pdPASS) {
                Serial.println("LLMTask: Failed to send response to queue."); // 打印发送失败信息
            }
        }
    }
}
