# 双文件系统快速开始指南

## 🚀 快速部署（推荐）

### 一键自动化部署

```bash
python deploy_all.py
```

**自动完成**：
1. 压缩 Web 文件到 `data_littlefs/`
2. 编译固件
3. 上传固件到设备
4. **直接构建** LittleFS 镜像（2MB）并上传到 0x810000
5. **直接构建** FFat 镜像（6MB）并上传到 0xA10000

**⚠️ 技术说明**：脚本绕过 PlatformIO 的 `buildfs`，直接调用 `mklittlefs` 和 `mkfatfs` 工具，以确保使用正确的分区大小。详见 [文件系统大小问题解决方案](filesystem_size_issue_solution.md)

**输出示例**：
```
============================================================
  NOOX 设备完整部署脚本
  Automated Dual Filesystem Deployment
============================================================

[1/5] Compressing web files
============================================================
Compressing web files for LittleFS...
source_data\index.html: 6,100 bytes -> 1,658 bytes (72.8% reduction)
source_data\style.css: 24,479 bytes -> 4,807 bytes (80.4% reduction)
source_data\script.js: 16,634 bytes -> 4,182 bytes (74.9% reduction)
✓ Web files compressed successfully

[2/5] Compiling firmware
============================================================
✓ Firmware compiled successfully

[3/5] Uploading firmware
============================================================
✓ Firmware uploaded successfully

[4/5] Uploading LittleFS (Web files)
============================================================
✓ LittleFS uploaded successfully

[5/5] Uploading FFat (Agent files)
============================================================
✓ FFat uploaded successfully

============================================================
  ✓ Deployment Complete!
============================================================
```

### 监控设备启动

```bash
pio device monitor
```

**预期输出**：
```
[FS]  LittleFS Mounted successfully
[FS]  FFat Mounted successfully
[FS] Agent file found: 8388608 bytes (8.00 MB)
[USB]  USB MSC driver started successfully
[WEB] All web files present in LittleFS
[WEB] Web server started on port 80
✅ Setup complete. Starting main loop...
```

## ✅ 验证功能

### 1. 检查 USB 设备

**Windows**：
- 打开设备管理器
- 应该看到：
  - ✅ HID 兼容设备（键盘/鼠标）
  - ✅ USB 串行设备（COM 端口）
  - ✅ 可移动磁盘（NOOXDisk）

**Linux**：
```bash
lsusb                # 查看 USB 设备
ls /dev/ttyACM*      # 查看串口设备
lsblk                # 查看存储设备
```

### 2. 检查 U 盘内容

打开 "NOOXDisk" 驱动器，应该看到：
- ✅ `noox-host-agent.exe` (约 8MB)
- ✅ `README.txt`

### 3. 测试 Web 界面

1. 设备连接 WiFi（通过 OLED 屏幕操作）
2. 浏览器访问设备 IP 地址
3. 应该看到 NOOX Web 控制界面

## 📋 手动部署（高级用户）

如果需要更精细的控制，可以手动执行各个步骤：

```bash
# 1. 压缩 Web 文件
python compress_files_optimized.py

# 2. 编译并上传固件
pio run --target upload

# 3. 上传 LittleFS (需手动修改 platformio.ini)
# 修改: board_build.filesystem = littlefs
pio run --target uploadfs

# 4. 上传 FFat (需手动修改 platformio.ini)
# 修改: board_build.filesystem = ffat
pio run --target uploadfs
```

**注意**：手动部署容易出错，推荐使用自动化脚本。

## 🔧 常见问题

### Q1: Web 文件丢失

**症状**：访问 Web 界面显示 404，串口显示：
```
[WEB] WARNING: /index.html.gz not found in LittleFS
[WEB] ERROR: Web files missing!
```

**解决**：
```bash
# 重新运行部署脚本
python deploy_all.py
```

### Q2: U 盘为空

**症状**：可以看到 NOOXDisk，但里面没有文件

**解决**：
```bash
# 确保 data_ffat 目录有 agent 文件
ls data_ffat/

# 重新运行部署脚本
python deploy_all.py
```

### Q3: Agent 文件太大

**症状**：
```
[FS] ❌ FFat Mount Failed!
```

**原因**：agent.exe 超过 4MB

**解决方案 A**：压缩 agent
```bash
# 使用 UPX 压缩
upx --best noox-host-agent.exe
```

**解决方案 B**：调整分区大小

编辑 `boards/partitions_16MB_app_large.csv`：
```csv
littlefs,   data, spiffs,  0x810000, 0x200000,  # 减少到 2MB
ffat,       data, fat,     0xA10000, 0x5F0000,  # 增加到 6MB
```

### Q4: 部署脚本失败

**症状**：部署脚本中途失败

**解决**：
1. 检查设备是否正确连接
2. 查看错误信息定位问题
3. 脚本会自动回滚配置文件
4. 修复问题后重新运行脚本

## 📊 系统资源使用

| 组件 | 大小 | 说明 |
|------|------|------|
| 固件 | ~2MB | 纯固件代码（不含 Web 文件） |
| LittleFS | 4MB | 配置 + Web 文件（实际使用 ~10KB） |
| FFat | 4MB | Agent 文件（实际使用 ~8MB） |
| PSRAM | 8MB | 运行时缓冲区 |

## 🎯 开发工作流

### 修改 Web 界面

```bash
# 1. 编辑源文件
nano source_data/index.html

# 2. 压缩并部署
python compress_files_optimized.py

# 3. 只上传 LittleFS（可选，快速更新）
# 手动设置 platformio.ini: board_build.filesystem = littlefs
# 复制 data_littlefs/ 到 data/
pio run --target uploadfs

# 或使用完整部署（推荐）
python deploy_all.py
```

### 更新 Agent 程序

```bash
# 1. 编译 Go agent
cd host-agent
go build -o noox-host-agent.exe

# 2. 复制到 data_ffat 目录
cp noox-host-agent.exe ../data_ffat/

# 3. 运行部署脚本（会自动上传 FFat）
cd ..
python deploy_all.py
```

### 完整重置

```bash
# 1. 擦除 Flash
pio run --target erase

# 2. 上传固件
pio run --target upload

# 3. 上传文件系统
pio run --target uploadfs
```

## 📝 文件清单

**必需文件**：
- ✅ `source_data/index.html` - Web 界面 HTML 源文件
- ✅ `source_data/style.css` - Web 样式源文件
- ✅ `source_data/script.js` - Web 脚本源文件
- ✅ `data_ffat/noox-host-agent.exe` - 主机代理程序
- ✅ `boards/partitions_16MB_app_large.csv` - 分区表
- ✅ `deploy_all.py` - 自动化部署脚本

**自动生成**：
- ⚙️ `data_littlefs/*.gz` - 压缩的 Web 文件（部署前生成）
- ⚙️ `data/` - 临时目录（部署时自动创建和清理）

## 🎓 相关文档

- [详细技术文档](./dual_filesystem_implementation.md)
- [用户使用指南](./USER_GUIDE.md)
- [USB MSC 修复说明](./usb_msc_initialization_fix.md)

## ✨ 架构优势

| 特性 | 说明 |
|------|------|
| 🚀 **一键部署** | 自动化脚本完成所有步骤 |
| 🔧 **智能配置** | 自动切换文件系统配置 |
| 💾 **优化体积** | 固件不含 Web 文件，更小更快 |
| 🔄 **灵活更新** | Web 文件和 Agent 独立更新 |
| ✅ **原生兼容** | U 盘功能即插即用 |
| 📦 **独立分区** | FFat 和 LittleFS 完全隔离 |

---

## 📋 当前状态（2025-10-13）

### ✅ 已完成
- [x] 解决文件系统大小问题（PlatformIO bug）
- [x] LittleFS 正确挂载（2MB）
- [x] FFat 正确挂载（6MB）
- [x] WiFi 连接正常
- [x] Web 服务器运行
- [x] 设备稳定启动

### 📝 下一步工作
- [ ] 上传 `noox-host-agent.exe` 到 FFat 分区
- [ ] 测试 USB MSC 功能（U 盘模拟）
- [ ] 验证 Web 界面文件访问
- [ ] 测试通过 Web 界面上传 Agent 文件

### 🐛 已知问题
- **USB MSC 驱动启动失败**：因为 FFat 分区中还没有文件，导致驱动初始化失败。上传 agent 文件后应该能解决。

---

**需要帮助？** 查看 [故障排查文档](./dual_filesystem_implementation.md#故障排查) 或 [文件系统大小问题解决方案](./filesystem_size_issue_solution.md)

