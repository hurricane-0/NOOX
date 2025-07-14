document.addEventListener('DOMContentLoaded', () => {
    // --- Element Selectors ---
    const dialogueModeBtn = document.getElementById('dialogue-mode-btn');
    const advancedModeBtn = document.getElementById('advanced-mode-btn');
    const advancedModePanels = document.getElementById('advanced-mode-panels');
    const settingsBtn = document.getElementById('settings-btn');
    const settingsSidebar = document.getElementById('settings-sidebar');
    const closeSidebarBtn = settingsSidebar.querySelector('.close-sidebar-btn');
    const messageInput = document.getElementById('message-input');
    const sendBtn = document.getElementById('send-btn');
    const messagesContainer = document.getElementById('messages');
    const functionItems = document.querySelectorAll('.function-item');
    
    // Settings Sidebar Elements
    const wifiSsidInput = document.getElementById('wifi-ssid');
    const wifiPasswordInput = document.getElementById('wifi-password');
    const apiKeyInput = document.getElementById('api-key');
    const llmSelect = document.getElementById('llm-select');
    const saveSettingsBtn = document.getElementById('save-settings-btn');

    let websocket;

    // --- Core Functions ---
    function appendMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        messageElement.textContent = text;
        messagesContainer.appendChild(messageElement);
        messagesContainer.scrollTop = messagesContainer.scrollHeight;
    }

    function sendToESP32(data) {
        if (websocket && websocket.readyState === WebSocket.OPEN) {
            websocket.send(JSON.stringify(data));
        } else {
            console.warn('WebSocket not open. Cannot send data.');
            appendMessage('无法发送消息，连接未建立。', 'system-error');
        }
    }

    // --- WebSocket Initialization ---
    function initWebSocket() {
        websocket = new WebSocket(`ws://${location.host}/ws`);

        websocket.onopen = () => {
            console.log('WebSocket connected');
            appendMessage('已连接到设备。', 'system');
        };

        websocket.onmessage = (event) => {
            console.log('Message from ESP32:', event.data);
            try {
                const data = JSON.parse(event.data);
                // The backend sends various message types, but for the chat UI,
                // we are primarily interested in 'chat_message' or raw text.
                if (data.type === 'chat_message' && data.sender === 'bot') {
                    appendMessage(data.text, 'ai');
                } else if (data.type === 'tool_execution_result') {
                    const resultText = `Tool '${data.tool_name}' executed. Result: ${data.result}`;
                    appendMessage(resultText, 'system');
                } else {
                     // Fallback for other JSON messages or plain text
                    appendMessage(event.data, 'ai');
                }
            } catch (e) {
                // If data is not JSON, display as plain text
                appendMessage(event.data, 'ai');
            }
        };

        websocket.onerror = (event) => {
            console.error('WebSocket error:', event);
            appendMessage('WebSocket 连接出错。', 'system-error');
        };

        websocket.onclose = () => {
            console.log('WebSocket closed');
            appendMessage('连接已断开。', 'system-error');
        };
    }

    // --- UI Logic ---
    function sendMessage() {
        const messageText = messageInput.value.trim();
        if (messageText) {
            appendMessage(messageText, 'user');
            sendToESP32({ type: 'chat_message', payload: messageText });
            messageInput.value = '';
            messageInput.style.height = 'auto';
        }
    }
    
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

    // --- Event Listeners ---
    dialogueModeBtn.addEventListener('click', () => {
        dialogueModeBtn.classList.add('active');
        advancedModeBtn.classList.remove('active');
        advancedModePanels.style.display = 'none';
        sendToESP32({ type: 'set_llm_mode', mode: 'chat' });
    });

    advancedModeBtn.addEventListener('click', () => {
        advancedModeBtn.classList.add('active');
        dialogueModeBtn.classList.remove('active');
        advancedModePanels.style.display = 'flex';
        sendToESP32({ type: 'set_llm_mode', mode: 'advanced' });
        updateAuthorizedTools(); // Send current tool authorizations
    });

    settingsBtn.addEventListener('click', () => {
        settingsSidebar.classList.add('open');
    });

    closeSidebarBtn.addEventListener('click', () => {
        settingsSidebar.classList.remove('open');
    });

    messageInput.addEventListener('input', () => {
        messageInput.style.height = 'auto';
        messageInput.style.height = `${messageInput.scrollHeight}px`;
    });

    sendBtn.addEventListener('click', sendMessage);

    messageInput.addEventListener('keypress', (event) => {
        if (event.key === 'Enter' && !event.shiftKey) {
            event.preventDefault();
            sendMessage();
        }
    });

    functionItems.forEach(item => {
        const autoBtn = item.querySelector('.auto-btn');
        autoBtn.addEventListener('click', () => {
            autoBtn.classList.toggle('active');
            // If in advanced mode, update authorizations immediately
            if (advancedModeBtn.classList.contains('active')) {
                updateAuthorizedTools();
            }
        });
    });

    saveSettingsBtn.addEventListener('click', () => {
        const ssid = wifiSsidInput.value.trim();
        const pass = wifiPasswordInput.value.trim();
        const apiKey = apiKeyInput.value.trim();

        if (!ssid || !pass || !apiKey) {
            alert('请填写所有网络和授权字段。');
            return;
        }

        const settingsToSave = {
            type: 'save_settings',
            payload: {
                ssid: ssid,
                pass: pass,
                apiKey: apiKey,
                llm_provider: llmSelect.value // Assuming backend handles this
            }
        };
        
        sendToESP32(settingsToSave);
        appendMessage('设置已发送至设备保存。', 'system');
        settingsSidebar.classList.remove('open');
    });

    // --- Initial Execution ---
    appendMessage('欢迎使用 AIHi！正在尝试连接设备...', 'system');
    initWebSocket();
});
