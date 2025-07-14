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

String TaskManager::executeTool(const String& toolName, const JsonObject& params) {
    Serial.println("正在执行LLM工具: " + toolName);
    String result = "错误: 未知工具或参数无效。";

    if (toolName == "usb_hid_keyboard_type") {
        if (params["text"].is<String>()) {
            String text = params["text"].as<String>();
            hidManager.sendString(text);
            result = "成功: 输入 '" + text + "'";
        } else {
            result = "错误: usb_hid_keyboard_type 缺少 'text' 参数。";
        }
    } else if (toolName == "usb_hid_mouse_click") {
        if (params["button"].is<const char*>()) {
            String buttonStr = String(params["button"].as<const char*>()); // 显式转换
            int button = 0;
            if (buttonStr == "left") button = 1;
            else if (buttonStr == "right") button = 2;
            else if (buttonStr == "middle") button = 4;
            
            if (button != 0) {
                hidManager.clickMouse(button);
                result = "成功: 点击鼠标按钮 " + buttonStr;
            } else {
                result = "错误: usb_hid_mouse_click 的 'button' 参数无效。请使用 'left'、'right' 或 'middle'。";
            }
        } else {
            result = "错误: usb_hid_mouse_click 缺少 'button' 参数。";
        }
    } else if (toolName == "usb_hid_mouse_move") {
        if (params["x"].is<int>() && params["y"].is<int>()) {
            int x = params["x"].as<int>();
            int y = params["y"].as<int>();
            hidManager.moveMouse(x, y);
            result = "成功: 鼠标移动 " + String(x) + "," + String(y);
        } else {
            result = "错误: usb_hid_mouse_move 缺少 'x' 或 'y' 参数。";
        }
    } else if (toolName == "wifi_killer_scan") {
        wifiManager.startWifiKillerMode(); // 假定此操作启动扫描或相关动作
        result = "成功: 已启动Wi-Fi Killer扫描。";
    } else if (toolName == "timer_set_countdown") {
        if (params["duration_ms"].is<long>()) {
            long durationMs = params["duration_ms"].as<long>();
            timerManager.setTimer(durationMs, TaskManager::onTimerTaskCallback);
            timerManager.startTimer();
            result = "成功: 设置并启动倒计时定时器 " + String(durationMs) + "ms。";
        } else {
            result = "错误: timer_set_countdown 缺少 'duration_ms' 参数。";
        }
    } else if (toolName == "gpio_set_level") {
        if (params["pin"].is<int>() && params["level"].is<int>()) {
            int pin = params["pin"].as<int>();
            int level = params["level"].as<int>();
            if (pin == 1) {
                hardwareManager.setReservedGpio1State(level == 1 ? HIGH : LOW);
                result = "成功: 设置保留GPIO 1为 " + String(level);
            } else if (pin == 2) {
                hardwareManager.setReservedGpio2State(level == 1 ? HIGH : LOW);
                result = "成功: 设置保留GPIO 2为 " + String(level);
            } else {
                result = "错误: gpio_set_level 的 'pin' 参数无效。仅支持1或2。";
            }
        } else {
            result = "错误: gpio_set_level 缺少 'pin' 或 'level' 参数。";
        }
    } else if (toolName == "ble_scan_devices") {
        bleManager.startScan(5); // 扫描5秒
        result = "成功: 已启动BLE设备扫描。";
    } else if (toolName == "run_automation_script") { // 工具名精确匹配
        if (params["script_name"].is<const char*>()) {
            String scriptName = String(params["script_name"].as<const char*>()); // 显式转换
            String scriptPath = "/scripts/" + scriptName + ".json"; // 假定脚本为JSON文件，存放于/scripts/
            Serial.printf("尝试运行自动化脚本: %s 路径 %s\n", scriptName.c_str(), scriptPath.c_str());

            String scriptContent = sdManager.readFile(scriptPath);
            if (scriptContent.length() == 0) {
                result = "错误: 自动化脚本 '" + scriptName + "' 未找到或为空。";
                Serial.println(result);
                return result;
            }

            JsonDocument scriptDoc; // 如需更大脚本可调整大小
            DeserializationError scriptError = deserializeJson(scriptDoc, scriptContent);

            if (scriptError) {
                result = "错误: 自动化脚本 '" + scriptName + "' 解析失败: " + scriptError.c_str();
                Serial.println(result);
                return result;
            }

            if (scriptDoc["steps"].is<JsonArray>()) {
                JsonArray steps = scriptDoc["steps"].as<JsonArray>();
                Serial.printf("执行脚本 '%s' 中的 %u 步\n", scriptName.c_str(), steps.size());
                String stepResult = "成功"; // 所有步骤的聚合结果

                for (JsonVariant stepVariant : steps) {
                    JsonObject step = stepVariant.as<JsonObject>();
                    if (step["tool_name"].is<const char*>() && step["parameters"].is<JsonObject>()) {
                        String stepToolName = String(step["tool_name"].as<const char*>()); // 显式转换
                        JsonObject stepParams = step["parameters"].as<JsonObject>();
                        Serial.printf("  执行步骤工具: %s\n", stepToolName.c_str());
                        String currentStepResult = executeTool(stepToolName, stepParams); // 递归调用支持嵌套工具
                        if (currentStepResult.startsWith("错误:")) {
                            stepResult = "错误: 步骤 '" + stepToolName + "' 失败: " + currentStepResult;
                            Serial.println(stepResult);
                            break; // 遇到错误则停止执行
                        }
                    } else {
                        stepResult = "错误: 脚本 '" + scriptName + "' 步骤格式无效。缺少 'tool_name' 或 'parameters'。";
                        Serial.println(stepResult);
                        break;
                    }
                }
                result = "成功: 自动化脚本 '" + scriptName + "' 执行完成。总体结果: " + stepResult;
            } else {
                result = "错误: 自动化脚本 '" + scriptName + "' 没有 'steps' 数组。";
                Serial.println(result);
            }
        } else {
            result = "错误: run_automation_script 缺少 'script_name' 参数。";
            Serial.println(result);
        }
    } else {
        result = "错误: 工具 '" + toolName + "' 未识别或未实现。";
        Serial.println(result);
    }
    return result;
}

// 保留 executeTask 以兼容系统其他部分
void TaskManager::executeTask(const String& taskName, const String& params) {
    Serial.println("执行传统任务: " + taskName + " 参数: " + params);
    // 此函数可保留用于兼容性，也可重构为调用 executeTool
    // 目前仅记录日志，不执行任何操作，以避免与 executeTool 冲突
    // 如有需要可将其映射到 executeTool。
    Serial.println("注意: 调用了传统 executeTask。建议重构为使用 executeTool。");
}

void TaskManager::handleHIDTask(const String& params) {
    // 此函数已废弃，请使用 executeTool 进行 HID 操作
    Serial.println("警告: 调用了 handleHIDTask。请使用 executeTool 进行 HID 操作。");
}

void TaskManager::handleWiFiTask(const String& params) {
    // 此函数已废弃，请使用 executeTool 进行 WiFi 操作
    Serial.println("警告: 调用了 handleWiFiTask。请使用 executeTool 进行 WiFi 操作。");
}

void TaskManager::handleBluetoothTask(const String& params) {
    // 此函数已废弃，请使用 executeTool 进行蓝牙操作
    Serial.println("警告: 调用了 handleBluetoothTask。请使用 executeTool 进行蓝牙操作。");
}

void TaskManager::handleTimerTask(const String& params) {
    // 此函数已废弃，请使用 executeTool 进行定时器操作
    Serial.println("警告: 调用了 handleTimerTask。请使用 executeTool 进行定时器操作。");
}

void TaskManager::handleGpioTask(const String& params) {
    // 此函数已废弃，请使用 executeTool 进行GPIO操作
    Serial.println("警告: 调用了 handleGpioTask。请使用 executeTool 进行GPIO操作。");
}

void TaskManager::handleWifiKillerTask(const String& params) {
    // 此函数已废弃，请使用 executeTool 进行Wi-Fi Killer操作
    Serial.println("警告: 调用了 handleWifiKillerTask。请使用 executeTool 进行Wi-Fi Killer操作。");
}
