document.addEventListener('DOMContentLoaded', () => {
    const chatMessages = document.getElementById('chatMessages');
    const messageInput = document.getElementById('messageInput');
    const sendButton = document.getElementById('sendButton');
    const modeSelect = document.getElementById('modeSelect');
    const advancedControls = document.getElementById('advancedControls');
    const taskStatusDisplay = document.getElementById('taskStatusDisplay');
    const authConfirmationArea = document.getElementById('authConfirmationArea');

    // Determine WebSocket URL based on current host
    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsPort = 81; // WebSocket port defined in main.cpp
    const wsUrl = `${wsProtocol}//${window.location.hostname}:${wsPort}`;

    let socket;

    function connectWebSocket() {
        socket = new WebSocket(wsUrl);

        socket.onopen = (event) => {
            console.log('WebSocket connected:', event);
            appendMessage('System', 'Connected to ESP32-S3.', 'system');
        };

        socket.onmessage = (event) => {
            console.log('Message from server:', event.data);
            const messageData = event.data;
            if (messageData.startsWith('Gemini:')) {
                const geminiResponse = messageData.substring(7);
                appendMessage('Gemini', geminiResponse, 'gemini');
            } else if (messageData.startsWith('AuthRequest:')) {
                // Handle authorization request from ESP32
                const authMessage = messageData.substring(12);
                appendMessage('System', `Authorization Required: ${authMessage}`, 'system');
                // Optionally, display a confirmation dialog/button on the web page
                // For now, just log it and inform the user.
                console.warn('High-risk operation requires authorization!');
                // You might add a button here for user to click to send 'AuthConfirm'
                appendMessage('System', 'Click "Confirm" button on device or here to authorize.', 'system');
                displayAuthConfirmation(authMessage);
            } else if (messageData.startsWith('AuthDenied:')) {
                appendMessage('System', 'Action Denied by User.', 'system');
                clearAuthConfirmation();
            } else if (messageData.startsWith('System:Switched to')) {
                appendMessage('System', messageData.substring(7), 'system');
                // Update mode select dropdown based on device's actual mode
                if (messageData.includes("Chat Mode")) {
                    modeSelect.value = 'chat';
                } else if (messageData.includes("Advanced Mode")) {
                    modeSelect.value = 'advanced';
                }
            } else if (messageData.startsWith('TaskStatus:')) {
                const statusMessage = messageData.substring(11);
                taskStatusDisplay.textContent = `Task Status: ${statusMessage}`;
                appendMessage('System', statusMessage, 'system');
            } else if (messageData.startsWith('System:Task completed.')) {
                taskStatusDisplay.textContent = 'Task Status: Completed';
                appendMessage('System', 'Task completed.', 'system');
            }
            else {
                appendMessage('System', messageData, 'system');
            }
        };

        socket.onclose = (event) => {
            console.log('WebSocket disconnected:', event);
            appendMessage('System', 'Disconnected from ESP32-S3. Reconnecting...', 'system');
            setTimeout(connectWebSocket, 3000); // Attempt to reconnect after 3 seconds
        };

        socket.onerror = (error) => {
            console.error('WebSocket error:', error);
            appendMessage('System', 'WebSocket error. Check console for details.', 'error');
        };
    }

    function appendMessage(sender, text, type) {
        const messageElement = document.createElement('div');
        messageElement.classList.add('message', type);
        messageElement.innerHTML = `<strong>${sender}:</strong> ${text}`;
        chatMessages.appendChild(messageElement);
        chatMessages.scrollTop = chatMessages.scrollHeight; // Scroll to the bottom
    }

    function displayAuthConfirmation(message) {
        authConfirmationArea.innerHTML = `
            <p><strong>Authorization Required:</strong> ${message}</p>
            <button id="confirmAuthBtn">Confirm</button>
            <button id="denyAuthBtn">Deny</button>
        `;
        document.getElementById('confirmAuthBtn').onclick = () => {
            socket.send('AuthConfirm:true');
            clearAuthConfirmation();
        };
        document.getElementById('denyAuthBtn').onclick = () => {
            socket.send('AuthConfirm:false');
            clearAuthConfirmation();
        };
        authConfirmationArea.style.display = 'block';
    }

    function clearAuthConfirmation() {
        authConfirmationArea.innerHTML = '';
        authConfirmationArea.style.display = 'none';
    }

    sendButton.addEventListener('click', () => {
        sendMessage();
    });

    messageInput.addEventListener('keypress', (event) => {
        if (event.key === 'Enter') {
            sendMessage();
        }
    });

    function sendMessage() {
        const message = messageInput.value.trim();
        if (message) {
            appendMessage('You', message, 'user');
            if (socket && socket.readyState === WebSocket.OPEN) {
                if (modeSelect.value === 'chat') {
                    socket.send(`chat:${message}`); // Prefix with 'chat:' for ESP32 parsing
                } else if (modeSelect.value === 'advanced') {
                    // In advanced mode, assume user input is a task for Gemini
                    // We'll need to instruct the user to provide JSON or have Gemini generate it
                    appendMessage('System', 'In Advanced Mode, please provide a structured task for Gemini (e.g., JSON).', 'info');
                    // For now, we'll just send it as a generic advanced command
                    socket.send(`advanced:${message}`);
                }
            } else {
                appendMessage('System', 'WebSocket not connected. Message not sent.', 'error');
            }
            messageInput.value = ''; // Clear input
        }
    }

    modeSelect.addEventListener('change', () => {
        const selectedMode = modeSelect.value;
        if (socket && socket.readyState === WebSocket.OPEN) {
            socket.send(`SetMode:${selectedMode}`);
            if (selectedMode === 'advanced') {
                advancedControls.style.display = 'block';
            } else {
                advancedControls.style.display = 'none';
                taskStatusDisplay.textContent = 'Task Status: N/A';
            }
        } else {
            appendMessage('System', 'WebSocket not connected. Cannot switch mode.', 'error');
            // Revert dropdown if not connected
            modeSelect.value = currentMode === CHAT_MODE ? 'chat' : 'advanced';
        }
    });

    // Initial WebSocket connection
    connectWebSocket();
});
