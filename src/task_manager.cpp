#include "task_manager.h"
#include "hid_manager.h"
#include "wifi_manager.h"
#include "hardware_manager.h" // 引入 HardwareManager 头文件

// 调整构造函数，移除 TimerManager, BLEManager 和 SDManager 引用
TaskManager::TaskManager(HIDManager& hid, AppWiFiManager& wifi, HardwareManager& hw)
    : hidManager(hid), wifiManager(wifi), hardwareManager(hw),
      currentLLMMode("Chat"), currentTaskStatus("Idle") { // Initialize member variables
}

// LLM 工具调用的新方法
String TaskManager::executeTool(const String& toolName, const JsonObject& params) {
    Serial.printf("Executing tool: %s\n", toolName.c_str());
    currentTaskStatus = "Executing: " + toolName; // Update task status for UI

    if (toolName.startsWith("usb_hid_")) {
        String result = _handleHidTool(toolName, params);
        currentTaskStatus = "Idle"; // Reset status after execution
        return result;
    } else if (toolName.startsWith("wifi_")) {
        // WiFi工具现在只用于WebSocket，不应有外部调用，但保留结构
        String result = _handleWiFiTool(toolName, params);
        currentTaskStatus = "Idle";
        return result;
    } else if (toolName.startsWith("gpio_")) {
        String result = _handleGpioTool(toolName, params);
        currentTaskStatus = "Idle";
        return result;
    }
    currentTaskStatus = "Error: Unknown Tool"; // Update status for unknown tool
    return "错误: 未知工具: " + toolName;
}


// 新增用于 UI 显示的方法实现
String TaskManager::getCurrentLLMMode() {
    return currentLLMMode;
}

String TaskManager::getCurrentTaskStatus() {
    return currentTaskStatus;
}

// Methods to update internal state (can be called by WebManager/LLMManager)
void TaskManager::setLLMMode(const String& mode) {
    currentLLMMode = mode;
}

void TaskManager::setTaskStatus(const String& status) {
    currentTaskStatus = status;
}

String TaskManager::_handleHidTool(const String& toolName, const JsonObject& params) {
    // ... (HID tool handling remains the same)
    if (toolName == "usb_hid_keyboard_type") {
        if (params["text"].is<String>()) {
            String text = params["text"].as<String>();
            hidManager.sendString(text);
            return "成功: 输入 '" + text + "'";
        } else {
            return "错误: usb_hid_keyboard_type 缺少 'text' 参数。";
        }
    } else if (toolName == "usb_hid_mouse_click") {
        if (params["button"].is<const char*>()) {
            String buttonStr = String(params["button"].as<const char*>());
            int button = 0;
            if (buttonStr == "left") button = 1;
            else if (buttonStr == "right") button = 2;
            else if (buttonStr == "middle") button = 4;
            
            if (button != 0) {
                hidManager.clickMouse(button);
                return "成功: 点击鼠标按钮 " + buttonStr;
            } else {
                return "错误: usb_hid_mouse_click 的 'button' 参数无效。请使用 'left'、'right' 或 'middle'。";
            }
        } else {
            return "错误: usb_hid_mouse_click 缺少 'button' 参数。";
        }
    } else if (toolName == "usb_hid_mouse_move") {
        if (params["x"].is<int>() && params["y"].is<int>()) {
            int x = params["x"].as<int>();
            int y = params["y"].as<int>();
            hidManager.moveMouse(x, y);
            return "成功: 鼠标移动 " + String(x) + "," + String(y);
        } else {
            return "错误: usb_hid_mouse_move 缺少 'x' 或 'y' 参数。";
        }
    }
    return "错误: 未识别的 HID 工具: " + toolName;
}

String TaskManager::_handleWiFiTool(const String& toolName, const JsonObject& params) {
    // Wi-Fi Killer 模式已移除，此函数现在应为空或仅处理 WebSocket 相关状态（如果需要）
    // 根据简化要求，Wi-Fi 仅用于 WebSocket 服务器，不应有外部工具调用
    return "错误: Wi-Fi 工具已移除或不支持此操作: " + toolName;
}


String TaskManager::_handleGpioTool(const String& toolName, const JsonObject& params) {
    // ... (GPIO tool handling remains the same)
    if (toolName == "gpio_set_level") {
        if (params["pin"].is<int>() && params["level"].is<int>()) {
            int pin = params["pin"].as<int>();
            int level = params["level"].as<int>();
            if (pin == 1) {
                hardwareManager.setReservedGpio1State(level == 1 ? HIGH : LOW);
                return "成功: 设置保留GPIO 1为 " + String(level);
            } else if (pin == 2) {
                hardwareManager.setReservedGpio2State(level == 1 ? HIGH : LOW);
                return "成功: 设置保留GPIO 2为 " + String(level);
            } else {
                return "错误: gpio_set_level 的 'pin' 参数无效。仅支持1或2。";
            }
        } else {
            return "错误: gpio_set_level 缺少 'pin' 或 'level' 参数。";
        }
    }
    return "错误: 未识别的 GPIO 工具: " + toolName;
}
