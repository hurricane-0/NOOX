#include "wifi_manager.h"
#include <WiFi.h>
#include <ArduinoJson.h> // 引入 ArduinoJson 用于配置处理
#include <vector> // 用于 std::vector

// 用于 Wi-Fi Killer 模式
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h" // 用于事件循环

// 静态混杂模式回调的全局指针
static AppWiFiManager* s_instance = nullptr;

// 数据包捕获回调函数
void IRAM_ATTR wifi_packet_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (s_instance != nullptr) {
        // 处理数据包。目前仅打印消息。
        // 在完整实现中，你可以解析数据包以识别客户端、AP等。
        wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
        Serial.printf("Packet sniffed! Type: %d, RSSI: %d, Len: %d\n", type, pkt->rx_ctrl.rssi, pkt->rx_ctrl.sig_len);
        // 可以访问 pkt->payload 获取原始数据包数据
    }
}

// 更新构造函数以接受并保存 SDManager 引用
AppWiFiManager::AppWiFiManager(LLMManager& llm, SDManager& sd) 
    : llmManager(llm), sdManager(sd) {
    s_instance = this; // 设置静态实例
}

void AppWiFiManager::begin() {
    loadAndConnect(); // 调用新的辅助函数
}

void AppWiFiManager::loop() {
    // 保持活动或检查状态（如有需要）
    // 目前，仅确保保持连接
    if (WiFi.status() != WL_CONNECTED) {
        // 可选：在此实现更强健的重新连接策略
        // 为了简单起见，我们将依赖 loadAndConnect 进行初始连接
        // 并假设在保存凭据时会调用它。
    }
}

String AppWiFiManager::getIPAddress() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "N/A";
}

String AppWiFiManager::getWiFiStatus() {
    switch (WiFi.status()) {
        case WL_IDLE_STATUS: return "Idle";
        case WL_NO_SSID_AVAIL: return "No SSID";
        case WL_SCAN_COMPLETED: return "Scan Done";
        case WL_CONNECTED: return "Connected";
        case WL_CONNECT_FAILED: return "Failed";
        case WL_CONNECTION_LOST: return "Lost";
        case WL_DISCONNECTED: return "Disconnected";
        default: return "Unknown";
    }
}

bool AppWiFiManager::addWiFiCredential(const String& ssid, const String& password) {
    JsonDocument doc = sdManager.loadWiFiConfig(); // Use new method
    JsonObject config = doc.as<JsonObject>();

    JsonArray wifiNetworks = config["wifi_networks"].to<JsonArray>(); // 这会在不存在时创建数组

    // 检查 SSID 是否已存在，如果存在，则更新
    bool updated = false;
    for (JsonObject network : wifiNetworks) {
        if (network["ssid"] == ssid) {
            network["password"] = password;
            updated = true;
            break;
        }
    }

    if (!updated) {
        // 添加新凭据
        JsonObject newNetwork = wifiNetworks.add<JsonObject>(); // 使用 add<JsonObject>()
        newNetwork["ssid"] = ssid;
        newNetwork["password"] = password;
    }

    bool success = sdManager.saveWiFiConfig(doc); // Use new method
    if (success) {
        Serial.println("WiFi credentials added/updated. Attempting to connect...");
        loadAndConnect(); // 尝试使用更新的凭据连接
    }
    return success;
}

bool AppWiFiManager::deleteWiFiCredential(const String& ssid) {
    JsonDocument doc = sdManager.loadWiFiConfig(); // Use new method
    JsonObject config = doc.as<JsonObject>();

    JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();
    if (wifiNetworks.isNull()) {
        Serial.println("No WiFi networks to delete.");
        return false;
    }

    int indexToRemove = -1;
    for (size_t i = 0; i < wifiNetworks.size(); ++i) {
        if (wifiNetworks[i]["ssid"] == ssid) {
            indexToRemove = i;
            break;
        }
    }

    if (indexToRemove != -1) {
        wifiNetworks.remove(indexToRemove);
    bool success = sdManager.saveWiFiConfig(doc); // Use new method
        if (success) {
            Serial.println("WiFi credential deleted. Reconnecting if current network was deleted.");
            // 如果当前连接的网络被删除，则断开连接并尝试连接到其他网络
            if (WiFi.SSID() == ssid) {
                WiFi.disconnect();
                loadAndConnect();
            }
        }
        return success;
    }
    Serial.println("SSID not found for deletion.");
    return false;
}

std::vector<WiFiCredential> AppWiFiManager::getSavedCredentials() {
    std::vector<WiFiCredential> credentials;
    JsonDocument doc = sdManager.loadWiFiConfig(); // Use new method
    JsonObject config = doc.as<JsonObject>();

    JsonArray wifiNetworks = config["wifi_networks"].as<JsonArray>();
    if (!wifiNetworks.isNull()) {
        for (JsonObject network : wifiNetworks) {
            WiFiCredential cred;
            cred.ssid = network["ssid"].as<String>();
            cred.password = network["password"].as<String>();
            credentials.push_back(cred);
        }
    }
    return credentials;
}

void AppWiFiManager::connectToWiFi() {
    // 此函数现在是 loadAndConnect 的辅助函数，用于连接到特定的 SSID/密码
    // 它是私有的，由 loadAndConnect 调用
}

void AppWiFiManager::loadAndConnect() {
    JsonDocument doc = sdManager.loadWiFiConfig(); // Use new method
    
    if (doc.isNull() || !doc["wifi_networks"].is<JsonArray>()) {
        Serial.println("WiFi config not found on SD card or no networks saved.");
        return;
    }

    JsonArray wifiNetworks = doc["wifi_networks"].as<JsonArray>();
    if (wifiNetworks.isNull() || wifiNetworks.size() == 0) {
        Serial.println("No WiFi networks saved.");
        return;
    }

    // 尝试连接到每个保存的网络
    for (JsonObject network : wifiNetworks) {
        String ssid = network["ssid"].as<String>();
        String password = network["password"].as<String>();

        if (ssid.length() == 0) {
            Serial.println("Skipping network with empty SSID.");
            continue;
        }

        Serial.print("Attempting to connect to WiFi: ");
        Serial.println(ssid);

        WiFi.begin(ssid.c_str(), password.c_str());
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            Serial.print(".");
            retries++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to WiFi!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            return; // 成功连接，退出
        } else {
            Serial.println("\nWiFi connection failed for this network.");
            WiFi.disconnect(); // 在尝试下一个网络之前断开连接
        }
    }
    Serial.println("Failed to connect to any saved WiFi network.");
}

void AppWiFiManager::startWifiKillerMode() {
    Serial.println("Wi-Fi Killer Mode: Starting...");
    
    // 保存当前 WiFi 模式以便稍后恢复
    // 注意：WiFi.getMode() 返回 uint8_t，转换为 wifi_mode_t
    // 如果 WiFi 未连接或处于特定状态，这可能不完全可靠。
    // 更健壮的解决方案可能涉及存储最后 *预期* 的模式。
    // 为了简单起见，我们假设 STA 模式是要返回的默认值。
    // currentWifiMode = WiFi.getMode(); 
    
    WiFi.disconnect(true); // 从任何连接的 AP 断开
    delay(100); // 给它一点时间断开连接

    // 将 WiFi 设置为站点模式（或 APSTA，如果需要其他功能）
    // 对于混杂模式，WIFI_MODE_STA 通常是足够的。
    esp_wifi_set_mode(WIFI_MODE_STA); 
    
    // 启用混杂模式
    esp_wifi_set_promiscuous(true);
    
    // 注册数据包嗅探回调
    esp_wifi_set_promiscuous_rx_cb(&wifi_packet_sniffer_cb);

    // 设置要监视的默认频道（例如，频道 1）
    // 在完整实现中，你会扫描频道或允许用户选择。
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    Serial.println("Wi-Fi Killer Mode: Promiscuous mode enabled on channel 1.");
    Serial.println("WARNING: Wi-Fi Killer functionality (deauthentication) has ethical and legal implications.");
    Serial.println("         Use only on networks you own or have explicit permission to test.");
}

void AppWiFiManager::stopWifiKillerMode() {
    Serial.println("Wi-Fi Killer Mode: Stopping...");
    
    // 禁用混杂模式
    esp_wifi_set_promiscuous(false);
    
    // 注销数据包嗅探回调
    esp_wifi_set_promiscuous_rx_cb(NULL);

    // 将 WiFi 恢复到正常操作（例如，重新连接到保存的网络）
    Serial.println("Wi-Fi Killer Mode: Promiscuous mode disabled. Attempting to reconnect to WiFi.");
    loadAndConnect(); // 尝试重新连接到保存的网络
}
