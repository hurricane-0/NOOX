#include "task_manager.h"
#include "hid_manager.h"
#include "wifi_manager.h"
#include "hardware_manager.h" // 引入 HardwareManager 头文件
#include "ble_manager.h" // 引入 BLEManager 头文件
#include "timer_manager.h" // 引入 TimerManager 头文件

// 初始化静态实例
TaskManager* TaskManager::_instance = nullptr;

TaskManager::TaskManager(HIDManager& hid, AppWiFiManager& wifi, HardwareManager& hw, TimerManager& timer, BLEManager& ble, SDManager& sd)
    : hidManager(hid), wifiManager(wifi), hardwareManager(hw), timerManager(timer), bleManager(ble), sdManager(sd) {
    _instance = this; // 设置静态实例
}

// LLM 工具调用的新方法
String TaskManager::executeTool(const String& toolName, const JsonObject& params) {
    Serial.printf("Executing tool: %s\n", toolName.c_str());
    if (toolName.startsWith("usb_hid_")) {
        return _handleHidTool(toolName, params);
    } else if (toolName.startsWith("wifi_")) {
        return _handleWiFiTool(toolName, params);
    } else if (toolName.startsWith("timer_")) {
        return _handleTimerTool(toolName, params);
    } else if (toolName.startsWith("gpio_")) {
        return _handleGpioTool(toolName, params);
    } else if (toolName.startsWith("ble_")) {
        return _handleBleTool(toolName, params);
    } else if (toolName.startsWith("run_automation_script")) {
        return _handleAutomationScriptTool(toolName, params);
    }
    return "错误: 未知工具: " + toolName;
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
    Serial.println("TaskManager定时器回调被触发!");
    // 可以在此处通过 WebSocket 向 UI 发送消息或触发其他任务
    // 示例：webManager.broadcast("{\"type\":\"timer_event\", \"status\":\"triggered\"}");
    // 注意：此处无法直接访问 webManager，需要其他回调机制或全局访问。
}


String TaskManager::_handleHidTool(const String& toolName, const JsonObject& params) {
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
    if (toolName == "wifi_killer_scan" || toolName == "wifi_killer_start") {
        wifiManager.startWifiKillerMode();
        return "成功: 已启动Wi-Fi Killer模式。";
    } else if (toolName == "wifi_killer_stop") {
        wifiManager.stopWifiKillerMode();
        return "成功: 已停止Wi-Fi Killer模式。";
    }
    return "错误: 未识别的 WiFi 工具: " + toolName;
}

String TaskManager::_handleTimerTool(const String& toolName, const JsonObject& params) {
    if (toolName == "timer_set") {
        if (params["duration"].is<long>()) { // Changed from duration_ms to duration
            long durationMs = params["duration"].as<long>();
            timerManager.setTimer(durationMs, TaskManager::onTimerTaskCallback);
            return "成功: 设置定时器为 " + String(durationMs) + "ms。";
        } else {
            return "错误: timer_set 缺少 'duration' 参数。";
        }
    } else if (toolName == "timer_start") {
        timerManager.startTimer();
        return "成功: 启动定时器。";
    } else if (toolName == "timer_stop") {
        timerManager.stopTimer();
        return "成功: 停止定时器。";
    }
    return "错误: 未识别的 Timer 工具: " + toolName;
}

String TaskManager::_handleGpioTool(const String& toolName, const JsonObject& params) {
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

String TaskManager::_handleBleTool(const String& toolName, const JsonObject& params) {
    if (toolName == "ble_scan_devices") {
        bleManager.startScan(5); // 扫描5秒
        return "成功: 已启动BLE设备扫描。";
    }
    return "错误: 未识别的 BLE 工具: " + toolName;
}

String TaskManager::_handleAutomationScriptTool(const String& toolName, const JsonObject& params) {
    if (toolName == "run_automation_script") {
        if (params["script_name"].is<const char*>()) {
            String scriptName = String(params["script_name"].as<const char*>());
            Serial.printf("尝试运行自动化脚本: %s\n", scriptName.c_str());

            JsonDocument scriptsDoc = sdManager.loadAutomationScripts();
            if (scriptsDoc.isNull() || !scriptsDoc["scripts"].is<JsonArray>()) {
                String result = "错误: 自动化脚本配置文件无效或没有脚本。";
                Serial.println(result);
                return result;
            }

            JsonArray scriptsArray = scriptsDoc["scripts"].as<JsonArray>();
            JsonObject scriptToExecute;
            bool scriptFound = false;
            for (JsonVariant v : scriptsArray) {
                JsonObject currentScript = v.as<JsonObject>();
                if (currentScript["name"] == scriptName) {
                    scriptToExecute = currentScript;
                    scriptFound = true;
                    break;
                }
            }

            if (!scriptFound) {
                String result = "错误: 自动化脚本 '" + scriptName + "' 未找到。";
                Serial.println(result);
                return result;
            }

            if (scriptToExecute["steps"].is<JsonArray>()) {
                JsonArray steps = scriptToExecute["steps"].as<JsonArray>();
                Serial.printf("执行脚本 '%s' 中的 %u 步\n", scriptName.c_str(), steps.size());
                String stepResult = "成功";

                for (JsonVariant stepVariant : steps) {
                    JsonObject step = stepVariant.as<JsonObject>();
                    if (step["tool_name"].is<const char*>() && step["parameters"].is<JsonObject>()) {
                        String stepToolName = String(step["tool_name"].as<const char*>());
                        JsonObject stepParams = step["parameters"].as<JsonObject>();
                        Serial.printf("  执行步骤工具: %s\n", stepToolName.c_str());
                        String currentStepResult = executeTool(stepToolName, stepParams);
                        if (currentStepResult.startsWith("错误:")) {
                            stepResult = "错误: 步骤 '" + stepToolName + "' 失败: " + currentStepResult;
                            Serial.println(stepResult);
                            break;
                        }
                    } else {
                        stepResult = "错误: 脚本 '" + scriptName + "' 步骤格式无效。缺少 'tool_name' 或 'parameters'。";
                        Serial.println(stepResult);
                        break;
                    }
                }
                return "成功: 自动化脚本 '" + scriptName + "' 执行完成。总体结果: " + stepResult;
            } else {
                String result = "错误: 自动化脚本 '" + scriptName + "' 没有 'steps' 数组。";
                Serial.println(result);
                return result;
            }
        } else {
            String result = "错误: run_automation_script 缺少 'script_name' 参数。";
            Serial.println(result);
            return result;
        }
    }
    return "错误: 未识别的自动化脚本工具: " + toolName;
}
