document.addEventListener('DOMContentLoaded', () => {
    // --- Global State ---
    let websocket; // WebSocket连接对象
    let currentConfig = {}; // 当前配置数据
    let loadingMessageElement = null; // 加载消息元素

    // Configure marked.js for markdown rendering
    if (typeof marked !== 'undefined') {
        marked.setOptions({
            breaks: true,
            gfm: true
        });
    }

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
    const toastContainer = document.getElementById('toast-container');

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
    
    // Toast notification system
    function showToast(message, type = 'info', duration = 4000) {
        const toast = document.createElement('div');
        toast.classList.add('toast', type);
        
        const icon = document.createElement('span');
        icon.classList.add('toast-icon');
        icon.textContent = type === 'error' ? '✕' : type === 'success' ? '✓' : 'ℹ';
        
        const messageSpan = document.createElement('span');
        messageSpan.classList.add('toast-message');
        messageSpan.textContent = message;
        
        const closeBtn = document.createElement('button');
        closeBtn.classList.add('toast-close');
        closeBtn.textContent = '×';
        closeBtn.onclick = () => removeToast(toast);
        
        toast.appendChild(icon);
        toast.appendChild(messageSpan);
        toast.appendChild(closeBtn);
        
        toastContainer.appendChild(toast);
        
        if (duration > 0) {
            setTimeout(() => removeToast(toast), duration);
        }
    }
    
    function removeToast(toast) {
        toast.classList.add('hiding');
        setTimeout(() => {
            if (toast.parentElement) {
                toast.parentElement.removeChild(toast);
            }
        }, 300);
    }

    // Show loading indicator
    function showLoadingMessage() {
        if (loadingMessageElement) return; // 避免重复显示
        
        loadingMessageElement = document.createElement('div');
        loadingMessageElement.classList.add('message', 'loading');
        loadingMessageElement.textContent = 'thinking...';
        
        messagesContainer.appendChild(loadingMessageElement);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    // Remove loading indicator
    function removeLoadingMessage() {
        if (loadingMessageElement && loadingMessageElement.parentElement) {
            loadingMessageElement.parentElement.removeChild(loadingMessageElement);
            loadingMessageElement = null;
        }
    }
    
    // 添加消息到聊天窗口
    function appendMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        
        // For AI messages, render markdown
        if (sender === 'ai' && typeof marked !== 'undefined') {
            const contentDiv = document.createElement('div');
            contentDiv.innerHTML = marked.parse(text);
            messageElement.appendChild(contentDiv);
            
            // Add pulse animation for AI responses
            messageElement.classList.add('pulse');
            setTimeout(() => {
                messageElement.classList.remove('pulse');
            }, 1500);
        } else {
        const textNode = document.createTextNode(text);
        messageElement.appendChild(textNode);
        }

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
            showToast('无法发送消息，未连接到设备', 'error');
        }
    }

    // --- WebSocket Initialization ---
    // 初始化WebSocket连接
    function initWebSocket() {
        websocket = new WebSocket(`ws://${location.host}/ws`);
        websocket.onopen = () => {
            console.log('WebSocket connected');
            appendMessage('已连接到设备', 'system');
            showToast('设备连接成功', 'success', 3000);
        };
        websocket.onmessage = (event) => {
            console.log('Message from ESP32:', event.data);
            
            // Remove loading indicator when receiving response
            removeLoadingMessage();
            
            try {
                const data = JSON.parse(event.data);
                // 处理不同类型的消息
                if (data.type === 'chat_message' && data.sender === 'bot') {
                    appendMessage(data.text, 'ai');
                } else if (data.type === 'tool_execution_result') {
                    appendMessage(`工具 '${data.tool_name}' 已执行。结果: ${data.result}`, 'system');
                } else if (data.type === 'config_update_status') {
                    if (data.status === 'success') {
                        appendMessage(data.message, 'system');
                        showToast('设置保存成功', 'success');
                        settingsSidebar.classList.remove('open');
                        loadSettings();
                    } else {
                        showToast(`错误: ${data.message}`, 'error');
                    }
                } else {
                    appendMessage(event.data, 'ai');
                }
            } catch (e) {
                appendMessage(event.data, 'ai');
            }
        };
        websocket.onerror = (event) => {
            console.error('WebSocket error:', event);
            showToast('WebSocket连接错误', 'error');
            removeLoadingMessage();
        };
        websocket.onclose = () => {
            console.log('WebSocket closed');
            showToast('设备连接已断开', 'error', 5000);
            removeLoadingMessage();
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
            showToast('无法加载设备设置', 'error');
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
            showToast('正在保存设置...', 'info', 2000);
            const response = await fetch('/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(currentConfig)
            });
            const result = await response.json();
            if (result.status !== 'success') {
                throw new Error(result.message);
            }
        } catch (error) {
            console.error('Error saving settings:', error);
            showToast(`保存设置失败: ${error.message}`, 'error');
        }
    }

    // --- UI Logic ---
    // 发送消息到聊天窗口和设备
    function sendMessage() {
        const messageText = messageInput.value.trim();
        if (messageText) {
            appendMessage(messageText, 'user');
            showLoadingMessage(); // Show thinking indicator
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
            showToast('请输入WiFi名称', 'error');
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
        showToast(`WiFi '${ssid}' 已保存，点击"保存并应用"同步到设备`, 'success');
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
            showToast(result.message, result.status === 'success' ? 'success' : 'error');
        } catch (error) {
            showToast(`连接WiFi失败: ${error.message}`, 'error');
        }
    });

    // 断开WiFi事件
    wifiDisconnectBtn.addEventListener('click', async () => {
        try {
            const response = await fetch('/api/wifi/disconnect', { method: 'POST' });
            const result = await response.json();
            showToast(result.message, result.status === 'success' ? 'success' : 'error');
        } catch (error) {
            showToast(`断开WiFi失败: ${error.message}`, 'error');
        }
    });

    // 删除WiFi网络事件
    wifiDeleteBtn.addEventListener('click', () => {
        const ssid = wifiNetworkSelect.value;
        if (!ssid) return;

        currentConfig.wifi_networks = currentConfig.wifi_networks.filter(n => n.ssid !== ssid);
        updateWiFiUI();
        showToast(`WiFi '${ssid}' 已删除，点击"保存并应用"同步到设备`, 'info');
    });

    // GPIO toggle event handlers
    const gpioToggles = document.querySelectorAll('.gpio-toggle');
    gpioToggles.forEach(toggle => {
        toggle.addEventListener('click', () => {
            const gpioNum = toggle.getAttribute('data-gpio');
            toggle.classList.toggle('active');
            const state = toggle.classList.contains('active');
            console.log(`GPIO ${gpioNum} toggled to ${state ? 'ON' : 'OFF'}`);
            showToast(`GPIO ${gpioNum} ${state ? '已开启' : '已关闭'}`, 'info', 2000);
            // Send GPIO state to device
            sendToESP32({ type: 'gpio_control', gpio: gpioNum, state: state });
        });
    });

    // --- Initialization ---
    // 页面初始化，显示欢迎信息并尝试连接设备
    appendMessage('欢迎使用 NOOX！正在连接设备...', 'system');
    initWebSocket();
});
