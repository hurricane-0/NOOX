# 双文件系统实现说明（FFat + LittleFS）

## 概述

NOOX 设备现在使用**双文件系统**架构，解决了 USB MSC（U 盘模拟）的兼容性问题：

- **FFat（4MB）**：专用于 USBMSC，主机可见的 U 盘，存放 `noox-host-agent.exe`
- **LittleFS（4MB）**：用于内部配置和 Web 文件

## 架构图

```
┌─────────────────────────────────────────────────────────────┐
│ ESP32-S3 Flash (16MB)                                       │
├─────────────────────────────────────────────────────────────┤
│ NVS (20KB)          │ 系统配置                               │
│ OTA Data (8KB)      │ OTA 元数据                            │
│ App0 (8MB)          │ 固件程序                               │
│ LittleFS (2MB)      │ • config.json (设备配置)               │
│                     │ • *.html.gz (Web 界面，嵌入式部署)     │
│ FFat (6MB)          │ • noox-host-agent.exe (主机代理)       │
│                     │ • README.txt (说明文件)                │
└─────────────────────────────────────────────────────────────┘
         ↓                              ↓
    WebManager                      USBMSC
  (内部 HTTP 服务器)               (主机可见 U 盘)
```

## 技术细节

### 1. 分区表配置

文件：`boards/partitions_16MB_app_large.csv`

```csv
# Name,     Type, SubType, Offset,   Size,     Flags
nvs,        data, nvs,     0x9000,   0x5000,
otadata,    data, ota,     0xe000,   0x2000,
app0,       app,  ota_0,   0x10000,  0x800000,
littlefs,   data, spiffs,  0x810000, 0x200000,  # 2MB for internal use
ffat,       data, fat,     0xA10000, 0x5F0000,  # 6MB for USBMSC
```

**说明**：
- `littlefs` 使用 `spiffs` subtype 以兼容 PlatformIO
- `ffat` 使用 `fat` subtype，ESP32-Arduino 会自动识别为 FAT 文件系统

### 2. 初始化顺序

文件：`src/main.cpp`

```cpp
void setup() {
    // 1. LittleFS 初始化（内部数据）
    LittleFS.begin(true);
    
    // 2. FFat 初始化（U 盘数据）
    FFat.begin(true, "/ffat");
    
    // 3. USBMSC 初始化（自动绑定到 FFat）
    usb_msc_driver.begin(block_count, block_size);
    
    // 4. 其他组件...
    webManagerPtr->begin(); // 会自动部署嵌入的 Web 文件
}
```

**关键点**：
- ESP32-Arduino 会自动将 USBMSC 绑定到最后挂载的 FAT 文件系统（FFat）
- 无需手动编写读写回调函数
- 主机会将设备识别为标准 FAT 格式的 U 盘

### 3. Web 文件部署策略

#### 问题
- Web 文件需要在 LittleFS 中
- FFat 用于 USBMSC，不适合存放内部文件
- PlatformIO 的 `uploadfs` 只能上传到一个分区

#### 解决方案：自动化脚本部署

**部署前**：
1. 运行 `python compress_files_optimized.py`
2. 脚本压缩 `source_data/*.{html,css,js}` 文件
3. 输出到 `data_littlefs/` 目录（约 10KB）

**部署时（自动化脚本 `deploy_all.py`）**：
1. 压缩 Web 文件到 `data_littlefs/`
2. 编译并上传固件
3. 临时修改 `platformio.ini`：`board_build.filesystem = littlefs`
4. 复制 `data_littlefs/` 到临时 `data/` 目录
5. 上传 LittleFS 分区
6. 恢复 `platformio.ini`：`board_build.filesystem = ffat`
7. 复制 `data_ffat/` 到临时 `data/` 目录
8. 上传 FFat 分区
9. 清理临时文件

**运行时**：
1. `WebManager::begin()` 检查 LittleFS 中是否存在文件
2. 如果不存在，显示警告提示运行部署脚本
3. Web 文件直接从 LittleFS 提供服务

**优势**：
- ✅ 固件体积更小（不含 Web 文件）
- ✅ 启动速度更快（无需部署步骤）
- ✅ 逻辑更直观（文件系统直接存储）
- ✅ 一键自动化部署（无需手动切换配置）
- ✅ 独立更新 Web 文件和 Agent

### 4. Agent 文件部署

文件位置：`data_ffat/noox-host-agent.exe`

#### 方法 1：自动化脚本（推荐）

```bash
# 一键部署所有文件系统
python deploy_all.py
```

脚本自动：
- 上传 LittleFS（Web 文件）
- 上传 FFat（Agent 文件）
- 管理配置文件切换

#### 方法 2：手动 uploadfs（高级）

```bash
# 手动修改 platformio.ini: board_build.filesystem = ffat
# 复制 data_ffat/ 到 data/
pio run --target uploadfs
```

**注意**：手动方式容易出错，推荐使用自动化脚本。

#### 方法 3：Web 界面上传（未来实现）

可以添加一个 Web 路由来上传 agent 文件：

```cpp
server.on("/upload_agent", HTTP_POST, 
    [](AsyncWebServerRequest *request){},
    [](AsyncWebServerRequest *request, String filename, size_t index, 
       uint8_t *data, size_t len, bool final) {
        // 写入 FFat
        File f = FFat.open("/noox-host-agent.exe", "w");
        f.write(data, len);
        f.close();
    }
);
```

## 使用流程

### 开发者部署流程

**推荐：一键自动化部署**
```bash
# 完成所有部署步骤
python deploy_all.py

# 监控串口输出
pio device monitor
```

**手动部署（高级用户）**
```bash
# 1. 压缩 Web 文件
python compress_files_optimized.py

# 2. 编译固件
pio run

# 3. 上传固件
pio run --target upload

# 4. 上传 LittleFS（需手动切换 platformio.ini）
# 修改: board_build.filesystem = littlefs
# 复制 data_littlefs/ 到 data/
pio run --target uploadfs

# 5. 上传 FFat（需手动切换 platformio.ini）
# 修改: board_build.filesystem = ffat
# 复制 data_ffat/ 到 data/
pio run --target uploadfs

# 6. 监控串口输出
pio device monitor
```

### 用户使用流程

```
1. 连接 NOOX 设备到电脑
   ↓
2. 主机识别 3 个设备：
   - HID 键盘/鼠标
   - CDC 虚拟串口
   - MSC U 盘（NOOXDisk）
   ↓
3. 打开 U 盘，看到：
   - noox-host-agent.exe
   - README.txt
   ↓
4. 复制 agent.exe 到本地运行
   或等待自动启动（实验性功能）
```

## 预期串口输出

```
Serial setup
Setup starting...
=====================================
[FS] Initializing LittleFS...
[FS]  LittleFS Mounted successfully
[FS]    Total: 4128768 bytes (3.94 MB)
[FS]    Used:  10647 bytes (0.01 MB)
[FS] Initializing FFat for USBMSC...
[FS]  FFat Mounted successfully
[FS]    Total: 4194304 bytes (4.00 MB)
[FS]    Used:  8388608 bytes (8.00 MB)
[FS] Agent file found: 8388608 bytes (8.00 MB)
[USB] Configuring USBMSC driver...
[USB]  USB MSC driver started successfully
[USB]  PC will see NOOX as a removable disk
[USB]  FFat partition is accessible via U disk
=====================================
...
[WEB] Initializing web server...
[WEB] All web files present in LittleFS
[WEB] Web server started on port 80
...
✅ Setup complete. Starting main loop...
```

**如果 Web 文件未上传**：
```
[WEB] Initializing web server...
[WEB] WARNING: /index.html.gz not found in LittleFS
[WEB] WARNING: /style.css.gz not found in LittleFS
[WEB] WARNING: /script.js.gz not found in LittleFS
[WEB] ========================================
[WEB] ERROR: Web files missing!
[WEB] Please run deployment script:
[WEB]   python deploy_all.py
[WEB] ========================================
[WEB] Web server started on port 80
```

## 优势总结

| 特性 | C 数组嵌入方案 | 直接 LittleFS 存储（当前） |
|------|--------------|------------------------|
| **固件大小** | ⚠️ +10KB | ✅ 无额外开销 |
| **启动速度** | ⚠️ 需部署 (~0.5秒) | ✅ 直接读取（更快） |
| **部署复杂度** | ⚠️ 中（一次upload） | ✅ 低（自动化脚本） |
| **逻辑清晰度** | ⚠️ 间接（嵌入→部署） | ✅ 直观（直接存储） |
| **更新灵活性** | ⚠️ 需重新编译固件 | ✅ 独立更新文件系统 |
| **调试便利性** | ⚠️ 难（嵌入在固件中） | ✅ 易（直接访问文件） |
| **开发效率** | ⚠️ 中（每次压缩+编译） | ✅ 高（脚本自动化） |

| 特性 | USB MSC 回调方案 | FFat 原生方案（当前） |
|------|---------------|---------------------|
| **实现复杂度** | 🔴 高（需手写回调） | 🟢 低（框架自动） |
| **主机兼容性** | 🔴 差（需格式转换） | 🟢 优（原生 FAT） |
| **性能** | ⚠️ 中（回调开销） | 🟢 好（直接访问） |
| **可靠性** | ⚠️ 中（容易出错） | 🟢 高（久经考验） |
| **主机识别** | ❌ 可能失败 | ✅ 即插即用 |

## 故障排查

### 问题 1：USBMSC 无法识别

**症状**：主机看不到 U 盘设备

**排查**：
```cpp
// 检查串口输出
[USB] ❌ USB MSC driver failed to start
```

**解决**：
1. 确认 FFat 分区已正确格式化
2. 重新烧录固件和文件系统
3. 检查分区表配置

### 问题 2：U 盘为空

**症状**：主机可以看到 U 盘，但里面没有文件

**排查**：
```cpp
[FS] ⚠️  noox-host-agent.exe NOT found in FFat
```

**解决**：
```bash
# 上传 FFat 文件系统
pio run --target uploadfs
```

### 问题 3：Web 界面 404

**症状**：访问设备 IP 显示 404

**排查**：
```cpp
[WEB] ❌ Failed to deploy web files!
```

**解决**：
1. 重新运行 `python compress_files_optimized.py`
2. 检查 `include/web_files_embedded.h` 是否生成
3. 重新编译并上传固件

### 问题 4：LittleFS 空间不足

**症状**：
```cpp
[WEB] ❌ Write error for /index.html.gz
```

**解决**：
1. 检查 LittleFS 使用情况
2. 必要时增加 LittleFS 分区大小（减少 FFat）
3. 删除旧的/不需要的文件

## 未来改进

### 短期
- [ ] 添加 Web 界面的 agent 上传功能
- [ ] 实现 FFat 文件浏览 API
- [ ] 优化嵌入式文件的内存使用

### 中期
- [ ] 支持 OTA 更新 agent.exe
- [ ] 添加多平台 agent（Linux、macOS）
- [ ] 实现安全的文件加密

### 长期
- [ ] 动态调整分区大小
- [ ] 支持 USB MSC 写入（双向同步）
- [ ] 实现虚拟 CD-ROM（自动运行）

## 参考资料

- [ESP32-Arduino USB API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html)
- [ESP32-S3 USBMSC 示例](https://github.com/espressif/arduino-esp32/tree/master/libraries/USB/examples)
- [LittleFS 文档](https://github.com/littlefs-project/littlefs)
- [FAT 文件系统规范](https://en.wikipedia.org/wiki/File_Allocation_Table)

## 相关文件

**核心代码**：
- `boards/partitions_16MB_app_large.csv` - 分区表定义
- `src/main.cpp` - 双文件系统初始化
- `src/web_manager.cpp` - Web 服务器和文件检查
- `include/web_manager.h` - WebManager 头文件
- `platformio.ini` - 构建配置（由脚本自动切换）

**部署工具**：
- `deploy_all.py` - 自动化部署脚本（推荐使用）
- `compress_files_optimized.py` - Web 文件压缩工具

**源文件**：
- `source_data/index.html` - Web 界面 HTML 源码
- `source_data/style.css` - Web 样式源码
- `source_data/script.js` - Web 脚本源码

**数据目录**：
- `data_littlefs/` - LittleFS 分区内容（压缩的 Web 文件）
- `data_ffat/` - FFat 分区内容（agent.exe）
- `data/` - 临时目录（部署时创建，完成后删除）

---

**版本**：1.0  
**最后更新**：2025-10-13  
**作者**：NOOX 开发团队
