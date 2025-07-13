#include "task_manager.h"
#include "hid_manager.h"
#include "wifi_manager.h"
#include "hardware_manager.h" // Include HardwareManager header
#include "ble_manager.h" // Include BLEManager header
#include "timer_manager.h" // Include TimerManager header

// 初始化静态实例
TaskManager* TaskManager::_instance = nullptr;

TaskManager::TaskManager(HIDManager& hid, AppWiFiManager& wifi, HardwareManager& hw, TimerManager& timer, BLEManager& ble, SDManager& sd)
    : hidManager(hid), wifiManager(wifi), hardwareManager(hw), timerManager(timer), bleManager(ble), sdManager(sd) {
    _instance = this; // 设置静态实例
}

bool TaskManager::isTimerRunning() {
    return timerManager.isTimerRunning();
}

void IRAM_ATTR TaskManager::onTimerTaskCallback() {
    if (_instance) {
        _instance->_handleTimerCallback();
    }
}

void TaskManager::_handleTimerCallback() {
    Serial.println("Timer callback triggered from TaskManager!");
    // 可以在此处通过 WebSocket 向 UI 发送消息或触发其他任务
    // 示例：webManager.broadcast("{\"type\":\"timer_event\", \"status\":\"triggered\"}");
    // 注意：此处无法直接访问 webManager，需要其他回调机制或全局访问。
}

String TaskManager::executeTool(const String& toolName, const JsonObject& params) {
    Serial.println("Executing LLM tool: " + toolName);
    String result = "Error: Unknown tool or invalid parameters.";

    if (toolName == "usb_hid_keyboard_type") {
    if (params["text"].is<String>()) {
            String text = params["text"].as<String>();
            hidManager.sendString(text);
            result = "Success: Typed '" + text + "'";
        } else {
            result = "Error: Missing 'text' parameter for usb_hid_keyboard_type.";
        }
    } else if (toolName == "usb_hid_mouse_click") {
        if (params["button"].is<const char*>()) {
            String buttonStr = String(params["button"].as<const char*>()); // Explicit conversion
            int button = 0;
            if (buttonStr == "left") button = 1;
            else if (buttonStr == "right") button = 2;
            else if (buttonStr == "middle") button = 4;
            
            if (button != 0) {
                hidManager.clickMouse(button);
                result = "Success: Clicked mouse button " + buttonStr;
            } else {
                result = "Error: Invalid 'button' parameter for usb_hid_mouse_click. Use 'left', 'right', or 'middle'.";
            }
        } else {
            result = "Error: Missing 'button' parameter for usb_hid_mouse_click.";
        }
    } else if (toolName == "usb_hid_mouse_move") {
        if (params["x"].is<int>() && params["y"].is<int>()) {
            int x = params["x"].as<int>();
            int y = params["y"].as<int>();
            hidManager.moveMouse(x, y);
            result = "Success: Moved mouse by " + String(x) + "," + String(y);
        } else {
            result = "Error: Missing 'x' or 'y' parameters for usb_hid_mouse_move.";
        }
    } else if (toolName == "wifi_killer_scan") {
        wifiManager.startWifiKillerMode(); // Assuming this initiates a scan or relevant action
        result = "Success: Initiated Wi-Fi Killer scan.";
    } else if (toolName == "timer_set_countdown") {
        if (params["duration_ms"].is<long>()) {
            long durationMs = params["duration_ms"].as<long>();
            timerManager.setTimer(durationMs, TaskManager::onTimerTaskCallback);
            timerManager.startTimer();
            result = "Success: Set and started countdown timer for " + String(durationMs) + "ms.";
        } else {
            result = "Error: Missing 'duration_ms' parameter for timer_set_countdown.";
        }
    } else if (toolName == "gpio_set_level") {
        if (params["pin"].is<int>() && params["level"].is<int>()) {
            int pin = params["pin"].as<int>();
            int level = params["level"].as<int>();
            if (pin == 1) {
                hardwareManager.setReservedGpio1State(level == 1 ? HIGH : LOW);
                result = "Success: Set Reserved GPIO 1 to " + String(level);
            } else if (pin == 2) {
                hardwareManager.setReservedGpio2State(level == 1 ? HIGH : LOW);
                result = "Success: Set Reserved GPIO 2 to " + String(level);
            } else {
                result = "Error: Invalid 'pin' for gpio_set_level. Only 1 or 2 are supported.";
            }
        } else {
            result = "Error: Missing 'pin' or 'level' parameters for gpio_set_level.";
        }
    } else if (toolName == "ble_scan_devices") {
        bleManager.startScan(5); // 5 seconds scan
        result = "Success: Initiated BLE device scan.";
    } else if (toolName == "run_automation_script") { // Changed from startsWith to exact match for the tool name
        if (params["script_name"].is<const char*>()) {
            String scriptName = String(params["script_name"].as<const char*>()); // Explicit conversion
            String scriptPath = "/scripts/" + scriptName + ".json"; // Assuming scripts are JSON files in /scripts/
            Serial.printf("Attempting to run automation script: %s from path %s\n", scriptName.c_str(), scriptPath.c_str());

            String scriptContent = sdManager.readFile(scriptPath);
            if (scriptContent.length() == 0) {
                result = "Error: Automation script '" + scriptName + "' not found or empty.";
                Serial.println(result);
                return result;
            }

            JsonDocument scriptDoc; // Adjust size as needed for larger scripts
            DeserializationError scriptError = deserializeJson(scriptDoc, scriptContent);

            if (scriptError) {
                result = "Error: Failed to parse automation script '" + scriptName + "': " + scriptError.c_str();
                Serial.println(result);
                return result;
            }

            if (scriptDoc["steps"].is<JsonArray>()) {
                JsonArray steps = scriptDoc["steps"].as<JsonArray>();
                Serial.printf("Executing %u steps in script '%s'\n", steps.size(), scriptName.c_str());
                String stepResult = "Success"; // Aggregate result for all steps

                for (JsonVariant stepVariant : steps) {
                    JsonObject step = stepVariant.as<JsonObject>();
                    if (step["tool_name"].is<const char*>() && step["parameters"].is<JsonObject>()) {
                        String stepToolName = String(step["tool_name"].as<const char*>()); // Explicit conversion
                        JsonObject stepParams = step["parameters"].as<JsonObject>();
                        Serial.printf("  Executing step tool: %s\n", stepToolName.c_str());
                        String currentStepResult = executeTool(stepToolName, stepParams); // Recursive call for nested tools
                        if (currentStepResult.startsWith("Error:")) {
                            stepResult = "Error: Step '" + stepToolName + "' failed: " + currentStepResult;
                            Serial.println(stepResult);
                            break; // Stop execution on first error
                        }
                    } else {
                        stepResult = "Error: Invalid step format in script '" + scriptName + "'. Missing 'tool_name' or 'parameters'.";
                        Serial.println(stepResult);
                        break;
                    }
                }
                result = "Success: Automation script '" + scriptName + "' executed. Overall result: " + stepResult;
            } else {
                result = "Error: Automation script '" + scriptName + "' has no 'steps' array.";
                Serial.println(result);
            }
        } else {
            result = "Error: Missing 'script_name' parameter for run_automation_script.";
            Serial.println(result);
        }
    } else {
        result = "Error: Tool '" + toolName + "' not recognized or not implemented.";
        Serial.println(result);
    }
    return result;
}

// Keep executeTask for compatibility if needed by other parts of the system
void TaskManager::executeTask(const String& taskName, const String& params) {
    Serial.println("Executing legacy task: " + taskName + " with params: " + params);
    // This function can remain for compatibility or be refactored to use executeTool
    // For now, it will just log and not perform any action to avoid conflicts with executeTool
    // You might want to map these to executeTool calls if they are still needed.
    Serial.println("Note: Legacy executeTask called. Consider refactoring to use executeTool.");
}

void TaskManager::handleHIDTask(const String& params) {
    // This function is now deprecated in favor of executeTool
    Serial.println("Warning: handleHIDTask called. Use executeTool for HID operations.");
}

void TaskManager::handleWiFiTask(const String& params) {
    // This function is now deprecated in favor of executeTool
    Serial.println("Warning: handleWiFiTask called. Use executeTool for WiFi operations.");
}

void TaskManager::handleBluetoothTask(const String& params) {
    // This function is now deprecated in favor of executeTool
    Serial.println("Warning: handleBluetoothTask called. Use executeTool for Bluetooth operations.");
}

void TaskManager::handleTimerTask(const String& params) {
    // This function is now deprecated in favor of executeTool
    Serial.println("Warning: handleTimerTask called. Use executeTool for Timer operations.");
}

void TaskManager::handleGpioTask(const String& params) {
    // This function is now deprecated in favor of executeTool
    Serial.println("Warning: handleGpioTask called. Use executeTool for GPIO operations.");
}

void TaskManager::handleWifiKillerTask(const String& params) {
    // This function is now deprecated in favor of executeTool
    Serial.println("Warning: handleWifiKillerTask called. Use executeTool for Wi-Fi Killer operations.");
}
