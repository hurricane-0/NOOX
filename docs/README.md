# NOOX 文档导航

## 📚 文档列表

### 用户文档
- **[USER_GUIDE.md](./USER_GUIDE.md)** - 完整用户使用指南
- **[QUICKSTART_DUAL_FS.md](./QUICKSTART_DUAL_FS.md)** - 双文件系统快速开始

### 开发者文档
- **[TECHNICAL_SPECIFICATION.md](./TECHNICAL_SPECIFICATION.md)** - 技术规格文档
- **[shell_ai_feature_guide.md](./shell_ai_feature_guide.md)** - Shell AI 功能开发指南

## 🚀 快速开始

1. **首次使用**：阅读 [USER_GUIDE.md](./USER_GUIDE.md) 的第1-3章
2. **开发部署**：参考 [QUICKSTART_DUAL_FS.md](./QUICKSTART_DUAL_FS.md)
3. **高级开发**：查看 [TECHNICAL_SPECIFICATION.md](./TECHNICAL_SPECIFICATION.md)

## 📝 文档版本

- 用户指南版本：v2.0
- 技术规格版本：v3.0
- 最后更新：2025-10-13

## 💡 常见问题

### WiFi 无法连接？
查看 USER_GUIDE.md 第 9.1 节

### LLM API 调用失败？
查看 USER_GUIDE.md 第 9.2 节

### 如何添加新工具？
查看 TECHNICAL_SPECIFICATION.md 第 13 节

## 🔗 相关资源

- 项目仓库：https://github.com/your-repo/NOOX
- 问题反馈：https://github.com/your-repo/NOOX/issues


好问题！这是一个常见的误解。让我解释一下：

[2 tools called]

完美的问题！让我解释这个关键概念：

## 🔑 核心概念：`board_build.filesystem` ≠ 代码中可用的文件系统

### `board_build.filesystem` 的作用

```ini
board_build.filesystem = ffat  # ⚠️ 只影响 uploadfs 命令！
```

这个配置**只决定一件事**：
- 当你运行 `pio run --target uploadfs` 时
- PlatformIO 会把 `data/` 目录的内容上传到**哪个分区**

```
board_build.filesystem = ffat
         ↓
pio run --target uploadfs
         ↓
上传 data/ 内容 → FFat 分区
```

### 代码中的文件系统加载

在代码中，你可以**独立挂载任意文件系统**，与 `platformio.ini` 的设置无关：

```cpp
// src/main.cpp:80 - 手动挂载 LittleFS
LittleFS.begin(true);  // ✅ 挂载 littlefs 分区

// src/main.cpp:94 - 手动挂载 FFat  
FFat.begin(true, "/ffat");  // ✅ 挂载 ffat 分区
```

## 📊 三者的关系

```
┌────────────────────────────────────────────────────────┐
│ 1. 分区表 (partitions_16MB_app_large.csv)             │
│    定义：Flash 中有哪些分区                             │
├────────────────────────────────────────────────────────┤
│  littlefs, data, spiffs, 0x810000, 0x3F0000  ← 定义   │
│  ffat,     data, fat,    0xC00000, 0x400000  ← 定义   │
└────────────┬───────────────────────────────────────────┘
             │
             ↓
┌────────────────────────────────────────────────────────┐
│ 2. platformio.ini                                      │
│    决定：uploadfs 上传到哪个分区                        │
├────────────────────────────────────────────────────────┤
│  board_build.filesystem = ffat  ← 只影响 uploadfs     │
│                                                        │
│  执行 uploadfs 时：                                    │
│  data/ 目录 → FFat 分区 ✅                             │
└────────────┬───────────────────────────────────────────┘
             │
             ↓
┌────────────────────────────────────────────────────────┐
│ 3. 代码 (main.cpp)                                     │
│    决定：运行时挂载哪些文件系统                          │
├────────────────────────────────────────────────────────┤
│  LittleFS.begin()  → 挂载 littlefs 分区 ✅             │
│  FFat.begin()      → 挂载 ffat 分区 ✅                 │
│                                                        │
│  可以同时挂载多个！                                     │
└────────────────────────────────────────────────────────┘
```

## 🔍 详细解释

### 1. 分区表定义了"容器"

```csv
# boards/partitions_16MB_app_large.csv
littlefs,   data, spiffs,  0x810000, 0x3F0000,  # 4MB 空间
ffat,       data, fat,     0xC00000, 0x400000,  # 4MB 空间
```

这告诉 ESP32：
- Flash 的 `0x810000` 开始的 4MB 是一个分区（类型：spiffs，用于 LittleFS）
- Flash 的 `0xC00000` 开始的 4MB 是另一个分区（类型：fat，用于 FFat）

### 2. `platformio.ini` 控制上传目标

```ini
board_build.filesystem = ffat
```

**只在运行 `uploadfs` 时有效**：
```bash
pio run --target uploadfs
# ↑ 会读取 data/ 目录
# ↓ 上传到 ffat 分区（因为 board_build.filesystem = ffat）
```

**如果你改为**：
```ini
board_build.filesystem = littlefs  # 改成 littlefs
```

**然后运行**：
```bash
pio run --target uploadfs
# ↑ 会读取 data/ 目录  
# ↓ 上传到 littlefs 分区（因为改成了 littlefs）
```

### 3. 代码中独立挂载

```cpp
void setup() {
    // 挂载 LittleFS（读取 littlefs 分区）
    LittleFS.begin(true);
    
    // 挂载 FFat（读取 ffat 分区）
    FFat.begin(true, "/ffat");
    
    // 两者独立工作，互不干扰！
}
```

**ESP32-Arduino 如何找到分区？**

```cpp
LittleFS.begin()  // 自动查找分区表中 subtype="spiffs" 的分区
FFat.begin()      // 自动查找分区表中 subtype="fat" 的分区
```

## 💡 实际工作流程

### 当前配置下的文件流向

```
开发阶段：
─────────────────────────────────────────────
1. 准备文件：
   data/noox-host-agent.exe  ← 放在这里
   data/README.txt

2. 运行 uploadfs：
   $ pio run --target uploadfs
   
3. PlatformIO 读取配置：
   board_build.filesystem = ffat  ← 看到这个
   
4. 上传到 FFat 分区：
   data/ 内容 → Flash 的 ffat 分区 ✅


运行阶段：
─────────────────────────────────────────────
1. 设备启动，执行 main.cpp：

2. 挂载 LittleFS：
   LittleFS.begin()
   → 读取 Flash 的 littlefs 分区 ✅
   → 找到嵌入式部署的 Web 文件

3. 挂载 FFat：
   FFat.begin()  
   → 读取 Flash 的 ffat 分区 ✅
   → 找到 uploadfs 上传的 agent.exe

4. 两个文件系统都可用！
   LittleFS 用于：config.json, *.gz
   FFat 用于：agent.exe (USBMSC 可见)
```

## 🎯 类比理解

把 Flash 想象成一个硬盘：

```
Flash (硬盘)
├── C:\ (littlefs 分区) ← LittleFS.begin() 挂载
│   ├── config.json
│   └── web/
└── D:\ (ffat 分区)     ← FFat.begin() 挂载
    └── noox-host-agent.exe
```

- **分区表** = 硬盘分区（定义 C:\ 和 D:\ 的位置和大小）
- **`board_build.filesystem`** = "双击 setup.exe 时默认安装到 C:\ 还是 D:\"
- **代码中的 `begin()`** = "程序运行时打开 C:\ 和 D:\"

## 📝 总结

| 问题 | 答案 |
|------|------|
| `board_build.filesystem = ffat` 会阻止 LittleFS 工作吗？ | ❌ 不会！两者独立 |
| 代码能同时使用 LittleFS 和 FFat 吗？ | ✅ 可以！分别调用 `begin()` |
| 如何上传文件到 LittleFS？ | 改 `board_build.filesystem = littlefs` 然后 uploadfs |
| 如何上传文件到 FFat？ | 改 `board_build.filesystem = ffat` 然后 uploadfs |
| 为什么 Web 文件不用 uploadfs？ | 因为用了 C 数组嵌入，自动部署 |

**关键点**：`board_build.filesystem` 只影响 `uploadfs` 命令的目标，不影响代码中能使用哪些文件系统！✨