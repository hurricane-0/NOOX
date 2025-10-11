#include "llm_manager.h"
#include "usb_shell_manager.h" // Include the full header for UsbShellManager
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// 为LLM响应定义JSON文档缓冲区的大小
const size_t JSON_DOC_SIZE = 262144; // 为可能更大的LLM响应增加了大小，并利用PSRAM
const size_t MAX_RESPONSE_LENGTH = JSON_DOC_SIZE; // Maximum expected response length

// 定义静态响应缓冲区，避免动态内存分配
static char responseBuffer[1024];

// 用于配置网络超时的常量
const unsigned long NETWORK_TIMEOUT = 20000;  // 20秒
const unsigned long STREAM_TIMEOUT = 20000;   // 流读取超时20秒
const unsigned long RETRY_DELAY = 100;        // 重试延迟100ms

// 定义 API 端点
const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const char* OPENROUTER_API_HOST = "openrouter.ai";
const char* OPENAI_API_HOST = "api.openai.com";

// 构造函数
LLMManager::LLMManager(ConfigManager& config, AppWiFiManager& wifi, UsbShellManager* usbShellManager)
    : configManager(config), wifiManager(wifi), _usbShellManager(usbShellManager) {
    // 创建 LLM 请求队列，用于接收来自其他模块的请求
    llmRequestQueue = xQueueCreate(5, sizeof(LLMRequest));
    // 创建 LLM 响应队列，用于发送处理完的响应给请求者
    llmResponseQueue = xQueueCreate(5, sizeof(LLMResponse));

    // 检查队列是否成功创建
    if (llmRequestQueue == NULL || llmResponseQueue == NULL) {
        Serial.println("Error creating LLM queues!");
    }
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

    // 构造LLM请求
    LLMRequest request;
    request.requestId = requestId;
    request.prompt = prompt;
    request.mode = ADVANCED_MODE; // Shell通信使用高级模式

    // 将请求发送到队列，等待后台任务处理
    if (xQueueSend(llmRequestQueue, &request, portMAX_DELAY) != pdPASS) {
        Serial.println("processUserInput: Failed to send request to queue.");
        _usbShellManager->sendAiResponseToHost(requestId, "Error: Failed to send request to LLM task.");
    }
    // 响应将在 llmTask 中处理
}

String LLMManager::getCurrentModelName() {
    return currentModel;
}

// 处理来自主机的shell命令执行结果
void LLMManager::processShellOutput(const String& requestId, const String& cmd, const String& output, const String& error, const String& status, int exitCode) {
    // 将上一个命令及其输出作为上下文，构建新的提示
    String prompt = "Previous shell command: " + cmd + "\n";
    prompt += "STDOUT: " + output + "\n";
    prompt += "STDERR: " + error + "\n";
    prompt += "Status: " + status + "\n";
    prompt += "Exit Code: " + String(exitCode) + "\n";
    prompt += "Based on the above shell output, what should be the next action or response?";

    // 构造LLM请求
    LLMRequest request;
    request.requestId = requestId;
    request.prompt = prompt;
    request.mode = ADVANCED_MODE;

    // 发送请求到队列
    if (xQueueSend(llmRequestQueue, &request, portMAX_DELAY) != pdPASS) {
        Serial.println("processShellOutput: Failed to send request to queue.");
        _usbShellManager->sendAiResponseToHost(requestId, "Error: Failed to send request to LLM task.");
    }
    // 响应将在 llmTask 中处理
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
    http.setTimeout(30000);     // 设置30秒总超时
    client.setTimeout(20000);   // 设置底层 TCP 超时为20秒
    
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
            // 使用缓冲区方式接收响应以避免内存碎片
            WiFiClient* stream = http.getStreamPtr();
            char* buffer = (char*)ps_malloc(MAX_RESPONSE_LENGTH); // 使用PSRAM分配缓冲区
            if (!buffer) {
                http.end();
                return "Error: Failed to allocate memory for response buffer";
            }
            memset(buffer, 0, MAX_RESPONSE_LENGTH);
            
            size_t totalBytesRead = 0;
            unsigned long startTime = millis();
            
            Serial.println("Starting to read response stream...");
            
            while (http.connected() && (millis() - startTime < STREAM_TIMEOUT) && 
                   totalBytesRead < MAX_RESPONSE_LENGTH - 1) { // 留一个字节给空终止符
                size_t available = stream->available();
                if (available) {
                    size_t bytesToRead = min(available, MAX_RESPONSE_LENGTH - 1 - totalBytesRead);
                    size_t bytesRead = stream->readBytes(buffer + totalBytesRead, bytesToRead);
                    totalBytesRead += bytesRead;
                    startTime = millis(); // Reset timeout timer
                } else {
                    delay(RETRY_DELAY); // Wait for more data
                }
            }
            
            buffer[totalBytesRead] = '\0';
            Serial.printf("[LLM] Received %d bytes\n", totalBytesRead);
            
            // 提取JSON响应
            char* start = strchr(buffer, '{');
            char* end = strrchr(buffer, '}');
            
            String jsonStr;
            if (start && end && end > start) {
                *++end = '\0';
                jsonStr = String(start);
            } else {
                free(buffer);
                http.end();
                Serial.println("[LLM] Error: Invalid JSON response");
                return "Error: Invalid response format";
            }
            
            free(buffer);
            http.end();
            
            if(jsonStr.isEmpty()) {
                Serial.println("[LLM] Error: Empty response");
                return "Error: Empty response";
            }
            
            // 解析JSON
            JsonDocument responseDoc;
            DeserializationError error = deserializeJson(responseDoc, jsonStr);
            
            if(error) {
                Serial.printf("[LLM] JSON parse error: %s\n", error.c_str());
                return "Error: Failed to parse response";
            }

            // 只提取 LLM 的实际回复内容
            if (responseDoc["choices"][0]["message"]["content"].is<String>()) {
                String content = responseDoc["choices"][0]["message"]["content"].as<String>();
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


// 根据模式和工具生成系统提示
String LLMManager::generateSystemPrompt(LLMMode mode) {
    String prompt = "";
    // 聊天模式下的系统提示
    if (mode == CHAT_MODE) {
        prompt += "You are a helpful and friendly AI assistant. Respond concisely and accurately to user queries.";
    } 
    // 高级模式下的系统提示 (用于Shell工具调用)
    else if (mode == ADVANCED_MODE) {
        prompt += "You are an intelligent assistant that helps users by executing shell commands on their computer. ";
        prompt += "Based on the user's request and the output of previous commands, decide the next step. ";
        prompt += "Your response MUST be a JSON object representing a tool call to 'sendtoshell'. ";
        prompt += "The 'sendtoshell' tool can either execute a command or provide a natural language response. ";
        prompt += "Here are the tools you are authorized to use:\n";
        prompt += getToolDescriptions(); // 获取工具描述
        prompt += "\nExample of executing a shell command:\n";
        prompt += "```json\n";
        prompt += "{\n";
        prompt += "  \"tool_calls\": [\n";
        prompt += "    {\n";
        prompt += "      \"name\": \"sendtoshell\",\n";
        prompt += "      \"args\": {\n";
        prompt += "        \"type\": \"command\",\n";
        prompt += "        \"value\": \"ls -l\"\n";
        prompt += "      }\n";
        prompt += "    }\n";
        prompt += "  ]\n";
        prompt += "}\n";
        prompt += "```\n";
        prompt += "\nExample of providing a natural language response:\n";
        prompt += "```json\n";
        prompt += "{\n";
        prompt += "  \"tool_calls\": [\n";
        prompt += "    {\n";
        prompt += "      \"name\": \"sendtoshell\",\n";
        prompt += "      \"args\": {\n";
        prompt += "        \"type\": \"text\",\n";
        prompt += "        \"value\": \"Hello! How can I help you today?\"\n";
        prompt += "      }\n";
        prompt += "    }\n";
        prompt += "  ]\n";
        prompt += "}\n";
        prompt += "```\n";
    }
    return prompt;
}

// 获取工具的描述字符串
String LLMManager::getToolDescriptions() {
    String descriptions = "";
    // 添加 Shell 执行和 AI 回复的统一工具描述
    descriptions += "- **sendtoshell**: A unified tool to send either a shell command for execution or a natural language response to the host computer's terminal. Parameters: `{\"type\": \"command\" | \"text\", \"value\": \"command_or_text_content\"}`\n";
    
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

// 处理 LLM 的原始响应，解析工具调用或自然语言回复。
void LLMManager::handleLLMRawResponse(const String& requestId, const String& llmContentString) {
    // 创建静态缓冲区，避免动态内存分配
    static char responseBuffer[1024];
    static char requestIdBuffer[64];
    static char valueBuffer[1024];
    
    // 清空缓冲区
    memset(responseBuffer, 0, sizeof(responseBuffer));
    memset(requestIdBuffer, 0, sizeof(requestIdBuffer));
    memset(valueBuffer, 0, sizeof(valueBuffer));
    
    // 安全地复制requestId到静态缓冲区
    if (requestId.c_str() != nullptr) {
        strlcpy(requestIdBuffer, requestId.c_str(), sizeof(requestIdBuffer));
    }
    
    try {
        // 使用静态分配的JsonDocument，避免堆分配
        StaticJsonDocument<8192> contentDoc;
        DeserializationError error = deserializeJson(contentDoc, llmContentString);

        if (error) {
            // If parsing as JSON fails, it's a natural language response
            Serial.printf("handleLLMRawResponse: Content is natural language or invalid JSON. Error: %s\n", error.c_str());
            
            // 安全地复制响应到静态缓冲区
            strlcpy(valueBuffer, llmContentString.c_str(), sizeof(valueBuffer));
            Serial.printf("Natural language response: %s\n", valueBuffer);
            
            // 确保字符串不为空再发送
            if (strlen(valueBuffer) > 0) {
                _usbShellManager->sendAiResponseToHost(requestIdBuffer, valueBuffer);
            } else {
                _usbShellManager->sendAiResponseToHost(requestIdBuffer, "Error: Empty response from LLM.");
            }
        } else {
            // Successfully parsed as JSON, now check for tool calls
            if (contentDoc["tool_calls"].is<JsonArray>() && contentDoc["tool_calls"].size() > 0) {
                JsonObject toolCall = contentDoc["tool_calls"][0].as<JsonObject>();
                
                // 使用静态缓冲区存储工具名称
                const char* toolNamePtr = toolCall["name"] | "";
                
                if (strcmp(toolNamePtr, "sendtoshell") == 0) {
                    const char* outputTypePtr = toolCall["args"]["type"] | "";
                    const char* valuePtr = toolCall["args"]["value"] | "";
                    
                    // 安全地复制值到静态缓冲区
                    strlcpy(valueBuffer, valuePtr, sizeof(valueBuffer));

                    if (strcmp(outputTypePtr, "command") == 0) {
                        // 发送命令到主机
                        Serial.printf("LLM requested shell command: %s\n", valueBuffer);
                        _usbShellManager->sendShellCommandToHost(requestIdBuffer, valueBuffer);
                    } else if (strcmp(outputTypePtr, "text") == 0) {
                        // 发送自然语言回复到主机
                        Serial.printf("LLM requested AI response: %s\n", valueBuffer);
                        _usbShellManager->sendAiResponseToHost(requestIdBuffer, valueBuffer);
                    } else {
                        Serial.printf("LLM called sendtoshell with unknown output_type: %s\n", outputTypePtr);
                        _usbShellManager->sendAiResponseToHost(requestIdBuffer, "Error: LLM called sendtoshell with unknown output type.");
                    }
                } else {
                    // 未知的工具调用
                    Serial.printf("LLM called unknown tool: %s\n", toolNamePtr);
                    
                    // 使用snprintf安全地格式化错误消息
                    snprintf(responseBuffer, sizeof(responseBuffer), "Error: LLM called an unknown tool: %s", toolNamePtr);
                    _usbShellManager->sendAiResponseToHost(requestIdBuffer, responseBuffer);
                }
            } else {
                // Parsed as JSON, but no tool_calls, so treat as natural language
                Serial.println("handleLLMRawResponse: Parsed as JSON but no tool_calls. Treating as natural language.");
                
                // 安全地复制响应到静态缓冲区
                strlcpy(valueBuffer, llmContentString.c_str(), sizeof(valueBuffer));
                Serial.printf("Natural language response: %s\n", valueBuffer);
                
                _usbShellManager->sendAiResponseToHost(requestIdBuffer, valueBuffer);
            }
        }
    } catch (const std::exception& e) {
        // 捕获任何可能的异常
        Serial.printf("Exception in handleLLMRawResponse: %s\n", e.what());
        _usbShellManager->sendAiResponseToHost(requestIdBuffer, "Error: Exception occurred while processing LLM response.");
    } catch (...) {
        // 捕获任何未知异常
        Serial.println("Unknown exception in handleLLMRawResponse");
        _usbShellManager->sendAiResponseToHost(requestIdBuffer, "Error: Unknown exception while processing LLM response.");
    }

    // 手动触发垃圾回收
    ESP.getFreeHeap();
    
    // 确保在函数结束前所有临时对象都被正确释放
    Serial.println("handleLLMRawResponse: Processing completed successfully");
    
    // 再次手动触发垃圾回收
    ESP.getFreeHeap();
}

// LLM aysnc loop
void LLMManager::loop() {
    static LLMRequest request;   // 使用静态变量，避免栈上分配
    static char promptBuffer[1024];  // 静态缓冲区存储prompt
    static char requestIdBuffer[64]; // 静态缓冲区存储requestId

    // 检查队列中是否有请求，非阻塞
    if (xQueueReceive(llmRequestQueue, &request, 0) == pdPASS) {
        // 清空静态缓冲区
        memset(promptBuffer, 0, sizeof(promptBuffer));
        memset(requestIdBuffer, 0, sizeof(requestIdBuffer));
        
        // 安全地复制字符串到静态缓冲区
        if (request.prompt.c_str() != nullptr) {
            strlcpy(promptBuffer, request.prompt.c_str(), sizeof(promptBuffer));
        }
        if (request.requestId.c_str() != nullptr) {
            strlcpy(requestIdBuffer, request.requestId.c_str(), sizeof(requestIdBuffer));
        }
        
        Serial.printf("LLMTask: Received request for prompt: %s (requestId: %s)\n", promptBuffer, requestIdBuffer);
        
        // 调用核心函数生成响应
        String llmContent = generateResponse(request.requestId, request.prompt, request.mode);
        
        // 手动触发垃圾回收
        ESP.getFreeHeap();
        
        Serial.printf("LLMTask: Generated content: %s\n", llmContent.c_str());

        // 处理LLM的原始响应，解析工具调用或自然语言回复
        handleLLMRawResponse(request.requestId, llmContent);
        
        // 手动触发垃圾回收
        ESP.getFreeHeap();
        
        // 清空String对象
        request.prompt = "";
        request.requestId = "";
        llmContent = "";
        
        // 再次手动触发垃圾回收
        ESP.getFreeHeap();
    }
}
