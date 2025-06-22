document.addEventListener('DOMContentLoaded', () => {
    const chatMessages = document.getElementById('chatMessages');
    const messageInput = document.getElementById('messageInput');
    const sendButton = document.getElementById('sendButton');

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
                // Example: Add a button dynamically for confirmation
                const confirmBtn = document.createElement('button');
                confirmBtn.textContent = 'Confirm Action';
                confirmBtn.onclick = () => {
                    socket.send('AuthConfirm:true');
                    confirmBtn.remove(); // Remove button after click
                };
                chatMessages.appendChild(confirmBtn);
                chatMessages.scrollTop = chatMessages.scrollHeight; // Scroll to bottom
            } else if (messageData.startsWith('AuthDenied:')) {
                appendMessage('System', 'Action Denied by User.', 'system');
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
                socket.send(`chat:${message}`); // Prefix with 'chat:' for ESP32 parsing
            } else {
                appendMessage('System', 'WebSocket not connected. Message not sent.', 'error');
            }
            messageInput.value = ''; // Clear input
        }
    }

    // Initial WebSocket connection
    connectWebSocket();
});
