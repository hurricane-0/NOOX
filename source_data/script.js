document.addEventListener('DOMContentLoaded', () => {
    // --- Global State ---
    let websocket; // WebSocket连接对象
    let currentConfig = {}; // 当前配置数据

    // --- Element Selectors ---
    // 获取页面上的主要DOM元素
    const dialogueModeBtn = document.getElementById('dialogue-mode-btn');
    const advancedModeBtn = document.getElementById('advanced-mode-btn');
    const advancedModePanels = document.getElementById('advanced-mode-panels');
    const settingsBtn = document.getElementById('settings-btn');
    const settingsSidebar = document.getElementById('settings-sidebar');
    const closeSidebarBtn = settingsSidebar.querySelector('.close-sidebar-btn');
    const messageInput = document.getElementById('message-input');
    const sendBtn = document.getElementById('send-btn');
    const messagesContainer = document.getElementById('messages');

    // --- New Settings Panel Elements ---
    // 设置面板相关元素
    const llmProviderSelect = document.getElementById('llm-provider-select');
    const llmModelSelect = document.getElementById('llm-model-select');
    const llmApiKeyInput = document.getElementById('llm-api-key');
    const wifiNetworkSelect = document.getElementById('wifi-network-select');
    const wifiConnectBtn = document.getElementById('wifi-connect-btn');
    const wifiDisconnectBtn = document.getElementById('wifi-disconnect-btn');
    const wifiDeleteBtn = document.getElementById('wifi-delete-btn');
    const wifiSsidInput = document.getElementById('wifi-ssid-input');
    const wifiPasswordInput = document.getElementById('wifi-password-input');
    const wifiAddBtn = document.getElementById('wifi-add-btn');
    const saveSettingsBtn = document.getElementById('save-settings-btn');

    // --- Core Functions ---
    // 添加消息到聊天窗口
    function appendMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        
        const textNode = document.createTextNode(text);
        messageElement.appendChild(textNode);

        const timestampElement = document.createElement('span');
        timestampElement.classList.add('timestamp');
        const now = new Date();
        timestampElement.textContent = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`;
        messageElement.appendChild(timestampElement);

        messagesContainer.appendChild(messageElement);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    // 通过WebSocket发送数据到ESP32
    function sendToESP32(data) {
        if (websocket && websocket.readyState === WebSocket.OPEN) {
            websocket.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket not connected.');
            appendMessage('Cannot send message, connection not established.', 'system-error');
        }
    }

    // --- WebSocket Initialization ---
    // 初始化WebSocket连接
    function initWebSocket() {
        websocket = new WebSocket(`ws://${location.host}/ws`);
        websocket.onopen = () => {
            console.log('WebSocket connected');
            appendMessage('Connected to device.', 'system');
        };
        websocket.onmessage = (event) => {
            console.log('Message from ESP32:', event.data);
            try {
                const data = JSON.parse(event.data);
                // 处理不同类型的消息
                if (data.type === 'chat_message' && data.sender === 'bot') {
                    appendMessage(data.text, 'ai');
                } else if (data.type === 'tool_execution_result') {
                    appendMessage(`Tool '${data.tool_name}' executed. Result: ${data.result}`, 'system');
                } else if (data.type === 'config_update_status') {
                    if (data.status === 'success') {
                        appendMessage(data.message, 'system');
                        settingsSidebar.classList.remove('open'); // Close sidebar on success
                        loadSettings(); // Reload settings to reflect changes
                    } else {
                        appendMessage(`Error: ${data.message}`, 'system-error');
                    }
                    // Remove loading indicator here if one was added
                }
                else {
                    appendMessage(event.data, 'ai');
                }
            } catch (e) {
                appendMessage(event.data, 'ai');
            }
        };
        websocket.onerror = (event) => {
            console.error('WebSocket error:', event);
            appendMessage('WebSocket connection error.', 'system-error');
        };
        websocket.onclose = () => {
            console.log('WebSocket closed');
            appendMessage('Connection disconnected.', 'system-error');
        };
    }

    // --- Settings Panel Logic ---
    // 更新LLM相关UI
    function updateLLMUI() {
        const providers = Object.keys(currentConfig.llm_providers || {});
        llmProviderSelect.innerHTML = providers.map(p => `<option value="${p}">${p}</option>`).join('');
        
        const selectedProvider = currentConfig.last_used.llm_provider;
        if (selectedProvider) {
            llmProviderSelect.value = selectedProvider;
            updateModelUI();
            llmApiKeyInput.value = currentConfig.llm_providers[selectedProvider].api_key || '';
        }
    }

    // 更新模型选择UI
    function updateModelUI() {
        const selectedProvider = llmProviderSelect.value;
        const models = currentConfig.llm_providers[selectedProvider]?.models || [];
        llmModelSelect.innerHTML = models.map(m => `<option value="${m}">${m}</option>`).join('');
        
        const selectedModel = currentConfig.last_used.model;
        if (models.includes(selectedModel)) {
            llmModelSelect.value = selectedModel;
        }
    }

    // 更新WiFi网络选择UI
    function updateWiFiUI() {
        const networks = currentConfig.wifi_networks || [];
        wifiNetworkSelect.innerHTML = networks.map(n => `<option value="${n.ssid}">${n.ssid}</option>`).join('');
        
        const lastUsedSSID = currentConfig.last_used.wifi_ssid;
        if (lastUsedSSID) {
            wifiNetworkSelect.value = lastUsedSSID;
        }
    }

    // 加载设备设置
    async function loadSettings() {
        try {
            const response = await fetch('/api/config');
            if (!response.ok) throw new Error('Failed to fetch config');
            currentConfig = await response.json();
            console.log('Settings loaded:', currentConfig);
            updateLLMUI();
            updateWiFiUI();
        } catch (error) {
            console.error('Error loading settings:', error);
            appendMessage('Could not load device settings.', 'system-error');
        }
    }

    // 保存设置到设备
    async function saveSettings() {
        // Update currentConfig from UI before saving
        const selectedProvider = llmProviderSelect.value;
        currentConfig.last_used.llm_provider = selectedProvider;
        currentConfig.last_used.model = llmModelSelect.value;
        if (currentConfig.llm_providers[selectedProvider]) {
            currentConfig.llm_providers[selectedProvider].api_key = llmApiKeyInput.value.trim();
        }

        try {
            appendMessage('Saving settings to device...', 'system'); // Add loading indicator
            const response = await fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentConfig)
            });
            const result = await response.json();
            if (result.status === 'success') {
                // The actual success message and sidebar closing will be handled by WebSocket 'config_update_status'
                // appendMessage('Settings update initiated.', 'system');
            } else {
                throw new Error(result.message);
            }
        } catch (error) {
            console.error('Error saving settings:', error);
            appendMessage(`Error saving settings: ${error.message}`, 'system-error');
        }
    }

    // --- UI Logic ---
    // 发送消息到聊天窗口和设备
    function sendMessage() {
        const messageText = messageInput.value.trim();
        if (messageText) {
            appendMessage(messageText, 'user');
            sendToESP32({ type: 'chat_message', payload: messageText });
            messageInput.value = '';
            messageInput.style.height = 'auto';
        }
    }

    // --- Event Listeners ---
    // 对话模式按钮事件
    dialogueModeBtn.addEventListener('click', () => {
        dialogueModeBtn.classList.add('active');
        advancedModeBtn.classList.remove('active');
        advancedModePanels.classList.remove('active'); // Remove active class
        sendToESP32({ type: 'set_llm_mode', mode: 'chat' });
    });

    // 高级模式按钮事件
    advancedModeBtn.addEventListener('click', () => {
        advancedModeBtn.classList.add('active');
        dialogueModeBtn.classList.remove('active');
        advancedModePanels.classList.add('active'); // Add active class
        sendToESP32({ type: 'set_llm_mode', mode: 'advanced' });
    });

    // 打开设置侧边栏
    settingsBtn.addEventListener('click', () => {
        loadSettings(); // Refresh settings every time the panel is opened
        settingsSidebar.classList.add('open');
    });

    // 关闭设置侧边栏
    closeSidebarBtn.addEventListener('click', () => {
        settingsSidebar.classList.remove('open');
    });

    // 输入框自适应高度
    messageInput.addEventListener('input', () => {
        messageInput.style.height = 'auto';
        messageInput.style.height = `${messageInput.scrollHeight}px`;
    });

    // 发送按钮事件
    sendBtn.addEventListener('click', sendMessage);
    // 回车发送消息（Shift+Enter换行）
    messageInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter' && !e.shiftKey) {
            e.preventDefault();
            sendMessage();
        }
    });

    // --- New Settings Panel Event Listeners ---
    // LLM提供商切换事件
    llmProviderSelect.addEventListener('change', () => {
        const selectedProvider = llmProviderSelect.value;
        llmApiKeyInput.value = currentConfig.llm_providers[selectedProvider]?.api_key || '';
        updateModelUI();
    });

    // 保存设置按钮事件
    saveSettingsBtn.addEventListener('click', saveSettings);

    // 添加WiFi网络事件
    wifiAddBtn.addEventListener('click', () => {
        const ssid = wifiSsidInput.value.trim();
        const password = wifiPasswordInput.value.trim();
        if (!ssid) {
            alert('Please enter a WiFi SSID.');
            return;
        }

        const existingNetwork = currentConfig.wifi_networks.find(n => n.ssid === ssid);
        if (existingNetwork) {
            existingNetwork.password = password;
        } else {
            currentConfig.wifi_networks.push({ ssid, password });
        }
        
        wifiSsidInput.value = '';
        wifiPasswordInput.value = '';
        updateWiFiUI();
        appendMessage(`WiFi network '${ssid}' saved locally. Click "Save & Apply" to send to device.`, 'system');
    });

    // 连接WiFi事件
    wifiConnectBtn.addEventListener('click', async () => {
        const ssid = wifiNetworkSelect.value;
        if (!ssid) return;
        try {
            const response = await fetch('/api/wifi/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: `ssid=${encodeURIComponent(ssid)}`
            });
            const result = await response.json();
            appendMessage(result.message, result.status === 'success' ? 'system' : 'system-error');
        } catch (error) {
            appendMessage(`Error connecting to WiFi: ${error.message}`, 'system-error');
        }
    });

    // 断开WiFi事件
    wifiDisconnectBtn.addEventListener('click', async () => {
        try {
            const response = await fetch('/api/wifi/disconnect', { method: 'POST' });
            const result = await response.json();
            appendMessage(result.message, result.status === 'success' ? 'system' : 'system-error');
        } catch (error) {
            appendMessage(`Error disconnecting WiFi: ${error.message}`, 'system-error');
        }
    });

    // 删除WiFi网络事件
    wifiDeleteBtn.addEventListener('click', () => {
        const ssid = wifiNetworkSelect.value;
        if (!ssid) return;

        currentConfig.wifi_networks = currentConfig.wifi_networks.filter(n => n.ssid !== ssid);
        updateWiFiUI();
        appendMessage(`WiFi network '${ssid}' deleted locally. Click "Save & Apply" to send to device.`, 'system');
    });

    // --- Initialization ---
    // 页面初始化，显示欢迎信息并尝试连接设备
    appendMessage('Welcome to NOOX! Attempting to connect to device...', 'system');
    initWebSocket();
});
