document.addEventListener('DOMContentLoaded', () => {
    // --- 元素选择器 ---
    // 对话模式按钮
    const dialogueModeBtn = document.getElementById('dialogue-mode-btn');
    // 高级模式按钮
    const advancedModeBtn = document.getElementById('advanced-mode-btn');
    // 高级模式面板，包含工具选择等
    const advancedModePanels = document.getElementById('advanced-mode-panels');
    // 设置按钮
    const settingsBtn = document.getElementById('settings-btn');
    // 设置侧边栏
    const settingsSidebar = document.getElementById('settings-sidebar');
    // 关闭设置侧边栏按钮
    const closeSidebarBtn = settingsSidebar.querySelector('.close-sidebar-btn');
    // 消息输入框
    const messageInput = document.getElementById('message-input');
    // 发送消息按钮
    const sendBtn = document.getElementById('send-btn');
    // 消息显示容器
    const messagesContainer = document.getElementById('messages');
    // 所有功能项（工具）
    const functionItems = document.querySelectorAll('.function-item');
    
    // 设置侧边栏中的元素
    const wifiSsidInput = document.getElementById('wifi-ssid'); // WiFi SSID 输入框
    const wifiPasswordInput = document.getElementById('wifi-password'); // WiFi 密码输入框
    const geminiApiKeyInput = document.getElementById('gemini-api-key'); // Gemini API 密钥输入框
    const deepseekApiKeyInput = document.getElementById('deepseek-api-key'); // DeepSeek API 密钥输入框
    const chatgptApiKeyInput = document.getElementById('chatgpt-api-key'); // ChatGPT API 密钥输入框
    const saveGeminiKeyBtn = document.getElementById('save-gemini-key-btn'); // 保存 Gemini 密钥按钮
    const saveDeepseekKeyBtn = document.getElementById('save-deepseek-key-btn'); // 保存 DeepSeek 密钥按钮
    const saveChatGPTKeyBtn = document.getElementById('save-chatgpt-key-btn'); // 保存 ChatGPT 密钥按钮
    const llmSelect = document.getElementById('llm-select'); // LLM 选择下拉框
    const saveLlmProviderBtn = document.getElementById('save-llm-provider-btn'); // 保存 LLM 提供商按钮

    let websocket; // WebSocket 实例

    // --- 核心功能函数 ---
    /**
     * @brief 在消息容器中添加一条新消息。
     * @param text 消息文本内容。
     * @param sender 消息发送者（'user', 'ai', 'system', 'system-error'）。
     */
    function appendMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        messageElement.textContent = text;
        messagesContainer.appendChild(messageElement);
        // 滚动到最新消息
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    /**
     * @brief 通过 WebSocket 向 ESP32 设备发送数据。
     * @param data 要发送的数据对象，将被转换为 JSON 字符串。
     */
    function sendToESP32(data) {
        if (websocket && websocket.readyState === WebSocket.OPEN) {
            websocket.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket 未连接。无法发送数据。');
            appendMessage('无法发送消息，连接未建立。', 'system-error');
        }
    }

    // --- WebSocket 初始化 ---
    /**
     * @brief 初始化 WebSocket 连接。
     *
     * 设置 WebSocket 的连接、消息接收、错误和关闭事件处理。
     */
    function initWebSocket() {
        // 连接到当前主机的 WebSocket 服务
        websocket = new WebSocket(`ws://${location.host}/ws`);

        // WebSocket 连接成功时触发
        websocket.onopen = () => {
            console.log('WebSocket 已连接');
            appendMessage('已连接到设备。', 'system');
        };

        // 收到来自 ESP32 的消息时触发
        websocket.onmessage = (event) => {
            console.log('收到来自 ESP32 的消息:', event.data);
            try {
                const data = JSON.parse(event.data);
                // 后端发送多种消息类型，但对于聊天界面，主要关注 'chat_message' 或原始文本。
                if (data.type === 'chat_message' && data.sender === 'bot') {
                    appendMessage(data.text, 'ai');
                } else if (data.type === 'tool_execution_result') {
                    // 显示工具执行结果
                    const resultText = `工具 '${data.tool_name}' 已执行。结果: ${data.result}`;
                    appendMessage(resultText, 'system');
                } else {
                     // 其他 JSON 消息或纯文本的备用处理
                    appendMessage(event.data, 'ai');
                }
            } catch (e) {
                // 如果数据不是 JSON，则作为纯文本显示
                appendMessage(event.data, 'ai');
            }
        };

        // WebSocket 连接出错时触发
        websocket.onerror = (event) => {
            console.error('WebSocket 错误:', event);
            appendMessage('WebSocket 连接出错。', 'system-error');
        };

        // WebSocket 连接关闭时触发
        websocket.onclose = () => {
            console.log('WebSocket 已关闭');
            appendMessage('连接已断开。', 'system-error');
        };
    }

    // --- UI 逻辑 ---
    /**
     * @brief 发送用户输入的消息。
     *
     * 获取输入框内容，添加到消息列表，并通过 WebSocket 发送给 ESP32。
     */
    function sendMessage() {
        const messageText = messageInput.value.trim();
        if (messageText) {
            appendMessage(messageText, 'user'); // 在UI上显示用户消息
            sendToESP32({ type: 'chat_message', payload: messageText }); // 通过WebSocket发送给ESP32
            messageInput.value = ''; // 清空输入框
            messageInput.style.height = 'auto'; // 重置输入框高度
        }
    }
    
    /**
     * @brief 更新授权工具列表并发送给 ESP32。
     *
     * 遍历所有功能项，收集当前激活的工具，并通过 WebSocket 发送。
     */
    function updateAuthorizedTools() {
        const authorizedTools = [];
        functionItems.forEach(item => {
            const autoBtn = item.querySelector('.auto-btn');
            if (autoBtn.classList.contains('active')) {
                const toolName = item.querySelector('span').textContent;
                authorizedTools.push(toolName);
            }
        });
        sendToESP32({ type: 'set_authorized_tools', tools: authorizedTools });
    }

    // --- 事件监听器 ---
    // 对话模式按钮点击事件
    dialogueModeBtn.addEventListener('click', () => {
        dialogueModeBtn.classList.add('active');
        advancedModeBtn.classList.remove('active');
        advancedModePanels.style.display = 'none'; // 隐藏高级模式面板
        sendToESP32({ type: 'set_llm_mode', mode: 'chat' }); // 通知ESP32切换到聊天模式
    });

    // 高级模式按钮点击事件
    advancedModeBtn.addEventListener('click', () => {
        advancedModeBtn.classList.add('active');
        dialogueModeBtn.classList.remove('active');
        advancedModePanels.style.display = 'flex'; // 显示高级模式面板
        sendToESP32({ type: 'set_llm_mode', mode: 'advanced' }); // 通知ESP32切换到高级模式
        updateAuthorizedTools(); // 发送当前工具授权状态
    });

    // 设置按钮点击事件
    settingsBtn.addEventListener('click', () => {
        settingsSidebar.classList.add('open'); // 打开设置侧边栏
    });

    // 关闭侧边栏按钮点击事件
    closeSidebarBtn.addEventListener('click', () => {
        settingsSidebar.classList.remove('open'); // 关闭设置侧边栏
    });

    // 消息输入框内容变化事件，用于自动调整高度
    messageInput.addEventListener('input', () => {
        messageInput.style.height = 'auto';
        messageInput.style.height = `${messageInput.scrollHeight}px`;
    });

    // 发送按钮点击事件
    sendBtn.addEventListener('click', sendMessage);

    // 消息输入框按键事件，回车发送消息（Shift+Enter 换行）
    messageInput.addEventListener('keypress', (event) => {
        if (event.key === 'Enter' && !event.shiftKey) {
            event.preventDefault(); // 阻止默认回车行为
            sendMessage();
        }
    });

    // 遍历所有功能项，为每个工具的“自动”按钮添加点击事件
    functionItems.forEach(item => {
        const autoBtn = item.querySelector('.auto-btn');
        autoBtn.addEventListener('click', () => {
            autoBtn.classList.toggle('active'); // 切换激活状态
            // 如果当前处于高级模式，立即更新工具授权状态
            if (advancedModeBtn.classList.contains('active')) {
                updateAuthorizedTools();
            }
        });
    });

    // 保存 Gemini API 密钥的事件监听器
    saveGeminiKeyBtn.addEventListener('click', () => {
        const apiKey = geminiApiKeyInput.value.trim();
        if (apiKey) {
            fetch('/setGeminiApiKey', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `apiKey=${encodeURIComponent(apiKey)}`
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    appendMessage('Gemini API Key 已更新。', 'system');
                } else {
                    appendMessage(`Gemini API Key 更新失败: ${data.message}`, 'system-error');
                }
            })
            .catch(error => {
                console.error('错误:', error);
                appendMessage('Gemini API Key 更新时发生错误。', 'system-error');
            });
        } else {
            alert('请输入 Gemini API Key。');
        }
    });

    // 保存 DeepSeek API 密钥的事件监听器
    saveDeepseekKeyBtn.addEventListener('click', () => {
        const apiKey = deepseekApiKeyInput.value.trim();
        if (apiKey) {
            fetch('/setDeepSeekApiKey', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `apiKey=${encodeURIComponent(apiKey)}`
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    appendMessage('DeepSeek API Key 已更新。', 'system');
                } else {
                    appendMessage(`DeepSeek API Key 更新失败: ${data.message}`, 'system-error');
                }
            })
            .catch(error => {
                console.error('错误:', error);
                appendMessage('DeepSeek API Key 更新时发生错误。', 'system-error');
            });
        } else {
            alert('请输入 DeepSeek API Key。');
        }
    });

    // 保存 ChatGPT API 密钥的事件监听器
    saveChatGPTKeyBtn.addEventListener('click', () => {
        const apiKey = chatgptApiKeyInput.value.trim();
        if (apiKey) {
            fetch('/setChatGPTApiKey', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `apiKey=${encodeURIComponent(apiKey)}`
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    appendMessage('ChatGPT API Key 已更新。', 'system');
                } else {
                    appendMessage(`ChatGPT API Key 更新失败: ${data.message}`, 'system-error');
                }
            })
            .catch(error => {
                console.error('错误:', error);
                appendMessage('ChatGPT API Key 更新时发生错误。', 'system-error');
            });
        } else {
            alert('请输入 ChatGPT API Key。');
        }
    });

    // 保存 LLM 提供商的事件监听器
    saveLlmProviderBtn.addEventListener('click', () => {
        const selectedProvider = llmSelect.value;
        if (selectedProvider) {
            fetch('/setLLMProvider', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `provider=${encodeURIComponent(selectedProvider)}`
            })
            .then(response => response.json())
            .then(data => {
                if (data.status === 'success') {
                    appendMessage(`LLM 提供商已更新为: ${selectedProvider}。`, 'system');
                } else {
                    appendMessage(`LLM 提供商更新失败: ${data.message}`, 'system-error');
                }
            })
            .catch(error => {
                console.error('错误:', error);
                appendMessage('LLM 提供商更新时发生错误。', 'system-error');
            });
        } else {
            alert('请选择一个 LLM 提供商。');
        }
    });

    // --- 初始化执行 ---
    appendMessage('欢迎使用 AIHi！正在尝试连接设备...', 'system');
    initWebSocket(); // 初始化 WebSocket 连接
});
