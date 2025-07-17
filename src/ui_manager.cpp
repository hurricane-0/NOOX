#include "ui_manager.h"
#include "hardware_config.h"
#include <U8g2lib.h>
#include <vector> // 用于保存脚本名称

// 按键消抖延迟
static unsigned long lastButtonPressTime = 0;
const unsigned long DEBOUNCE_DELAY = 200;

UIManager::UIManager(HardwareManager& hw, AppWiFiManager& wifi, SDManager& sd, TaskManager& task)
    : hardware(hw), wifi(wifi), sd(sd), taskManager(task), currentState(UI_STATE_STATUS), selectedMenuItem(0), scriptListScrollOffset(0) {}

void UIManager::begin() {
    hardware.getDisplay().clearBuffer();
    hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
    hardware.getDisplay().drawStr(0, 20, "Smart AI Platform");
    hardware.getDisplay().drawStr(0, 40, "Initializing...");
    hardware.getDisplay().sendBuffer();
    delay(2000);
}

void UIManager::update() {
    // 主循环，根据当前状态分发到对应的处理函数
    switch (currentState) {
        case UI_STATE_STATUS:
            handleStateStatus();
            break;
        case UI_STATE_MAIN_MENU:
            handleStateMainMenu();
            break;
        case UI_STATE_AUTO_SCRIPT_LIST:
            handleStateAutoScriptList();
            break;
        case UI_STATE_WIFI_KILLER:
            handleStateWifiKiller();
            break;
        case UI_STATE_TIMER:
            handleStateTimer();
            break;
        case UI_STATE_SETTINGS_MENU:
            handleStateSettingsMenu();
            break;
        default:
            currentState = UI_STATE_STATUS; // 默认回到状态界面
            break;
    }
}

// --- 状态处理函数 ---

void UIManager::handleStateStatus() {
    drawStatusScreen();

    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
    // 状态界面任意按键进入主菜单
        if (hardware.isButtonPressed(BUTTON_1_PIN) || hardware.isButtonPressed(BUTTON_2_PIN) || hardware.isButtonPressed(BUTTON_3_PIN)) {
            lastButtonPressTime = currentTime;
            currentState = UI_STATE_MAIN_MENU;
            selectedMenuItem = 0; // 重置菜单选项
        }
    }
}

void UIManager::handleStateMainMenu() {
    drawMainMenu();

    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        if (hardware.isButtonPressed(BUTTON_1_PIN)) { // OK键
            lastButtonPressTime = currentTime;
            switch (selectedMenuItem) {
                case 0: // Settings
                    Serial.println("Selected: Settings");
                    currentState = UI_STATE_SETTINGS_MENU;
                    selectedMenuItem = 0;
                    break;
                case 1: // Wi-Fi Killer
                    Serial.println("Selected: Wi-Fi Killer");
                    currentState = UI_STATE_WIFI_KILLER;
                    selectedMenuItem = 0;
                    break;
                case 2: // Timer
                    Serial.println("Selected: Timer");
                    currentState = UI_STATE_TIMER;
                    selectedMenuItem = 0;
                    break;
                case 3: // Auto
                    Serial.println("Selected: Auto");
                    currentState = UI_STATE_AUTO_SCRIPT_LIST;
                    selectedMenuItem = 0; // 新菜单重置选项
                    scriptListScrollOffset = 0;
                    break;
                case 4: // Back
                    currentState = UI_STATE_STATUS;
                    break;
            }
        } else if (hardware.isButtonPressed(BUTTON_2_PIN)) { // 上键
            lastButtonPressTime = currentTime;
            selectedMenuItem = (selectedMenuItem - 1 + 5) % 5; // 共5项，包括“返回”
        } else if (hardware.isButtonPressed(BUTTON_3_PIN)) { // 下键
            lastButtonPressTime = currentTime;
            selectedMenuItem = (selectedMenuItem + 1) % 5;
        }
    }
}

void UIManager::handleStateAutoScriptList() {
    drawAutoScriptList();

    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        std::vector<String> scripts = sd.listAutomationScriptNames(); // Use new method
        int itemCount = scripts.size() + 1; // 加1用于“返回”

        if (hardware.isButtonPressed(BUTTON_1_PIN)) { // OK键
            lastButtonPressTime = currentTime;
            if (selectedMenuItem == (itemCount - 1)) { // 选中“返回”
                currentState = UI_STATE_MAIN_MENU;
                selectedMenuItem = 0;
            } else {
                // 执行选中的脚本
                String scriptName = scripts[selectedMenuItem];
                Serial.print("Executing script: ");
                Serial.println(scriptName);
                // 创建一个空的 JsonObject 作为参数
                DynamicJsonDocument doc(1); // 最小容量，因为是空的
                JsonObject params = doc.to<JsonObject>();
                taskManager.executeTool(scriptName, params); // 假定脚本名即任务名，暂不传参数
                currentState = UI_STATE_STATUS; // 执行后返回状态界面
            }
        } else if (hardware.isButtonPressed(BUTTON_2_PIN)) { // 上键
            lastButtonPressTime = currentTime;
            selectedMenuItem = (selectedMenuItem - 1 + itemCount) % itemCount;
        } else if (hardware.isButtonPressed(BUTTON_3_PIN)) { // 下键
            lastButtonPressTime = currentTime;
            selectedMenuItem = (selectedMenuItem + 1) % itemCount;
        }
    }
}

void UIManager::handleStateWifiKiller() {
    drawWifiKillerScreen();
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        if (hardware.isButtonPressed(BUTTON_1_PIN)) { // OK键
            lastButtonPressTime = currentTime;
            // 切换Wi-Fi Killer模式：如果已激活则停止，否则启动
            // 假定TaskManager可跟踪Wi-Fi Killer状态，或可由当前状态推断
            // 为简化处理，这里直接切换启动/停止
            // 更健壮的方案应查询wifiManager实际状态
            static bool isKillerModeActive = false; // UI层简单状态跟踪
            if (isKillerModeActive) {
                DynamicJsonDocument doc(1);
                JsonObject params = doc.to<JsonObject>();
                taskManager.executeTool("wifi_killer_stop", params);
                isKillerModeActive = false;
                Serial.println("Wi-Fi Killer Mode: Stopped via UI.");
            } else {
                DynamicJsonDocument doc(1);
                JsonObject params = doc.to<JsonObject>();
                taskManager.executeTool("wifi_killer_start", params);
                isKillerModeActive = true;
                Serial.println("Wi-Fi Killer Mode: Started via UI.");
            }
            // 可选择停留在本界面显示状态，或返回主菜单
            // 当前实现为操作后返回主菜单
            currentState = UI_STATE_MAIN_MENU;
            selectedMenuItem = 1; // 主菜单高亮Wi-Fi Killer
        } else if (hardware.isButtonPressed(BUTTON_2_PIN) || hardware.isButtonPressed(BUTTON_3_PIN)) { // 上/下键
            lastButtonPressTime = currentTime;
            currentState = UI_STATE_MAIN_MENU; // 返回主菜单
            selectedMenuItem = 1; // 主菜单高亮Wi-Fi Killer
        }
    }
}

void UIManager::handleStateTimer() {
    drawTimerScreen();
    unsigned long currentTime = millis();
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        if (hardware.isButtonPressed(BUTTON_1_PIN)) { // OK键
            lastButtonPressTime = currentTime;
            // 切换定时器状态：如果正在运行则停止，否则设置并启动默认定时器
            if (taskManager.isTimerRunning()) { // 假定TaskManager有方法检查定时器状态
                DynamicJsonDocument doc(1);
                JsonObject params = doc.to<JsonObject>();
                taskManager.executeTool("timer_stop", params);
            } else {
                // 简化处理，默认设置5秒定时器
                DynamicJsonDocument doc(64); // 足够容纳一个int参数
                JsonObject params = doc.to<JsonObject>();
                params["duration"] = 5000; // 设置5000ms
                taskManager.executeTool("timer_set", params);

                DynamicJsonDocument doc2(1);
                JsonObject params2 = doc2.to<JsonObject>();
                taskManager.executeTool("timer_start", params2);
            }
            // 可选择返回状态界面或显示定时器状态
            currentState = UI_STATE_STATUS; // 操作后返回状态界面
        } else if (hardware.isButtonPressed(BUTTON_2_PIN) || hardware.isButtonPressed(BUTTON_3_PIN)) { // 上/下键
            lastButtonPressTime = currentTime;
            currentState = UI_STATE_MAIN_MENU; // 返回主菜单
            selectedMenuItem = 2; // 主菜单高亮Timer
        }
    }
}

void UIManager::handleStateSettingsMenu() {
    // 设置菜单状态处理
    drawSettingsMenu();
    unsigned long currentTime = millis();
    // 按键消抖处理
    if (currentTime - lastButtonPressTime > DEBOUNCE_DELAY) {
        // OK键按下，返回主菜单
        if (hardware.isButtonPressed(BUTTON_1_PIN)) {
            lastButtonPressTime = currentTime;
            // 简化处理：按OK直接返回主菜单，完整实现可进入WiFi/API Key子菜单
            Serial.println("Settings OK pressed - returning to main menu.");
            currentState = UI_STATE_MAIN_MENU;
            selectedMenuItem = 0; // 主菜单高亮Settings
        }
        // UP/DOWN键按下，返回主菜单
        else if (hardware.isButtonPressed(BUTTON_2_PIN) || hardware.isButtonPressed(BUTTON_3_PIN)) {
            lastButtonPressTime = currentTime;
            currentState = UI_STATE_MAIN_MENU;
            selectedMenuItem = 0;
        }
    }
}


// --- 绘制函数 ---

void UIManager::drawStatusScreen() {
    // 绘制状态界面（显示WiFi状态、IP、时间等）
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        // 获取WiFi信息和当前时间
        String ipAddress = wifi.getIPAddress();
        String wifiStatus = wifi.getWiFiStatus();
        // 获取当前时间（假设已NTP同步）
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        char timeStr[9];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
        String currentTime = String(timeStr);

        // OLED显示各项信息
        hardware.getDisplay().drawStr(0, 10, ("WiFi: " + wifiStatus).c_str());
        hardware.getDisplay().drawStr(0, 24, ("IP: " + ipAddress).c_str());
        hardware.getDisplay().drawStr(0, 38, ("Time: " + currentTime).c_str());
        hardware.getDisplay().setFont(u8g2_font_6x10_tf);
        hardware.getDisplay().drawStr(0, 60, "Press any key for menu...");
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawMainMenu() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
        hardware.getDisplay().drawStr(0, 12, "Main Menu");

        const int itemCount = 5;
        String menuItems[itemCount] = {"Settings", "Wi-Fi Killer", "Timer", "Auto", "Back"};
        
        for (int i = 0; i < itemCount; i++) {
            if (i == selectedMenuItem) {
                hardware.getDisplay().drawBox(0, 15 + (i * 12), 128, 12);
                hardware.getDisplay().setColorIndex(0);
            } else {
                hardware.getDisplay().setColorIndex(1);
            }
            hardware.getDisplay().drawStr(5, 25 + (i * 12), menuItems[i].c_str());
        hardware.getDisplay().setColorIndex(1); // 重置颜色
        }
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawAutoScriptList() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
        hardware.getDisplay().drawStr(0, 12, "Select Script");

        std::vector<String> scripts = sd.listAutomationScriptNames(); // Use new method
        if (scripts.empty()) {
            hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
            hardware.getDisplay().drawStr(5, 30, "No scripts found.");
        }

        int itemCount = scripts.size() + 1; // +1 for "Back"
        
        // 根据选中项调整滚动偏移
        const int visibleItems = 4; // 屏幕一次可显示的项目数
        if (selectedMenuItem < scriptListScrollOffset) {
            scriptListScrollOffset = selectedMenuItem;
        } else if (selectedMenuItem >= scriptListScrollOffset + visibleItems) {
            scriptListScrollOffset = selectedMenuItem - visibleItems + 1;
        }

        for (int i = 0; i < visibleItems; i++) {
            int currentItemIndex = scriptListScrollOffset + i;
            if (currentItemIndex < itemCount) {
                String itemText;
                if (currentItemIndex < scripts.size()) {
                    itemText = scripts[currentItemIndex];
                } else {
                    itemText = "Back";
                }

                if (currentItemIndex == selectedMenuItem) {
                    hardware.getDisplay().drawBox(0, 15 + (i * 12), 128, 12);
                    hardware.getDisplay().setColorIndex(0);
                } else {
                    hardware.getDisplay().setColorIndex(1);
                }
                hardware.getDisplay().drawStr(5, 25 + (i * 12), itemText.c_str());
                hardware.getDisplay().setColorIndex(1); // 重置颜色
            }
        }
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawWifiKillerScreen() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
        hardware.getDisplay().drawStr(0, 12, "Wi-Fi Killer");
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        hardware.getDisplay().drawStr(5, 30, "Press OK to start scan.");
        hardware.getDisplay().drawStr(5, 45, "UP/DOWN to go back.");
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawTimerScreen() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
        hardware.getDisplay().drawStr(0, 12, "Timer Control");
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        hardware.getDisplay().drawStr(5, 30, "Press OK to set timer.");
        hardware.getDisplay().drawStr(5, 45, "UP/DOWN to go back.");
    } while (hardware.getDisplay().nextPage());
}

void UIManager::drawSettingsMenu() {
    hardware.getDisplay().firstPage();
    do {
        hardware.getDisplay().setFont(u8g2_font_ncenB10_tr);
        hardware.getDisplay().drawStr(0, 12, "Settings");
        hardware.getDisplay().setFont(u8g2_font_ncenB08_tr);
        hardware.getDisplay().drawStr(5, 30, "Press OK for options.");
        hardware.getDisplay().drawStr(5, 45, "UP/DOWN to go back.");
    } while (hardware.getDisplay().nextPage());
}
