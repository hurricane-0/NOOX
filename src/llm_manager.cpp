#include "llm_manager.h"
#include "usb_shell_manager.h" // Include the full header for UsbShellManager
#include "hid_manager.h" // Include HIDManager header
#include "hardware_manager.h" // Include HardwareManager header
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// 为LLM响应定义JSON文档缓冲区的大小
const size_t JSON_DOC_SIZE = 262144; // 为可能更大的LLM响应增加了大小，并利用PSRAM
const size_t MAX_RESPONSE_LENGTH = JSON_DOC_SIZE; // Maximum expected response length

// ==================== ConversationHistory 类实现 ====================

// 构造函数
ConversationHistory::ConversationHistory(size_t maxMessages) 
    : capacity(maxMessages), count(0), startIndex(0) {
    // 使用PSRAM分配消息数组
    messages = (ConversationMessage*)ps_malloc(sizeof(ConversationMessage) * capacity);
    if (messages) {
        // 初始化所有消息指针为nullptr
        for (size_t i = 0; i < capacity; i++) {
            messages[i].role = nullptr;
            messages[i].content = nullptr;
        }
        Serial.printf("ConversationHistory initialized with capacity: %d\n", capacity);
    } else {
        Serial.println("Error: Failed to allocate memory for ConversationHistory!");
    }
}

// 析构函数
ConversationHistory::~ConversationHistory() {
    clear();
    if (messages) {
        free(messages);
        messages = nullptr;
    }
}

// 添加新消息
void ConversationHistory::addMessage(const String& role, const String& content) {
    if (!messages) return;
    
    // 计算实际存储位置（环形缓冲）
    size_t index;
    if (count < capacity) {
        // 还未满，直接追加
        index = count;
        count++;
    } else {
        // 已满，覆盖最旧的消息
        index = startIndex;
        // 释放被覆盖消息的内存
        if (messages[index].role) {
            free(messages[index].role);
            messages[index].role = nullptr;
        }
        if (messages[index].content) {
            free(messages[index].content);
            messages[index].content = nullptr;
        }
        // 移动起始索引
        startIndex = (startIndex + 1) % capacity;
    }
    
    // 分配并拷贝role
    size_t roleLen = role.length() + 1;
    messages[index].role = (char*)ps_malloc(roleLen);
    if (messages[index].role) {
        strncpy(messages[index].role, role.c_str(), roleLen);
        messages[index].role[roleLen - 1] = '\0';
    }
    
    // 分配并拷贝content
    size_t contentLen = content.length() + 1;
    messages[index].content = (char*)ps_malloc(contentLen);
    if (messages[index].content) {
        strncpy(messages[index].content, content.c_str(), contentLen);
        messages[index].content[contentLen - 1] = '\0';
    }
    
    Serial.printf("Added message to history (count: %d/%d): %s\n", count, capacity, role.c_str());
}

// 清除所有消息
void ConversationHistory::clear() {
    if (!messages) return;
    
    for (size_t i = 0; i < capacity; i++) {
        if (messages[i].role) {
            free(messages[i].role);
            messages[i].role = nullptr;
        }
        if (messages[i].content) {
            free(messages[i].content);
            messages[i].content = nullptr;
        }
    }
    count = 0;
    startIndex = 0;
    Serial.println("Conversation history cleared.");
}

// 获取消息数量
size_t ConversationHistory::getMessageCount() const {
    return count;
}

// 将历史消息填充到JSON数组
void ConversationHistory::getMessages(JsonArray& messagesArray) {
    if (!messages || count == 0) return;
    
    // 按正确顺序添加消息
    for (size_t i = 0; i < count; i++) {
        size_t index = (startIndex + i) % capacity;
        if (messages[index].role && messages[index].content) {
            JsonObject msg = messagesArray.add<JsonObject>();
            msg["role"] = messages[index].role;
            msg["content"] = messages[index].content;
        }
    }
    
    Serial.printf("Retrieved %d messages from history\n", count);
}

// ==================== LLMManager 类实现 ====================

// 用于配置网络超时的常量
const unsigned long NETWORK_TIMEOUT = 40000;  // 40秒（网络请求总超时）
const unsigned long STREAM_TIMEOUT = 40000;   // 流读取超时40秒（给LLM生成留足时间）
const unsigned long DATA_TIMEOUT = 2000;      // 数据接收间隔超时2秒

// 定义 API 端点
const char* DEEPSEEK_API_HOST = "api.deepseek.com";
const char* OPENROUTER_API_HOST = "openrouter.ai";
const char* OPENAI_API_HOST = "api.openai.com";

// 构造函数
LLMManager::LLMManager(ConfigManager& config, AppWiFiManager& wifi, UsbShellManager* usbShellManager,
                       HIDManager* hidManager, HardwareManager* hardwareManager)
    : configManager(config), wifiManager(wifi), _usbShellManager(usbShellManager),
      _hidManager(hidManager), _hardwareManager(hardwareManager), currentMode(CHAT_MODE) {
    // 创建 LLM 请求队列，用于接收来自其他模块的请求（优化：减少队列深度为3）
    llmRequestQueue = xQueueCreate(3, sizeof(LLMRequest));
    // 创建 LLM 响应队列，用于发送处理完的响应给请求者（优化：减少队列深度为3）
    llmResponseQueue = xQueueCreate(3, sizeof(LLMResponse));

    // 检查队列是否成功创建
    if (llmRequestQueue == NULL || llmResponseQueue == NULL) {
        Serial.println("Error creating LLM queues!");
    }
    
    // 初始化对话历史（容量40，支持约20轮对话）
    conversationHistory = new ConversationHistory(40);
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

// 清除对话历史
void LLMManager::clearConversationHistory() {
    if (conversationHistory) {
        conversationHistory->clear();
        Serial.println("LLMManager: Conversation history cleared.");
    }
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

    // 构建消息数组
    JsonArray messages = doc["messages"].to<JsonArray>();
    
    // 1. 添加系统提示
    String systemPrompt = generateSystemPrompt(mode);
    if (systemPrompt.length() > 0) {
        JsonObject sysMsg = messages.add<JsonObject>();
        sysMsg["role"] = "system";
        sysMsg["content"] = systemPrompt;
    }
    
    // 2. 添加对话历史
    if (conversationHistory) {
        conversationHistory->getMessages(messages);
    }
    
    // 3. 添加当前用户输入
    JsonObject currentMsg = messages.add<JsonObject>();
    currentMsg["role"] = "user";
    currentMsg["content"] = prompt;

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
            "3. **USB HID Control**: Simulate keyboard typing and mouse operations on the host computer\n"
            "4. **GPIO Control**: Control hardware pins (LEDs and GPIO) on the ESP32-S3 device\n"
            "\n"
            "# Available Tools\n"
            "\n"
            "## Primary Tool: sendtoshell\n"
            "Use this tool when you need to execute commands or send structured responses.\n"
            "\n"
            "**Parameters** (both required):\n"
            "  - type: string - MUST be exactly \"command\" or \"text\" (case-sensitive)\n"
            "  - value: string - The command string or text message (non-empty)\n"
            "\n"
            "**When to use**:\n"
            "  • type=\"command\": Execute shell commands on the host computer\n"
            "    - File operations: ls, cat, mkdir, rm, etc.\n"
            "    - System queries: pwd, whoami, hostname, etc.\n"
            "    - App launching: open/start applications\n"
            "  • type=\"text\": Send structured text messages to the user\n"
            "    - Important status updates\n"
            "    - Notifications that don't require action\n"
            "\n"
            "**Best Practices**:\n"
            "  • Always validate both parameters are present\n"
            "  • Use platform-appropriate commands (consider Windows/Linux/Mac differences)\n"
            "  • For destructive operations, confirm with user first\n"
            "  • Keep commands simple and atomic when possible\n"
            "\n"
            "**Common Mistakes to Avoid**:\n"
            "  • DO NOT use empty values\n"
            "  • DO NOT use types other than \"command\" or \"text\"\n"
            "  • DO NOT chain complex commands without understanding the shell environment\n"
            "  • DO NOT assume the working directory or environment variables\n"
            "\n"
            "## HID Tools: USB Keyboard and Mouse Control\n"
            "\n"
            "### hid_keyboard_type\n"
            "Type text via USB HID keyboard emulation.\n"
            "\n"
            "**Parameters** (required):\n"
            "  - text: string - The text to type (non-empty)\n"
            "\n"
            "**Example** (return as raw JSON):\n"
            "{\"tool_calls\": [{\"name\": \"hid_keyboard_type\", \"args\": {\"text\": \"Hello World\"}}]}\n"
            "\n"
            "\n"
            "### hid_keyboard_press\n"
            "Press key combinations or special keys.\n"
            "\n"
            "**Parameters** (required):\n"
            "  - keys: string - Key combination like \"Ctrl+C\", \"Alt+Tab\", or special key like \"Enter\"\n"
            "\n"
            "**Supported modifiers**: Ctrl, Shift, Alt, Win (case-insensitive)\n"
            "**Supported special keys**: F1-F12, Enter, Tab, Backspace, Escape, Home, End, PageUp, PageDown, Delete, Arrow keys\n"
            "\n"
            "**Examples**:\n"
            "  - Copy: \"Ctrl+C\"\n"
            "  - Paste: \"Ctrl+V\"\n"
            "  - Switch window: \"Alt+Tab\"\n"
            "  - Task manager: \"Ctrl+Shift+Esc\"\n"
            "  - Press Enter: \"Enter\"\n"
            "\n"
            "### hid_keyboard_macro\n"
            "Execute a sequence of keyboard/mouse actions.\n"
            "\n"
            "**Parameters** (required):\n"
            "  - actions: array - Array of action objects\n"
            "\n"
            "**Action types**:\n"
            "  - {\"action\": \"type\", \"value\": \"text\"} - Type text\n"
            "  - {\"action\": \"press\", \"key\": \"Ctrl+C\"} - Press key combination\n"
            "  - {\"action\": \"delay\", \"ms\": 500} - Wait specified milliseconds\n"
            "  - {\"action\": \"click\", \"button\": \"left\"} - Click mouse button\n"
            "  - {\"action\": \"move\", \"x\": 10, \"y\": 20} - Move mouse\n"
            "\n"
            "**Example** (return as raw JSON):\n"
            "{\"tool_calls\": [{\"name\": \"hid_keyboard_macro\", \"args\": {\"actions\": "
            "[{\"action\": \"type\", \"value\": \"notepad\"}, {\"action\": \"delay\", \"ms\": 500}, "
            "{\"action\": \"press\", \"key\": \"Enter\"}]}}]}\n"
            "\n"
            "\n"
            "### hid_mouse_click\n"
            "Click mouse button.\n"
            "\n"
            "**Parameters** (optional):\n"
            "  - button: string - \"left\" (default), \"right\", or \"middle\"\n"
            "\n"
            "### hid_mouse_move\n"
            "Move mouse cursor relatively.\n"
            "\n"
            "**Parameters** (required):\n"
            "  - x: integer - Horizontal movement (positive=right, negative=left)\n"
            "  - y: integer - Vertical movement (positive=down, negative=up)\n"
            "\n"
            "## GPIO Tools: Hardware Pin Control\n"
            "\n"
            "### gpio_set\n"
            "Control GPIO output pins on the ESP32-S3 device.\n"
            "\n"
            "**Parameters** (required):\n"
            "  - gpio: string - GPIO name from the available list\n"
            "  - state: boolean - true for HIGH, false for LOW\n"
            "\n"
            "**Available GPIOs**:\n"
            "  - led1, led2, led3 - Onboard LED indicators\n"
            "  - gpio1, gpio2 - Reserved general-purpose GPIO pins\n"
            "\n"
            "**Use Cases**:\n"
            "  - Control indicator LEDs for status display\n"
            "  - Trigger external devices via GPIO pins\n"
            "  - Create visual feedback patterns\n"
            "\n"
            "**Examples** (return as raw JSON):\n"
            "Turn on LED 1:\n"
            "{\"tool_calls\": [{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led1\", \"state\": true}}]}\n"
            "\n"
            "Turn off all LEDs:\n"
            "{\"tool_calls\": [{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led1\", \"state\": false}}, "
            "{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led2\", \"state\": false}}, "
            "{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led3\", \"state\": false}}]}\n"
            "\n"
            "\n"
            "**Note**: Only output control is supported. GPIO names are case-insensitive.\n"
            "\n"
            "# Response Modes\n"
            "\n"
            "You have TWO ways to respond:\n"
            "\n"
            "## Mode 1: Tool Call (Pure JSON Format)\n"
            "Use when you need to execute commands or send structured data.\n"
            "\n"
            "**CRITICAL: Return ONLY the raw JSON object. DO NOT wrap it in markdown code blocks (```json or ```).**\n"
            "\n"
            "Example (return exactly this format):\n"
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
            "\n"
            "For multiple tool calls, add more objects to the array:\n"
            "{\n"
            "  \"tool_calls\": [\n"
            "    {\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led1\", \"state\": true}},\n"
            "    {\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led2\", \"state\": true}}\n"
            "  ]\n"
            "}\n"
            "\n"
            "## Mode 2: Natural Language (Direct Text)\n"
            "Use for casual conversation, explanations, questions, or when no action is needed.\n"
            "Simply respond with plain text (no JSON):\n"
            "\n"
            "I can help you manage files, execute commands, and automate tasks on your computer. "
            "What would you like me to do?\n"
            "\n"
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
            "Your response (raw JSON, no markdown):\n"
            "{\"tool_calls\": [{\"name\": \"sendtoshell\", \"args\": {\"type\": \"command\", \"value\": \"ls -lah\"}}]}\n"
            "\n"
            "**Example 2: Explanation (Natural Language)**\n"
            "User: \"What can you help me with?\"\n"
            "Your response (plain text):\n"
            "I can help you with various tasks on your computer! I can execute shell commands, "
            "manage files and directories, run applications, check system status, and automate "
            "repetitive tasks. Just tell me what you need, and I'll do my best to help!\n"
            "\n"
            "**Example 3: Analysis (Natural Language)**\n"
            "User: \"Previous command output: [error logs]\"\n"
            "Your response (plain text):\n"
            "It looks like there's a permission error. The file you're trying to access requires "
            "elevated privileges. Would you like me to try running the command with sudo?\n"
            "\n"
            "**Example 4: Follow-up Action (JSON)**\n"
            "User: \"Yes, use sudo\"\n"
            "Your response (raw JSON, no markdown):\n"
            "{\"tool_calls\": [{\"name\": \"sendtoshell\", \"args\": {\"type\": \"command\", \"value\": \"sudo cat /var/log/syslog\"}}]}\n"
            "\n"
            "**Example 5: Multiple GPIO Controls (JSON)**\n"
            "User: \"Turn on all LEDs\"\n"
            "Your response (raw JSON with multiple tool calls):\n"
            "{\"tool_calls\": [{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led1\", \"state\": true}}, "
            "{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led2\", \"state\": true}}, "
            "{\"name\": \"gpio_set\", \"args\": {\"gpio\": \"led3\", \"state\": true}}]}\n"
            "\n"
            "\n"
            "# Decision Making Guidelines\n"
            "1. **Understand intent**: Is the user asking you to DO or to EXPLAIN?\n"
            "2. **Choose response mode**: Action → JSON, Conversation → Natural Language\n"
            "3. **JSON format**: When using tool calls, return ONLY raw JSON. NEVER use ```json or ``` wrappers\n"
            "4. **Be contextual**: Consider previous commands and their output\n"
            "5. **Be safe**: Avoid destructive commands without clear confirmation\n"
            "6. **Be helpful**: Explain complex operations, suggest alternatives\n"
            "7. **Be efficient**: Use the most direct approach to achieve the goal\n"
            "\n"
            "**CRITICAL REMINDER**: For tool calls, output pure JSON like this:\n"
            "{\"tool_calls\": [{\"name\": \"tool_name\", \"args\": {...}}]}\n"
            "NOT like this: ```json\\n{...}\\n```\n"
            "\n"
            "Choose the response mode that best fits the situation. Don't force JSON when "
            "natural conversation is more appropriate!";
        
        initialized = true;
    }
    
    return (mode == CHAT_MODE) ? cachedChatPrompt : cachedAdvancedPrompt;
}

// 处理 LLM 的原始响应，解析工具调用或自然语言回复。
void LLMManager::handleLLMRawResponse(const String& requestId, const String& prompt, const String& llmContentString) {
    LLMResponse response;
    memset(&response, 0, sizeof(LLMResponse));
    
    // 安全拷贝requestId到固定大小数组
    strncpy(response.requestId, requestId.c_str(), sizeof(response.requestId) - 1);
    response.requestId[sizeof(response.requestId) - 1] = '\0';
    response.isToolCall = false;

    // 清理可能的markdown代码块标记
    String cleanedContent = llmContentString;
    cleanedContent.trim();
    
    // 移除开头的 ```json 或 ```
    if (cleanedContent.startsWith("```json")) {
        cleanedContent = cleanedContent.substring(7);
    } else if (cleanedContent.startsWith("```")) {
        cleanedContent = cleanedContent.substring(3);
    }
    
    // 移除结尾的 ```
    if (cleanedContent.endsWith("```")) {
        cleanedContent = cleanedContent.substring(0, cleanedContent.length() - 3);
    }
    
    cleanedContent.trim();

    JsonDocument contentDoc;
    DeserializationError error = deserializeJson(contentDoc, cleanedContent);

    if (error) {
        // JSON解析失败，视为自然语言响应
        Serial.printf("handleLLMRawResponse: Natural language response (parse error: %s)\n", error.c_str());
        _usbShellManager->sendAiResponseToHost(requestId, llmContentString);
        allocateResponseString(response.naturalLanguageResponse, llmContentString);
    } else {
        // 成功解析JSON，检查是否有工具调用
        if (contentDoc["tool_calls"].is<JsonArray>() && contentDoc["tool_calls"].size() > 0) {
            JsonArray toolCalls = contentDoc["tool_calls"].as<JsonArray>();
            Serial.printf("handleLLMRawResponse: Processing %d tool calls\n", toolCalls.size());
            
            // 遍历所有工具调用
            for (JsonVariant toolCallVariant : toolCalls) {
                if (!toolCallVariant.is<JsonObject>()) {
                    continue;
                }
                
                JsonObject toolCall = toolCallVariant.as<JsonObject>();
                String toolName = toolCall["name"] | "";

            if (toolName == "sendtoshell") {
                String outputType = toolCall["args"]["type"] | "";
                String value = toolCall["args"]["value"] | "";

                // 参数验证
                if (outputType.isEmpty() || value.isEmpty()) {
                    Serial.println("LLM called sendtoshell with missing parameters");
                    String errorMsg = "Error: sendtoshell requires both 'type' and 'value' parameters";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                } else if (outputType != "command" && outputType != "text") {
                    Serial.printf("LLM called sendtoshell with invalid type: %s\n", outputType.c_str());
                    String errorMsg = "Error: sendtoshell type must be 'command' or 'text', got: " + outputType;
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                } else {
                    // 参数验证通过，执行工具调用
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
                    }
                }
            } else if (toolName == "hid_keyboard_type") {
                // 输入文本字符串
                String text = toolCall["args"]["text"] | "";
                
                if (text.isEmpty()) {
                    Serial.println("LLM called hid_keyboard_type with missing text");
                    String errorMsg = "Error: hid_keyboard_type requires 'text' parameter";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                } else if (_hidManager && _hidManager->isReady()) {
                    Serial.printf("LLM requested keyboard type: %s\n", text.c_str());
                    _hidManager->sendString(text);
                    String successMsg = "Typed text: " + text;
                    _usbShellManager->sendAiResponseToHost(requestId, successMsg);
                    
                    response.isToolCall = true;
                    strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                    response.toolName[sizeof(response.toolName) - 1] = '\0';
                    
                    JsonDocument argsDoc;
                    argsDoc["text"] = text;
                    String argsStr;
                    serializeJson(argsDoc, argsStr);
                    allocateResponseString(response.toolArgs, argsStr);
                } else {
                    String errorMsg = "Error: HID not available";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                }
            } else if (toolName == "hid_keyboard_press") {
                // 处理组合键和特殊键
                String keys = toolCall["args"]["keys"] | "";
                
                if (keys.isEmpty()) {
                    Serial.println("LLM called hid_keyboard_press with missing keys");
                    String errorMsg = "Error: hid_keyboard_press requires 'keys' parameter";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                } else if (_hidManager && _hidManager->isReady()) {
                    Serial.printf("LLM requested keyboard press: %s\n", keys.c_str());
                    bool success = _hidManager->pressKeyCombination(keys);
                    
                    if (success) {
                        String successMsg = "Pressed keys: " + keys;
                        _usbShellManager->sendAiResponseToHost(requestId, successMsg);
                        
                        response.isToolCall = true;
                        strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                        response.toolName[sizeof(response.toolName) - 1] = '\0';
                        
                        JsonDocument argsDoc;
                        argsDoc["keys"] = keys;
                        String argsStr;
                        serializeJson(argsDoc, argsStr);
                        allocateResponseString(response.toolArgs, argsStr);
                    } else {
                        String errorMsg = "Error: " + _hidManager->getLastError();
                        _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                        allocateResponseString(response.naturalLanguageResponse, errorMsg);
                        response.isToolCall = false;
                    }
                } else {
                    String errorMsg = "Error: HID not available";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                }
            } else if (toolName == "hid_keyboard_macro") {
                // 处理宏操作
                if (!toolCall["args"]["actions"].is<JsonArray>()) {
                    Serial.println("LLM called hid_keyboard_macro with invalid actions");
                    String errorMsg = "Error: hid_keyboard_macro requires 'actions' array parameter";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                } else if (_hidManager && _hidManager->isReady()) {
                    JsonArray actions = toolCall["args"]["actions"].as<JsonArray>();
                    Serial.printf("LLM requested keyboard macro with %d actions\n", actions.size());
                    bool success = _hidManager->executeMacro(actions);
                    
                    if (success) {
                        String successMsg = "Executed macro with " + String(actions.size()) + " actions";
                        _usbShellManager->sendAiResponseToHost(requestId, successMsg);
                        
                        response.isToolCall = true;
                        strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                        response.toolName[sizeof(response.toolName) - 1] = '\0';
                        
                        JsonDocument argsDoc;
                        argsDoc["actions"] = actions;
                        String argsStr;
                        serializeJson(argsDoc, argsStr);
                        allocateResponseString(response.toolArgs, argsStr);
                    } else {
                        String errorMsg = "Error: " + _hidManager->getLastError();
                        _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                        allocateResponseString(response.naturalLanguageResponse, errorMsg);
                        response.isToolCall = false;
                    }
                } else {
                    String errorMsg = "Error: HID not available";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                }
            } else if (toolName == "hid_mouse_click") {
                // 处理鼠标点击
                String button = toolCall["args"]["button"] | "left";
                int buttonCode = MOUSE_BUTTON_LEFT;
                
                if (button == "right") buttonCode = MOUSE_BUTTON_RIGHT;
                else if (button == "middle") buttonCode = MOUSE_BUTTON_MIDDLE;
                
                if (_hidManager && _hidManager->isReady()) {
                    Serial.printf("LLM requested mouse click: %s\n", button.c_str());
                    _hidManager->clickMouse(buttonCode);
                    String successMsg = "Clicked mouse button: " + button;
                    _usbShellManager->sendAiResponseToHost(requestId, successMsg);
                    
                    response.isToolCall = true;
                    strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                    response.toolName[sizeof(response.toolName) - 1] = '\0';
                    
                    JsonDocument argsDoc;
                    argsDoc["button"] = button;
                    String argsStr;
                    serializeJson(argsDoc, argsStr);
                    allocateResponseString(response.toolArgs, argsStr);
                } else {
                    String errorMsg = "Error: HID not available";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                }
            } else if (toolName == "hid_mouse_move") {
                // 处理鼠标移动
                int x = toolCall["args"]["x"] | 0;
                int y = toolCall["args"]["y"] | 0;
                
                if (_hidManager && _hidManager->isReady()) {
                    Serial.printf("LLM requested mouse move: x=%d, y=%d\n", x, y);
                    _hidManager->moveMouse(x, y);
                    String successMsg = "Moved mouse by (" + String(x) + ", " + String(y) + ")";
                    _usbShellManager->sendAiResponseToHost(requestId, successMsg);
                    
                    response.isToolCall = true;
                    strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                    response.toolName[sizeof(response.toolName) - 1] = '\0';
                    
                    JsonDocument argsDoc;
                    argsDoc["x"] = x;
                    argsDoc["y"] = y;
                    String argsStr;
                    serializeJson(argsDoc, argsStr);
                    allocateResponseString(response.toolArgs, argsStr);
                } else {
                    String errorMsg = "Error: HID not available";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                }
            } else if (toolName == "gpio_set") {
                // 设置 GPIO 输出状态
                String gpioName = toolCall["args"]["gpio"] | "";
                bool state = toolCall["args"]["state"] | false;
                
                if (gpioName.isEmpty()) {
                    Serial.println("LLM called gpio_set with missing gpio parameter");
                    String errorMsg = "Error: gpio_set requires 'gpio' parameter";
                    _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                    allocateResponseString(response.naturalLanguageResponse, errorMsg);
                    response.isToolCall = false;
                } else if (_hardwareManager) {
                    Serial.printf("LLM requested gpio_set: %s = %s\n", gpioName.c_str(), state ? "HIGH" : "LOW");
                    bool success = _hardwareManager->setGpioOutput(gpioName, state);
                    
                    if (success) {
                        String successMsg = "GPIO " + gpioName + " set to " + (state ? "HIGH" : "LOW");
                        _usbShellManager->sendAiResponseToHost(requestId, successMsg);
                        
                        response.isToolCall = true;
                        strncpy(response.toolName, toolName.c_str(), sizeof(response.toolName) - 1);
                        response.toolName[sizeof(response.toolName) - 1] = '\0';
                        
                        JsonDocument argsDoc;
                        argsDoc["gpio"] = gpioName;
                        argsDoc["state"] = state;
                        String argsStr;
                        serializeJson(argsDoc, argsStr);
                        allocateResponseString(response.toolArgs, argsStr);
                    } else {
                        String errorMsg = "Error: Invalid GPIO name: " + gpioName + ". Available: " + _hardwareManager->getAvailableGpios();
                        _usbShellManager->sendAiResponseToHost(requestId, errorMsg);
                        allocateResponseString(response.naturalLanguageResponse, errorMsg);
                        response.isToolCall = false;
                    }
                } else {
                    String errorMsg = "Error: Hardware manager not available";
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
            } // 结束工具调用循环
        } else {
            // 解析成功但没有tool_calls，视为自然语言响应
            Serial.println("handleLLMRawResponse: No tool_calls, treating as natural language.");
            _usbShellManager->sendAiResponseToHost(requestId, llmContentString);
            allocateResponseString(response.naturalLanguageResponse, llmContentString);
        }
    }

    // 保存用户输入和AI回复到对话历史
    if (conversationHistory) {
        conversationHistory->addMessage("user", prompt);
        conversationHistory->addMessage("assistant", llmContentString);
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

            // 处理LLM的原始响应，解析工具调用或自然语言回复（传递prompt用于保存历史）
            handleLLMRawResponse(requestIdStr, promptStr, llmContent);
            
            // 释放请求的prompt内存（接收方负责释放）
            free(request.prompt);
            request.prompt = nullptr;
        } else {
            Serial.println("LLMTask: Received request with NULL prompt, skipping.");
        }
    }
}

// 获取当前 LLM 模式
String LLMManager::getCurrentMode() const {
    return (currentMode == CHAT_MODE) ? "Chat" : "Advanced";
}

// 设置 LLM 模式
void LLMManager::setCurrentMode(LLMMode mode) {
    currentMode = mode;
    Serial.printf("LLM Mode changed to: %s\n", getCurrentMode().c_str());
}
