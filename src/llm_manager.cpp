#include "llm_manager.h"
#include "usb_shell_manager.h" // Include the full header for UsbShellManager
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// 为LLM响应定义JSON文档缓冲区的大小
const size_t JSON_DOC_SIZE = 262144; // 为可能更大的LLM响应增加了大小，并利用PSRAM
const size_t MAX_RESPONSE_LENGTH = JSON_DOC_SIZE; // Maximum expected response length

// 用于配置网络超时的常量
const unsigned long NETWORK_TIMEOUT = 40000;  // 40秒（网络请求总超时）
const unsigned long STREAM_TIMEOUT = 40000;   // 流读取超时40秒（给LLM生成留足时间）
const unsigned long DATA_TIMEOUT = 2000;      // 数据接收间隔超时2秒

// 定义 API 端点
const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const char* OPENROUTER_API_HOST = "openrouter.ai";
const char* OPENAI_API_HOST = "api.openai.com";

// 构造函数
LLMManager::LLMManager(ConfigManager& config, AppWiFiManager& wifi, UsbShellManager* usbShellManager)
    : configManager(config), wifiManager(wifi), _usbShellManager(usbShellManager) {
    // 创建 LLM 请求队列，用于接收来自其他模块的请求（优化：减少队列深度为3）
    llmRequestQueue = xQueueCreate(3, sizeof(LLMRequest));
    // 创建 LLM 响应队列，用于发送处理完的响应给请求者（优化：减少队列深度为3）
    llmResponseQueue = xQueueCreate(3, sizeof(LLMResponse));

    // 检查队列是否成功创建
    if (llmRequestQueue == NULL || llmResponseQueue == NULL) {
        Serial.println("Error creating LLM queues!");
    }
}

// ==================== 辅助函数实现 ====================

// 安全地分配PSRAM内存并拷贝字符串
char* LLMManager::allocateAndCopy(const String& str) {
    if (str.length() == 0) return nullptr;
    
    size_t len = str.length() + 1;
    char* buffer = (char*)ps_malloc(len);
    if (buffer) {
        strncpy(buffer, str.c_str(), len);
        buffer[len - 1] = '\0';
    }
    return buffer;
}

// 分配响应字符串内存的辅助方法
void LLMManager::allocateResponseString(char*& dest, const String& src) {
    dest = allocateAndCopy(src);
}

// 创建并发送LLM请求到队列的通用方法
bool LLMManager::createAndSendRequest(const String& requestId, const String& prompt, LLMMode mode) {
    LLMRequest request;
    memset(&request, 0, sizeof(LLMRequest));
    
    // 安全拷贝requestId到固定大小数组
    strncpy(request.requestId, requestId.c_str(), sizeof(request.requestId) - 1);
    request.requestId[sizeof(request.requestId) - 1] = '\0';
    
    // 使用PSRAM分配prompt内存
    request.prompt = allocateAndCopy(prompt);
    if (!request.prompt) {
        Serial.println("createAndSendRequest: Failed to allocate memory for prompt.");
        _usbShellManager->sendAiResponseToHost(requestId, "Error: Memory allocation failed.");
        return false;
    }
    
    request.mode = mode;
    
    // 发送请求到队列
    if (xQueueSend(llmRequestQueue, &request, portMAX_DELAY) != pdPASS) {
        Serial.println("createAndSendRequest: Failed to send request to queue.");
        free(request.prompt);
        _usbShellManager->sendAiResponseToHost(requestId, "Error: Failed to send request to LLM task.");
        return false;
    }
    
    return true;
}

// 初始化方法，用于加载配置
void LLMManager::begin() {
    configManager.loadConfig(); // 加载配置到内部的JsonDocument
    JsonDocument& config = configManager.getConfig(); // 获取加载后配置的引用
    // 从配置中获取上次使用的 LLM 提供商
    currentProvider = config["last_used"]["llm_provider"].as<String>();
    // 从配置中获取上次使用的模型
    currentModel = config["last_used"]["model"].as<String>();
    // 从配置中获取当前提供商的 API 密钥
    currentApiKey = config["llm_providers"][currentProvider]["api_key"].as<String>();

    // 打印 LLMManager 初始化信息
    Serial.printf("LLMManager initialized. Provider: %s, Model: %s\n", currentProvider.c_str(), currentModel.c_str());
}


// 根据当前提供商生成响应（此函数由后台任务调用）
String LLMManager::generateResponse(const String& requestId, const String& prompt, LLMMode mode) {
    // 检查WiFi是否连接
    if (wifiManager.getWiFiStatus() != "Connected") {
        return "Error: WiFi is not connected.";
    }

    // 打印 LLM 调用前最大的空闲堆内存块大小，用于调试内存使用情况
    Serial.printf("Largest Free Heap Block before LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("Free DRAM before LLM call: %u, Free PSRAM before LLM call: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    
    // 检查 API 密钥是否已设置
    if (currentApiKey.length() == 0) {
        return "Error: API Key is not set for the current provider.";
    }

    String response;
    // 根据当前提供商，调用相应的处理函数
    if (currentProvider == "deepseek" || currentProvider == "openrouter" || currentProvider == "openai") {
        response = getOpenAILikeResponse(requestId, prompt, mode);
    } else {
        response = "Error: Invalid LLM provider selected.";
    }

    // 打印 LLM 调用后最大的空闲堆内存块大小
    Serial.printf("Largest Free Heap Block after LLM call: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("Free DRAM after LLM call: %u, Free PSRAM after LLM call: %u\n", heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    return response;
}

// 处理来自主机的用户输入
void LLMManager::processUserInput(const String& requestId, const String& userInput) {
    String prompt = "User input: " + userInput;
    createAndSendRequest(requestId, prompt, ADVANCED_MODE); // Shell通信使用高级模式
}

String LLMManager::getCurrentModelName() {
    return currentModel;
}

// 处理来自主机的shell命令执行结果
void LLMManager::processShellOutput(const String& requestId, const String& cmd, const String& output, const String& error, const String& status, int exitCode) {
    // 将上一个命令及其输出作为上下文，构建新的提示
    String prompt = "Previous shell command: " + cmd + "\n" +
                    "STDOUT: " + output + "\n" +
                    "STDERR: " + error + "\n" +
                    "Status: " + status + "\n" +
                    "Exit Code: " + String(exitCode) + "\n" +
                    "Based on the above shell output, what should be the next action or response?";
    
    createAndSendRequest(requestId, prompt, ADVANCED_MODE);
}


// 获取类 OpenAI 格式的响应 (适用于 DeepSeek, OpenRouter, OpenAI)
String LLMManager::getOpenAILikeResponse(const String& requestId, const String& prompt, LLMMode mode) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // 跳过SSL证书验证

    const char* apiHost = "";
    String apiPath;

    if (currentProvider == "openrouter") {
        apiHost = OPENROUTER_API_HOST;
        apiPath = "/api/v1/chat/completions";
    } else if (currentProvider == "deepseek") {
        apiHost = DEEPSEEK_API_HOST;
        apiPath = "/chat/completions";
    } else if (currentProvider == "openai") {
        apiHost = OPENAI_API_HOST;
        apiPath = "/v1/chat/completions";
    } else {
        return "Error: Invalid OpenAI-like provider selected.";
    }

    String apiUrl = "https://" + String(apiHost) + apiPath;
    http.begin(client, apiUrl); // 使用安全客户端
    http.setTimeout(NETWORK_TIMEOUT);     // 设置网络请求总超时
    client.setTimeout(STREAM_TIMEOUT / 1000);   // 设置底层 TCP 超时（秒）
    
    // 启用更详细的HTTP调试输出
    http.setReuse(false);       // 禁用连接重用
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    http.addHeader("Content-Type", "application/json");
    // 添加额外的请求头以提高兼容性
    http.addHeader("Accept", "application/json");
    http.addHeader("Connection", "close");
    http.addHeader("Authorization", "Bearer " + currentApiKey);
    if (currentProvider == "openrouter") {
        http.addHeader("HTTP-Referer", "http://localhost");
    }

    JsonDocument doc;
    doc["model"] = currentModel;

    String systemPrompt = generateSystemPrompt(mode);
    if (systemPrompt.length() > 0) {
        doc["messages"].add<JsonObject>();
        doc["messages"][0]["role"] = "system";
        doc["messages"][0]["content"] = systemPrompt;
        doc["messages"].add<JsonObject>();
        doc["messages"][1]["role"] = "user";
        doc["messages"][1]["content"] = prompt;
    } else {
        doc["messages"].add<JsonObject>();
        doc["messages"][0]["role"] = "user";
        doc["messages"][0]["content"] = prompt;
    }

    String requestBody;
    serializeJson(doc, requestBody);

    int httpCode = http.POST(requestBody);
    Serial.printf("POST request completed with code: %d\n", httpCode);

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("[LLM] Starting to read response...");
            
            // 获取内容长度（如果有的话）
            int contentLength = http.getSize();
            Serial.printf("[LLM] Content-Length: %d\n", contentLength);
            
            // 使用HTTPClient的getString方法，它能更好地处理chunked编码
            WiFiClient* stream = http.getStreamPtr();
            
            // 分配PSRAM缓冲区
            char* buffer = (char*)ps_malloc(MAX_RESPONSE_LENGTH);
            if (!buffer) {
                http.end();
                return "Error: Failed to allocate memory for response buffer";
            }
            memset(buffer, 0, MAX_RESPONSE_LENGTH);
            
            size_t totalBytesRead = 0;
            unsigned long startTime = millis();
            unsigned long lastDataTime = millis();
            bool receivedData = false;
            
            // 改进的读取循环：更好地处理chunked编码
            while ((millis() - startTime < STREAM_TIMEOUT) && 
                   totalBytesRead < MAX_RESPONSE_LENGTH - 1) {
                
                // 等待数据可用或连接关闭
                size_t available = stream->available();
                
                if (available) {
                    // 有数据可读
                    receivedData = true;
                    size_t bytesToRead = min(available, MAX_RESPONSE_LENGTH - 1 - totalBytesRead);
                    size_t bytesRead = stream->readBytes(buffer + totalBytesRead, bytesToRead);
                    totalBytesRead += bytesRead;
                    lastDataTime = millis();
                    
                    Serial.printf("[LLM] Read %d bytes (total: %d)\n", bytesRead, totalBytesRead);
                } else {
                    // 没有数据可用
                    if (!http.connected()) {
                        // 连接已关闭，数据接收完毕
                        Serial.println("[LLM] Connection closed by server");
                        break;
                    }
                    
                    // 如果已经接收过数据，且超过DATA_TIMEOUT没有新数据，认为传输完成
                    if (receivedData && (millis() - lastDataTime > DATA_TIMEOUT)) {
                        Serial.println("[LLM] No more data, assuming transfer complete");
                        break;
                    }
                    
                    // 等待更多数据
                    delay(10);
                }
            }
            
            buffer[totalBytesRead] = '\0';
            Serial.printf("[LLM] Total received: %d bytes\n", totalBytesRead);
            
            http.end();
            
            // 检查是否接收到数据
            if (totalBytesRead == 0) {
                free(buffer);
                Serial.println("[LLM] Error: Received 0 bytes");
                return "Error: No data received from server";
            }
            
            // 输出前100个字符用于调试
            Serial.print("[LLM] Response preview: ");
            for (int i = 0; i < min((int)totalBytesRead, 100); i++) {
                if (buffer[i] >= 32 && buffer[i] < 127) {
                    Serial.print((char)buffer[i]);
                } else {
                    Serial.print('.');
                }
            }
            Serial.println();
            
            // 提取JSON响应
            char* start = strchr(buffer, '{');
            char* end = strrchr(buffer, '}');
            
            String jsonStr;
            if (start && end && end > start) {
                *++end = '\0';
                jsonStr = String(start);
            } else {
                free(buffer);
                Serial.println("[LLM] Error: No valid JSON found in response");
                return "Error: Invalid response format";
            }
            
            free(buffer);
            
            if (jsonStr.isEmpty()) {
                Serial.println("[LLM] Error: Empty JSON string");
                return "Error: Empty response";
            }
            
            Serial.printf("[LLM] JSON length: %d bytes\n", jsonStr.length());
            
            // 解析JSON
            JsonDocument responseDoc;
            DeserializationError error = deserializeJson(responseDoc, jsonStr);
            
            if (error) {
                Serial.printf("[LLM] JSON parse error: %s\n", error.c_str());
                return "Error: Failed to parse response";
            }

            // 只提取 LLM 的实际回复内容
            if (responseDoc["choices"][0]["message"]["content"].is<String>()) {
                String content = responseDoc["choices"][0]["message"]["content"].as<String>();
                Serial.printf("[LLM] Extracted content length: %d\n", content.length());
                return content;
            }
            
            Serial.println("[LLM] Error: Invalid response structure");
            return "Error: Invalid response structure";
        } else {
            http.end();
            Serial.printf("[LLM] HTTP error: %d\n", httpCode);
            return "Error: Request failed";
        }
    } else {
        http.end();
        Serial.println("[LLM] Connection failed");
        return "Error: Connection failed";
    }
}


// 根据模式和工具生成系统提示（优化：使用静态缓存避免重复构建）
String LLMManager::generateSystemPrompt(LLMMode mode) {
    // 静态缓存，只在第一次调用时构建
    static String cachedChatPrompt;
    static String cachedAdvancedPrompt;
    static bool initialized = false;
    
    if (!initialized) {
        // ============ 聊天模式提示词 ============
        cachedChatPrompt = 
            "You are a helpful and friendly AI assistant. "
            "Respond concisely and accurately to user queries with clear explanations.";
        
        // ============ 高级模式提示词（完整版，整合所有工具描述）============
        cachedAdvancedPrompt = 
            "# Your Role\n"
            "You are an advanced AI assistant integrated into an ESP32-S3 device with multi-modal capabilities. "
            "You can interact with the host computer through shell commands, USB HID (keyboard/mouse), and GPIO control. "
            "Your purpose is to help users accomplish tasks by intelligently combining these capabilities.\n"
            "\n"
            "# Core Capabilities\n"
            "1. **Command Execution**: Execute shell commands on the host computer and analyze their output\n"
            "2. **Natural Language Communication**: Provide explanations, suggestions, and responses to users\n"
            "3. **USB HID Control**: Simulate keyboard typing and mouse operations (future)\n"
            "4. **GPIO Control**: Control hardware pins on the ESP32-S3 (future)\n"
            "\n"
            "# Available Tools\n"
            "\n"
            "## Primary Tool: sendtoshell\n"
            "Use this tool when you need to execute commands or send structured responses.\n"
            "\n"
            "**Parameters**:\n"
            "  - type: \"command\" | \"text\"\n"
            "  - value: The command string or text message\n"
            "\n"
            "**When to use**:\n"
            "  • type=\"command\": Execute shell commands (file operations, system queries, app launching)\n"
            "  • type=\"text\": Send important notifications or structured messages\n"
            "\n"
            "## Future Tools (Not Yet Implemented)\n"
            "• usb_hid_keyboard_type - Type text via USB HID\n"
            "• usb_hid_mouse_click - Simulate mouse clicks\n"
            "• usb_hid_mouse_move - Move mouse cursor\n"
            "• gpio_set_level - Control GPIO pins\n"
            "\n"
            "# Response Modes\n"
            "\n"
            "You have TWO ways to respond:\n"
            "\n"
            "## Mode 1: Tool Call (JSON Format)\n"
            "Use when you need to execute commands or send structured data:\n"
            "\n"
            "```json\n"
            "{\n"
            "  \"tool_calls\": [\n"
            "    {\n"
            "      \"name\": \"sendtoshell\",\n"
            "      \"args\": {\n"
            "        \"type\": \"command\",\n"
            "        \"value\": \"ls -lah\"\n"
            "      }\n"
            "    }\n"
            "  ]\n"
            "}\n"
            "```\n"
            "\n"
            "## Mode 2: Natural Language (Direct Text)\n"
            "Use for casual conversation, explanations, questions, or when no action is needed:\n"
            "\n"
            "```\n"
            "I can help you manage files, execute commands, and automate tasks on your computer. "
            "What would you like me to do?\n"
            "```\n"
            "\n"
            "# When to Use Each Mode\n"
            "\n"
            "**Use JSON Tool Call when**:\n"
            "- User asks you to DO something (execute, create, delete, run, etc.)\n"
            "- You need to execute a shell command\n"
            "- Taking action is required\n"
            "\n"
            "**Use Natural Language when**:\n"
            "- User asks ABOUT something (what, how, why, explain)\n"
            "- Providing explanations or suggestions\n"
            "- Casual conversation or clarifying questions\n"
            "- Analyzing or interpreting command results\n"
            "- No action is immediately needed\n"
            "\n"
            "# Example Interactions\n"
            "\n"
            "**Example 1: Action Required (JSON)**\n"
            "User: \"List all files in the current directory\"\n"
            "Response:\n"
            "```json\n"
            "{\n"
            "  \"tool_calls\": [\n"
            "    {\n"
            "      \"name\": \"sendtoshell\",\n"
            "      \"args\": {\n"
            "        \"type\": \"command\",\n"
            "        \"value\": \"ls -lah\"\n"
            "      }\n"
            "    }\n"
            "  ]\n"
            "}\n"
            "```\n"
            "\n"
            "**Example 2: Explanation (Natural Language)**\n"
            "User: \"What can you help me with?\"\n"
            "Response:\n"
            "```\n"
            "I can help you with various tasks on your computer! I can execute shell commands, "
            "manage files and directories, run applications, check system status, and automate "
            "repetitive tasks. Just tell me what you need, and I'll do my best to help!\n"
            "```\n"
            "\n"
            "**Example 3: Analysis (Natural Language)**\n"
            "User: \"Previous command output: [error logs]\"\n"
            "Response:\n"
            "```\n"
            "It looks like there's a permission error. The file you're trying to access requires "
            "elevated privileges. Would you like me to try running the command with sudo?\n"
            "```\n"
            "\n"
            "**Example 4: Follow-up Action (JSON)**\n"
            "User: \"Yes, use sudo\"\n"
            "Response:\n"
            "```json\n"
            "{\n"
            "  \"tool_calls\": [\n"
            "    {\n"
            "      \"name\": \"sendtoshell\",\n"
            "      \"args\": {\n"
            "        \"type\": \"command\",\n"
            "        \"value\": \"sudo cat /var/log/syslog\"\n"
            "      }\n"
            "    }\n"
            "  ]\n"
            "}\n"
            "```\n"
            "\n"
            "# Decision Making Guidelines\n"
            "1. **Understand intent**: Is the user asking you to DO or to EXPLAIN?\n"
            "2. **Choose response mode**: Action → JSON, Conversation → Natural Language\n"
            "3. **Be contextual**: Consider previous commands and their output\n"
            "4. **Be safe**: Avoid destructive commands without clear confirmation\n"
            "5. **Be helpful**: Explain complex operations, suggest alternatives\n"
            "6. **Be efficient**: Use the most direct approach to achieve the goal\n"
            "\n"
            "Remember: Choose the response mode that best fits the situation. Don't force JSON when "
            "natural conversation is more appropriate!";
        
        initialized = true;
    }
    
    return (mode == CHAT_MODE) ? cachedChatPrompt : cachedAdvancedPrompt;
}

// 处理 LLM 的原始响应，解析工具调用或自然语言回复。
void LLMManager::handleLLMRawResponse(const String& requestId, const String& llmContentString) {
    LLMResponse response;
    memset(&response, 0, sizeof(LLMResponse));
    
    // 安全拷贝requestId到固定大小数组
    strncpy(response.requestId, requestId.c_str(), sizeof(response.requestId) - 1);
    response.requestId[sizeof(response.requestId) - 1] = '\0';
    response.isToolCall = false;

    JsonDocument contentDoc;
    DeserializationError error = deserializeJson(contentDoc, llmContentString);

    if (error) {
        // JSON解析失败，视为自然语言响应
        Serial.printf("handleLLMRawResponse: Natural language response (parse error: %s)\n", error.c_str());
        _usbShellManager->sendAiResponseToHost(requestId, llmContentString);
        allocateResponseString(response.naturalLanguageResponse, llmContentString);
    } else {
        // 成功解析JSON，检查是否有工具调用
        if (contentDoc["tool_calls"].is<JsonArray>() && contentDoc["tool_calls"].size() > 0) {
            JsonObject toolCall = contentDoc["tool_calls"][0].as<JsonObject>();
            String toolName = toolCall["name"] | "";

            if (toolName == "sendtoshell") {
                String outputType = toolCall["args"]["type"] | "";
                String value = toolCall["args"]["value"] | "";

                response.isToolCall = true;
                strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                response.toolName[sizeof(response.toolName) - 1] = '\0';
                
                // 构建toolArgs的JSON字符串
                JsonDocument argsDoc;
                argsDoc["type"] = outputType;
                argsDoc["value"] = value;
                String argsStr;
                serializeJson(argsDoc, argsStr);
                allocateResponseString(response.toolArgs, argsStr);

                if (outputType == "command") {
                    Serial.printf("LLM requested shell command: %s\n", value.c_str());
                    _usbShellManager->sendShellCommandToHost(requestId, value);
                } else if (outputType == "text") {
                    Serial.printf("LLM requested AI response: %s\n", value.c_str());
                    _usbShellManager->sendAiResponseToHost(requestId, value);
                } else {
                    Serial.printf("LLM called sendtoshell with unknown output_type: %s\n", outputType.c_str());
                    String errorMsg = "Error: LLM called sendtoshell with unknown output type.";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                }
            } else {
                Serial.printf("LLM called unknown tool: %s\n", toolName.c_str());
                String errorMsg = "Error: LLM called an unknown tool: " + toolName;
                _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                allocateResponseString(response.naturalLanguageResponse, errorMsg);
                response.isToolCall = false;
            }
        } else {
            // 解析成功但没有tool_calls，视为自然语言响应
            Serial.println("handleLLMRawResponse: No tool_calls, treating as natural language.");
            _usbShellManager->sendAiResponseToHost(requestId, llmContentString);
            allocateResponseString(response.naturalLanguageResponse, llmContentString);
        }
    }

    // 发送响应到队列
    if (xQueueSend(llmResponseQueue, &response, 0) != pdPASS) {
        Serial.println("handleLLMRawResponse: Failed to send response to queue.");
        // 发送失败，释放已分配的内存
        if (response.toolArgs) free(response.toolArgs);
        if (response.naturalLanguageResponse) free(response.naturalLanguageResponse);
    }
}

// LLM aysnc loop
void LLMManager::loop() {
    LLMRequest request;   // 用于存储接收到的请求
    String llmRawResponse; // 用于存储生成的原始响应

    // 检查队列中是否有请求，非阻塞
    if (xQueueReceive(llmRequestQueue, &request, 0) == pdPASS) {
        Serial.printf("LLMTask: Received request for prompt: %s (requestId: %s)\n", request.prompt ? request.prompt : "NULL", request.requestId);
        
        if (request.prompt) {
            // 将char*转换为String用于generateResponse函数
            String requestIdStr = String(request.requestId);
            String promptStr = String(request.prompt);
            
            // 调用核心函数生成响应
            String llmContent = generateResponse(requestIdStr, promptStr, request.mode);
            Serial.printf("LLMTask: Generated content: %s\n", llmContent.c_str());

            // 处理LLM的原始响应，解析工具调用或自然语言回复
            handleLLMRawResponse(requestIdStr, llmContent);
            
            // 释放请求的prompt内存（接收方负责释放）
            free(request.prompt);
            request.prompt = nullptr;
        } else {
            Serial.println("LLMTask: Received request with NULL prompt, skipping.");
        }
    }
}
