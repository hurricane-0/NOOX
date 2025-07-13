document.addEventListener('DOMContentLoaded', () => {
    const dialogueModeBtn = document.getElementById('dialogue-mode-btn');
    const advancedModeBtn = document.getElementById('advanced-mode-btn');
    const advancedModePanels = document.getElementById('advanced-mode-panels');
    const settingsBtn = document.getElementById('settings-btn');
    const settingsSidebar = document.getElementById('settings-sidebar');
    const closeSidebarBtn = settingsSidebar.querySelector('.close-sidebar-btn');
    const messageInput = document.getElementById('message-input');
    const sendBtn = document.getElementById('send-btn');
    const messagesContainer = document.getElementById('messages');
    const autoToggleBtns = document.querySelectorAll('.auto-btn, .auto-toggle-btn');

    // Mode Switching
    dialogueModeBtn.addEventListener('click', () => {
        dialogueModeBtn.classList.add('active');
        advancedModeBtn.classList.remove('active');
        advancedModePanels.style.display = 'none';
    });

    advancedModeBtn.addEventListener('click', () => {
        advancedModeBtn.classList.add('active');
        dialogueModeBtn.classList.remove('active');
        advancedModePanels.style.display = 'flex';
    });

    // Settings Sidebar
    settingsBtn.addEventListener('click', () => {
        settingsSidebar.classList.add('open');
    });

    closeSidebarBtn.addEventListener('click', () => {
        settingsSidebar.classList.remove('open');
    });

    // Close sidebar when clicking outside (optional, but good UX)
    // document.addEventListener('click', (event) => {
    //     if (settingsSidebar.classList.contains('open') && !settingsSidebar.contains(event.target) && !settingsBtn.contains(event.target)) {
    //         settingsSidebar.classList.remove('open');
    //     }
    // });

    // Input Box Auto-resize
    messageInput.addEventListener('input', () => {
        messageInput.style.height = 'auto';
        messageInput.style.height = messageInput.scrollHeight + 'px';
    });

    // Send Message (for demonstration)
    sendBtn.addEventListener('click', () => {
        const message = messageInput.value.trim();
        if (message) {
            appendMessage(message, 'user');
            messageInput.value = '';
            messageInput.style.height = 'auto'; // Reset height after sending
            // Simulate a response
            setTimeout(() => {
                appendMessage('这是来自AIHi的回复：' + message, 'ai');
            }, 500);
        }
    });

    // Allow sending with Enter key
    messageInput.addEventListener('keypress', (event) => {
        if (event.key === 'Enter' && !event.shiftKey) {
            event.preventDefault(); // Prevent new line
            sendBtn.click();
        }
    });

    function appendMessage(text, sender) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', sender);
        messageElement.textContent = text;
        messagesContainer.appendChild(messageElement);
        messagesContainer.scrollTop = messagesContainer.scrollHeight; // Scroll to bottom
    }

    // Placeholder for WebSocket communication
    // const websocket = new WebSocket('ws://your-esp32-ip/ws'); // Replace with your ESP32 IP

    // websocket.onopen = (event) => {
    //     console.log('WebSocket connected:', event);
    // };

    // websocket.onmessage = (event) => {
    //     console.log('Message from ESP32:', event.data);
    //     // Handle incoming messages (e.g., append to chat, update settings)
    //     appendMessage(event.data, 'ai'); // Example: treat all incoming as AI messages
    // };

    // websocket.onerror = (event) => {
    //     console.error('WebSocket error:', event);
    // };

    // websocket.onclose = (event) => {
    //     console.log('WebSocket closed:', event);
    // };

    // Example function to send data via WebSocket
    // function sendToESP32(data) {
    //     if (websocket.readyState === WebSocket.OPEN) {
    //         websocket.send(data);
    //     } else {
    //         console.warn('WebSocket not open. Cannot send data.');
    //     }
    // }

    // Toggle active state for AUTO buttons
    autoToggleBtns.forEach(btn => {
        btn.addEventListener('click', () => {
            btn.classList.toggle('active');
        });
    });

    // Settings functionality (basic placeholders)
    const addWifiBtn = document.getElementById('add-wifi-btn');
    const newWifiNameInput = document.getElementById('new-wifi-name');
    const newWifiPasswordInput = document.getElementById('new-wifi-password');
    const wifiListContainer = document.getElementById('wifi-list');
    const saveSettingsBtn = document.getElementById('save-settings-btn');

    addWifiBtn.addEventListener('click', () => {
        const name = newWifiNameInput.value.trim();
        const password = newWifiPasswordInput.value.trim();
        if (name && password) {
            const wifiEntry = document.createElement('div');
            wifiEntry.classList.add('wifi-entry');
            wifiEntry.innerHTML = `
                <span>名称: ${name}</span>
                <span>密码: ${'*'.repeat(password.length)}</span>
                <button class="delete-wifi-btn">🗑️</button>
            `;
            wifiListContainer.appendChild(wifiEntry);
            newWifiNameInput.value = '';
            newWifiPasswordInput.value = '';

            wifiEntry.querySelector('.delete-wifi-btn').addEventListener('click', (e) => {
                e.target.closest('.wifi-entry').remove();
            });
        }
    });

    saveSettingsBtn.addEventListener('click', () => {
        alert('设置已保存 (功能待实现)');
        // In a real application, you would send these settings to the ESP32 via WebSocket or HTTP POST
        // Example: sendToESP32(JSON.stringify({ type: 'settings', wifi: getWifiList(), llm: getLlmSettings() }));
        settingsSidebar.classList.remove('open');
    });

    // Initial message for chat area
    appendMessage('欢迎使用 AIHi！请开始您的对话。', 'ai');
});
