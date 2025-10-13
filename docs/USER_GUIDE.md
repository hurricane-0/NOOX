# NOOX 智能硬件平台用户指南

## 文档信息

- **项目名称**: NOOX ESP32-S3 AI 智能硬件平台
- **文档版本**: 2.0
- **适用固件版本**: v3.0+
- **文档日期**: 2025-10-12
- **面向用户**: 开发者、高级用户、系统集成商

---

## 目录

1. [快速入门](#1-快速入门)
2. [硬件连接](#2-硬件连接)
3. [首次配置](#3-首次配置)
4. [Web 界面使用](#4-web-界面使用)
5. [OLED 界面操作](#5-oled-界面操作)
6. [USB Shell 交互](#6-usb-shell-交互)
7. [LLM 功能详解](#7-llm-功能详解)
8. [故障排除](#8-故障排除)
9. [最佳实践](#9-最佳实践)

---

## 1. 快速入门

### 1.1 系统要求

#### 硬件要求

- **NOOX ESP32-S3 设备** × 1
- **USB Type-C 数据线** × 1
- **计算机**（Windows/macOS/Linux）
- **WiFi 路由器**（2.4 GHz）

#### 软件要求

- **Web 浏览器**（Chrome/Edge/Firefox/Safari）
- **主机代理程序**（可选，用于 USB Shell 功能）
- **串口监视器**（可选，用于调试）

### 1.2 5 分钟快速上手

#### 第一步：连接硬件

1. 使用 USB Type-C 数据线连接 NOOX 设备到计算机
2. 设备上电，OLED 屏幕显示初始化信息
3. RGB LED 呈现蓝色表示设备正常启动

#### 第二步：配置 WiFi

**方法 1：通过 OLED 菜单**

1. 按下 **Button A (OK)** 进入主菜单
2. 使用 **Button B (Up)** 和 **Button C (Down)** 选择 "WiFi Settings"
3. 按 **Button A** 确认
4. 选择 "Connect Saved"
5. 选择您的 WiFi 网络并确认连接

**方法 2：通过配置文件**

1. 设备在计算机上会显示为 U 盘（MSD 模式）
2. 编辑 `/config.json` 文件
3. 修改 `wifi_networks` 部分，添加您的 WiFi 信息：

```json
{
  "wifi_networks": [
    {
      "ssid": "YourWiFiName",
      "password": "YourPassword"
    }
  ]
}
```

4. 保存文件并重启设备

#### 第三步：访问 Web 界面

1. OLED 屏幕会显示设备的 IP 地址（例如：192.168.1.100）
2. 在浏览器中输入该 IP 地址
3. 进入 NOOX Web 控制台

#### 第四步：配置 LLM

1. 点击右上角的 **设置按钮** (⚙️)
2. 在 "LLM 提供商" 部分：
   - 选择提供商（DeepSeek/OpenRouter/OpenAI）
   - 输入 API Key
   - 选择模型
3. 点击 **保存并应用**

#### 第五步：开始对话

1. 在聊天窗口输入您的问题
2. 按回车或点击发送按钮
3. 等待 AI 响应

**恭喜！您已成功设置 NOOX 设备。**

---

## 2. 硬件连接

### 2.1 设备接口说明

```
┌─────────────────────────────────────┐
│          NOOX ESP32-S3              │
│                                     │
│  ┌────────┐                         │
│  │ OLED   │    ◉ RGB LED            │
│  │ 128x64 │                         │
│  └────────┘    ● LED1  ● LED2  ● LED3
│                                     │
│  [B1] [B2] [B3]  ← 按键              │
│  (OK) (↑)  (↓)                      │
│                                     │
│  [USB-C] ← 数据/电源                 │
└─────────────────────────────────────┘
```

### 2.2 按键定义

| 按键 | 名称 | GPIO | 功能 |
|------|------|------|------|
| B1 | Button A | GPIO47 | 确认/选择/进入菜单 |
| B2 | Button B | GPIO21 | 向上/向前翻页 |
| B3 | Button C | GPIO38 | 向下/向后翻页 |

**按键特性**:
- 高电平触发（按下时为 HIGH）
- 内部下拉电阻
- 硬件消抖延迟：200ms

### 2.3 LED 指示

#### RGB LED 状态指示

| 颜色 | 状态 |
|------|------|
| 🔵 蓝色 | 设备启动中 |
| 🟢 绿色 | WiFi 已连接 |
| 🟡 黄色 | WiFi 连接中 |
| 🔴 红色 | WiFi 连接失败 / 错误 |
| ⚪ 白色 | LLM 请求处理中 |
| ⚫ 熄灭 | 空闲 |

#### 单色 LED

- **LED 1/2/3**: 可通过 LLM 或 Web 界面控制

### 2.4 扩展接口（高级用户）

#### I2C 预留接口

| 引脚 | GPIO | 用途 |
|------|------|------|
| SDA | GPIO1 | I2C 数据线 |
| SCL | GPIO2 | I2C 时钟线 |

**用途示例**:
- 连接额外的传感器（温湿度、气压等）
- 连接第二块 OLED 显示屏
- 连接 I2C 模块（RTC、EEPROM 等）

#### SPI 接口 (TF 卡)

| 引脚 | GPIO | 用途 |
|------|------|------|
| CS | GPIO6 | 片选 |
| SCK | GPIO15 | 时钟 |
| MISO | GPIO16 | 主入从出 |
| MOSI | GPIO7 | 主出从入 |

**用途**:
- 扩展存储（日志记录）
- 数据采集缓存

#### UART 扩展

| 引脚 | GPIO | 用途 |
|------|------|------|
| TX | GPIO17 | 发送 |
| RX | GPIO18 | 接收 |

**用途**:
- 连接外部模块（GPS、蓝牙模块）
- 调试输出

#### 通用 GPIO

- GPIO8, GPIO9, GPIO10

**用途**:
- 可通过 LLM 控制的输出引脚
- 自定义传感器输入

---

## 3. 首次配置

### 3.1 配置 WiFi

#### 方法 1：通过 OLED 界面

**步骤**:

1. 开机后，设备显示状态屏幕
2. 按 **Button A (OK)** 进入主菜单
3. 选择 **WiFi Settings**，按 **OK**
4. 选择以下选项之一：
   - **Connect Saved**: 连接已保存的网络
   - **Scan Networks**: 扫描可用网络（未实现）
   - **Disconnect**: 断开当前连接

**连接已保存网络**:

1. 选择 **Connect Saved**
2. 使用 **Up/Down** 按键浏览网络列表
3. 选择目标网络，按 **OK**
4. 设备尝试连接，OLED 显示连接状态

#### 方法 2：编辑配置文件

**步骤**:

1. 设备通过 USB 连接后，会显示为 U 盘
2. 打开 `/config.json` 文件
3. 找到 `wifi_networks` 数组：

```json
"wifi_networks": [
  {
    "ssid": "YourWiFiName",
    "password": "YourPassword"
  },
  {
    "ssid": "OfficeWiFi",
    "password": "OfficePassword"
  }
]
```

4. 添加或修改网络信息
5. 保存文件
6. 重启设备（拔插 USB 或通过 OLED 菜单）

#### 方法 3：通过 Web 界面

**前提**: 设备已连接到 WiFi

1. 访问 Web 控制台
2. 点击 **设置** 按钮
3. 在 "WiFi 管理" 部分：
   - 点击 **添加网络**
   - 输入 SSID 和密码
   - 点击 **保存**
4. 选择网络并点击 **连接**

### 3.2 配置 LLM

#### 支持的 LLM 提供商

| 提供商 | 获取 API Key | 免费模型 |
|--------|--------------|----------|
| **DeepSeek** | [platform.deepseek.com](https://platform.deepseek.com) | ✅ deepseek-chat (200万tokens/天) |
| **OpenRouter** | [openrouter.ai](https://openrouter.ai) | ✅ 多种免费模型 |
| **OpenAI** | [platform.openai.com](https://platform.openai.com) | ❌ 需付费 |

#### 配置步骤

**方法 1：通过 Web 界面**

1. 访问 Web 控制台（http://设备IP地址）
2. 点击右上角 **设置** 图标 (⚙️)
3. 在 "LLM 配置" 部分：
   - **提供商**: 选择 DeepSeek / OpenRouter / OpenAI
   - **API Key**: 粘贴您的密钥
   - **模型**: 从下拉列表选择模型
4. 点击 **保存并应用**
5. 系统会重新加载配置

**方法 2：编辑配置文件**

编辑 `/config.json`：

```json
{
  "last_used": {
    "llm_provider": "deepseek",
    "model": "deepseek-chat",
    "wifi_ssid": "YourWiFi"
  },
  "llm_providers": {
    "deepseek": {
      "api_key": "sk-xxxxxxxxxxxxxxxxxxxxxxxxx",
      "models": ["deepseek-chat", "deepseek-reasoner"]
    },
    "openrouter": {
      "api_key": "sk-or-v1-xxxxxxxxxxxxxxxxxx",
      "models": [
        "z-ai/glm-4.5-air:free",
        "qwen/qwen3-235b-a22b:free",
        "deepseek/deepseek-chat-v3.1:free"
      ]
    },
    "openai": {
      "api_key": "",
      "models": ["gpt-4o", "gpt-3.5-turbo"]
    }
  }
}
```

**字段说明**:

- `last_used.llm_provider`: 当前使用的提供商
- `last_used.model`: 当前使用的模型
- `api_key`: API 密钥（必填）
- `models`: 可用模型列表

### 3.3 验证配置

#### 检查 WiFi 连接

**OLED 显示**:

```
Mode: Chat
Model: deepseek-chat
SSID: YourWiFi
IP: 192.168.1.100    ← 显示 IP 表示连接成功
Mem: 45% (280KB)
```

**串口输出**:

```
Connected to WiFi!
IP Address: 192.168.1.100
```

#### 测试 LLM 连接

1. 访问 Web 界面
2. 在聊天窗口输入：`你好`
3. 等待 AI 响应
4. 如果收到回复，说明配置正确

**常见错误**:

- `API key invalid`: API 密钥错误
- `Network timeout`: 网络连接问题
- `Model not found`: 模型名称错误

---

## 4. Web 界面使用

### 4.1 界面概览

```
┌─────────────────────────────────────────────────────────┐
│  NOOX Platform     [ 💬 Chat ] [ 🔧 Advanced ]    ⚙️    │ ← 顶栏
├─────────────────────────────────────────────────────────┤
│                                                         │
│  🤖 Bot: 你好！我是 NOOX AI 助手，有什么可以帮您的？  │
│                                                         │
│  👤 User: 帮我列出当前目录的文件                        │
│                                                         │
│  🤖 Bot: 正在执行命令...                                │
│         文件列表：                                      │
│         - config.json                                   │
│         - index.html.gz                                 │
│         - script.js.gz                                  │
│                                                         │
├─────────────────────────────────────────────────────────┤
│  [ 输入您的消息...                            ] [发送]  │ ← 输入框
└─────────────────────────────────────────────────────────┘
```

### 4.2 聊天功能

#### 发送消息

1. 在底部输入框输入您的问题或指令
2. 按 **Enter** 键或点击 **发送** 按钮
3. 消息会立即显示在聊天窗口
4. 等待 AI 响应（通常 2-15 秒）

#### 消息类型

**文本消息**:

```
User: 介绍一下 ESP32-S3 芯片
Bot: ESP32-S3 是乐鑫科技推出的...
```

**代码片段**:

````
User: 写一个 Python Hello World
Bot: 
```python
print("Hello, World!")
```
````

**列表内容**:

```
User: 列出 5 种编程语言
Bot:
1. Python
2. JavaScript
3. C++
4. Java
5. Go
```

### 4.3 模式切换

#### 聊天模式 (Chat Mode)

**特点**:
- 简单的问答交互
- 不调用工具
- 响应速度快
- 适合：闲聊、知识问答、文本生成

**示例对话**:

```
User: 什么是机器学习？
Bot: 机器学习是人工智能的一个分支...

User: 推荐一本关于 Python 的书
Bot: 我推荐《流畅的Python》...
```

#### 高级模式 (Advanced Mode)

**特点**:
- 支持工具调用（Shell 命令、HID 控制、GPIO）
- 可以执行复杂任务
- 响应时间较长
- 适合：系统自动化、文件操作、设备控制

**示例对话**:

```
User: 帮我在桌面创建一个名为 test.txt 的文件
Bot: [调用工具] execute_shell_command
     参数: {"command": "touch ~/Desktop/test.txt"}
     结果: 文件已成功创建

User: 打开记事本
Bot: [调用工具] control_keyboard
     参数: {"action": "press", "value": "Win+R"}
     参数: {"action": "type", "value": "notepad"}
     参数: {"action": "press", "value": "Enter"}
     结果: 记事本已打开
```

**切换方法**:

1. 点击顶栏的 **Chat** 或 **Advanced** 按钮
2. 当前模式会高亮显示
3. 模式切换会立即生效

### 4.4 设置面板

#### 打开设置

点击右上角的 **⚙️ 设置** 图标，左侧滑出设置面板。

#### 设置项说明

**1. LLM 配置**

```
┌─────────────────────────────┐
│  LLM 提供商                  │
│  ○ DeepSeek                 │
│  ● OpenRouter  ← 当前选中    │
│  ○ OpenAI                   │
│                             │
│  API Key                    │
│  [sk-or-v1-xxxx...]         │
│                             │
│  模型                       │
│  [qwen/qwen3-235b-a22b ▼]   │
│                             │
└─────────────────────────────┘
```

**2. WiFi 管理**

```
┌─────────────────────────────┐
│  已保存的网络                │
│  ✓ HomeWiFi (已连接)         │
│    OfficeWiFi                │
│    CafeWiFi                  │
│                             │
│  [+ 添加网络]                │
│                             │
│  操作：                      │
│  [连接] [断开] [删除]        │
│                             │
└─────────────────────────────┘
```

**添加网络**:

1. 点击 **添加网络**
2. 输入 SSID
3. 输入密码
4. 点击 **保存**

**连接网络**:

1. 在列表中选择网络
2. 点击 **连接**
3. 等待连接状态更新

**删除网络**:

1. 选择要删除的网络
2. 点击 **删除**
3. 确认操作

**3. 系统信息** (只读)

```
┌─────────────────────────────┐
│  固件版本: v3.0              │
│  芯片型号: ESP32-S3          │
│  Flash: 16 MB               │
│  PSRAM: 8 MB                │
│  WiFi: 192.168.1.100        │
│  运行时间: 02:15:30          │
│  内存使用: 45%               │
│                             │
└─────────────────────────────┘
```

#### 保存配置

1. 修改完设置后，点击底部的 **保存并应用**
2. 设备会：
   - 保存配置到 Flash
   - 重新初始化 LLM 管理器
   - 如果更改了 WiFi，会重新连接
3. 显示保存状态通知

### 4.5 对话历史管理

#### 查看历史

- 所有对话自动保存
- 向上滚动查看历史消息
- 最多保留 40 条消息（约 20 轮对话）

#### 清除历史

**为什么清除历史？**

- 对话主题切换
- 上下文混乱
- 减少 token 消耗

**操作方法**:

1. 在设置面板中找到 "对话历史"
2. 点击 **清除历史**
3. 确认操作
4. 所有历史消息被删除，开始新的对话

**注意**: 清除历史是全局的，会影响 Web 和 USB Shell 两个界面。

### 4.6 高级功能区（高级模式）

#### GPIO 控制面板

```
┌─────────────────────────────┐
│  GPIO 控制                   │
│                             │
│  LED 1    [ OFF ] [ ON  ]   │
│  LED 2    [ OFF ] [ ON  ]   │
│  LED 3    [ OFF ] [ ON  ]   │
│  GPIO 1   [ OFF ] [ ON  ]   │
│  GPIO 2   [ OFF ] [ ON  ]   │
│                             │
└─────────────────────────────┘
```

**使用方法**:

1. 点击 **ON** 按钮开启 GPIO
2. 点击 **OFF** 按钮关闭 GPIO
3. 状态会实时更新

**LLM 控制**:

在高级模式下，AI 可以自动控制 GPIO：

```
User: 打开 LED 1
Bot: [调用工具] control_gpio
     参数: {"gpio": "led1", "state": true}
     结果: LED 1 已开启
```

---

## 5. OLED 界面操作

### 5.1 状态显示屏幕

**默认屏幕**:

```
┌────────────────────┐
│Mode: Chat          │ ← LLM 模式
│Model: deepseek     │ ← 当前模型（简称）
│SSID: MyWiFi        │ ← WiFi 名称
│IP: 192.168.1.100   │ ← IP 地址
│Mem: 45% (280KB)    │ ← 内存使用
└────────────────────┘
```

**字段说明**:

- **Mode**: Chat (聊天模式) 或 Advanced (高级模式)
- **Model**: 当前使用的 LLM 模型名称
- **SSID**: 
  - 已连接：显示 WiFi 名称
  - 未连接：显示 "N/A"
- **IP**: 
  - 已连接：显示 IP 地址
  - 未连接：显示 WiFi 状态（Connecting / Failed）
- **Mem**: 
  - 百分比：已使用内存占总 DRAM 的百分比
  - KB：剩余可用内存

**操作**:

- 按 **Button A (OK)** 进入主菜单

### 5.2 主菜单

```
┌────────────────────┐
│Main Menu           │
│                    │
│> WiFi Settings     │ ← 当前选中
│  System Info       │
└────────────────────┘
```

**操作**:

- **Button B (Up)**: 向上移动选项
- **Button C (Down)**: 向下移动选项
- **Button A (OK)**: 进入选中的菜单项

**菜单项**:

1. **WiFi Settings**: 进入 WiFi 管理菜单
2. **System Info**: 返回状态显示屏幕

### 5.3 WiFi 管理菜单

```
┌────────────────────┐
│WiFi Menu           │
│                    │
│> Disconnect        │ ← 当前选中
│  Connect Saved     │
│  Scan Networks     │
└────────────────────┘
```

**菜单项**:

1. **Disconnect**: 断开当前 WiFi 连接
2. **Connect Saved**: 连接到已保存的网络
3. **Scan Networks**: 扫描可用网络（功能未实现）

**操作示例**:

**断开 WiFi**:

1. 选择 **Disconnect**
2. 按 **Button A (OK)**
3. 设备断开连接，返回状态屏幕
4. IP 字段显示 "WiFi: Idle"

**连接已保存网络**:

1. 选择 **Connect Saved**
2. 按 **Button A (OK)**
3. 进入已保存网络列表

### 5.4 已保存网络列表

```
┌────────────────────┐
│Saved WiFi Networks │
│                    │
│> HomeWiFi          │ ← 当前选中
│  OfficeWiFi        │
│  CafeWiFi          │
│                    │
└────────────────────┘
```

**操作**:

- **Button B/C**: 浏览网络列表
- **Button A (OK)**: 连接选中的网络

**连接过程**:

1. 按 **OK** 确认连接
2. 返回状态屏幕
3. WiFi 状态显示为 "Connecting"
4. 连接成功后显示 IP 地址
5. 连接失败后显示 "Failed"

**滚动显示**:

- 屏幕最多显示 4 个网络
- 如果网络超过 4 个，使用 **Up/Down** 按键滚动列表

---

## 6. USB Shell 交互

### 6.1 功能概述

USB Shell 允许您通过计算机的终端与 NOOX 设备进行命令行级别的交互，同时集成了 AI 能力。

**核心功能**:

1. **Shell 命令执行**: 在计算机上执行命令
2. **AI 对话**: 与 AI 进行自然语言交互
3. **无缝集成**: Shell 命令和 AI 回复在同一终端显示

### 6.2 主机代理程序

#### 什么是主机代理？

主机代理是一个运行在计算机上的原生可执行程序（Go 语言编写），负责：

- 连接 ESP32 的 CDC 虚拟串口
- 转发用户输入到设备
- 执行设备请求的 Shell 命令
- 显示 AI 响应

#### 安装主机代理

**方法 1：从设备获取**

1. 连接 NOOX 设备到计算机
2. 设备会显示为 U 盘（名称：NOOX Disk）
3. 在 U 盘中找到主机代理程序：
   - Windows: `agent.exe`
   - macOS: `agent_mac`
   - Linux: `agent_linux`
4. 将程序复制到您的计算机

**方法 2：从源码编译**

```bash
cd host-agent
go build -o agent main.go
```

#### 运行主机代理

**Windows**:

```powershell
# 双击运行或在命令行执行
.\agent.exe
```

**macOS / Linux**:

```bash
# 添加执行权限
chmod +x agent_mac

# 运行
./agent_mac
```

**自动启动** (实验性功能):

设备启动时，会通过 HID 模拟键盘自动运行代理程序：

```
Win+R → agent.exe --wifi-status=disconnected
```

### 6.3 使用 USB Shell

#### 启动会话

1. 确保设备通过 USB 连接到计算机
2. 运行主机代理程序
3. 代理会自动检测并连接到 ESP32 的 CDC 串口
4. 显示欢迎信息：

```
NOOX Host Agent starting...
Initial WiFi Status from ESP32: connected
Connected to ESP32 on /dev/ttyACM0
Performing initial device setup...
Link test successful!
Ready for input. Type your message or command:
```

#### 发送消息

**自然语言输入**:

```bash
> 你好，帮我列出当前目录的文件
```

**设备处理**:

```
[NOOX] 正在执行命令: ls -la
[Shell Output]
total 48
drwxr-xr-x  12 user  staff   384 Oct 12 10:30 .
drwxr-xr-x   6 user  staff   192 Oct 10 15:20 ..
-rw-r--r--   1 user  staff  1234 Oct 12 09:45 config.json
...

[NOOX AI] 当前目录包含以下文件：
- config.json
- index.html.gz
- ...
```

#### 直接执行命令

**询问 AI 执行命令**:

```bash
> 帮我创建一个名为 test.txt 的文件
```

**AI 响应**:

```
[NOOX] 正在执行命令: touch test.txt
[Shell Output] (命令成功执行，无输出)

[NOOX AI] 文件 test.txt 已成功创建。
```

#### 多轮对话

```bash
> 创建一个名为 data 的目录
[NOOX AI] 目录已创建。

> 进入这个目录
[NOOX] 正在执行命令: cd data
[NOOX AI] 已进入 data 目录。

> 在这里创建一个 readme.txt 文件
[NOOX] 正在执行命令: touch readme.txt
[NOOX AI] 文件已创建。

> 列出当前目录的文件
[NOOX] 正在执行命令: ls
[Shell Output]
readme.txt

[NOOX AI] 当前目录包含 1 个文件：readme.txt
```

### 6.4 通信协议

#### 消息格式

所有消息为 JSON 格式，以换行符结尾。

**用户输入消息** (主机 → ESP32):

```json
{
  "requestId": "uuid-user-1",
  "type": "userInput",
  "payload": "帮我列出文件"
}
```

**Shell 命令请求** (ESP32 → 主机):

```json
{
  "requestId": "uuid-shell-1",
  "type": "shellCommand",
  "payload": "ls -la"
}
```

**Shell 执行结果** (主机 → ESP32):

```json
{
  "requestId": "uuid-shell-1",
  "type": "shellCommandResult",
  "status": "success",
  "payload": {
    "command": "ls -la",
    "stdout": "文件列表...",
    "stderr": "",
    "exitCode": 0
  }
}
```

**AI 响应** (ESP32 → 主机):

```json
{
  "requestId": "uuid-ai-1",
  "type": "aiResponse",
  "payload": "当前目录包含以下文件：..."
}
```

### 6.5 高级用法

#### 链路测试

**目的**: 测试设备与主机的通信是否正常

**操作**:

主机代理启动时会自动发送 linkTest：

```json
{
  "requestId": "uuid-linktest-1",
  "type": "linkTest",
  "payload": "ping"
}
```

**设备响应**:

```json
{
  "requestId": "uuid-linktest-1",
  "type": "linkTestResult",
  "status": "success",
  "payload": "pong"
}
```

#### WiFi 自动配置

**场景**: 设备未连接 WiFi 时，主机代理尝试获取主机 WiFi 信息并发送给设备。

**流程**:

1. 设备启动时传递 WiFi 状态给代理：
   ```
   agent.exe --wifi-status=disconnected
   ```

2. 代理检测到 `disconnected`，获取主机 WiFi 信息

3. 发送连接请求：
   ```json
   {
     "requestId": "uuid-wifi-1",
     "type": "connectToWifi",
     "payload": {
       "ssid": "MyHomeWiFi",
       "password": "MyPassword"
     }
   }
   ```

4. 设备连接 WiFi 并响应：
   ```json
   {
     "requestId": "uuid-wifi-1",
     "type": "wifiConnectStatus",
     "status": "success",
     "payload": "Connected to MyHomeWiFi"
   }
   ```

---

## 7. LLM 功能详解

### 7.1 支持的提供商

#### DeepSeek

**官网**: https://platform.deepseek.com

**推荐模型**:
- `deepseek-chat`: 通用对话模型
- `deepseek-reasoner`: 深度推理模型

**获取 API Key**:

1. 注册账号
2. 进入 [API Keys](https://platform.deepseek.com/api_keys) 页面
3. 点击 "创建 API Key"
4. 复制密钥（格式：`sk-xxxxxx`）

#### OpenRouter

**官网**: https://openrouter.ai


**获取 API Key**:

1. 注册账号
2. 进入 [Keys](https://openrouter.ai/keys) 页面
3. 创建新密钥
4. 复制密钥（格式：`sk-or-v1-xxxxxx`）

#### OpenAI

**官网**: https://platform.openai.com


**获取 API Key**:

1. 注册账号（需要国际信用卡）
2. 进入 [API Keys](https://platform.openai.com/api-keys) 页面
3. 创建新密钥
4. 复制密钥（格式：`sk-xxxxxx`）

### 7.2 模式对比

| 特性 | 聊天模式 (Chat) | 高级模式 (Advanced) |
|------|-----------------|---------------------|
| **用途** | 日常对话、知识问答 | 系统自动化、任务执行 |
| **工具调用** | ❌ 不支持 | ✅ 支持 |
| **响应速度** | 快（2-5 秒） | 慢（5-15 秒） |
| **Token 消耗** | 低 | 高 |
| **系统提示词** | 简单友好 | 详细工具描述 |
| **适用场景** | 闲聊、咨询、创作 | 命令执行、设备控制 |

### 7.3 工具调用机制

#### 什么是工具调用？

工具调用（Function Calling）是 LLM 的一种能力，允许 AI 调用预定义的函数来完成任务。

**流程**:

```
用户: "帮我打开记事本"
    ↓
LLM 分析意图 → 需要调用 control_keyboard 工具
    ↓
LLM 返回工具调用：
{
  "tool": "control_keyboard",
  "args": {
    "action": "press",
    "value": "Win+R"
  }
}
    ↓
设备执行工具 → 模拟按键 Win+R
    ↓
设备返回执行结果 → "运行对话框已打开"
    ↓
LLM 继续调用工具输入 "notepad" 并回车
    ↓
设备返回 → "记事本已打开"
    ↓
LLM 最终回复: "记事本已成功打开。"
```

#### 可用工具

##### 1. execute_shell_command

**描述**: 在用户计算机上执行 Shell 命令

**参数**:

```json
{
  "command": "要执行的命令"
}
```

**示例**:

```json
{
  "tool": "execute_shell_command",
  "args": {
    "command": "ls -la"
  }
}
```

**适用场景**:
- 文件管理（创建、删除、移动、复制）
- 系统信息查询（磁盘使用、进程列表）
- 软件安装和配置
- 批量操作

##### 2. control_keyboard

**描述**: 模拟键盘输入文本或按键组合

**参数**:

```json
{
  "action": "type | press",
  "value": "文本内容或按键组合"
}
```

**示例**:

**输入文本**:

```json
{
  "tool": "control_keyboard",
  "args": {
    "action": "type",
    "value": "Hello, World!"
  }
}
```

**按键组合**:

```json
{
  "tool": "control_keyboard",
  "args": {
    "action": "press",
    "value": "Ctrl+C"
  }
}
```

**支持的按键**:
- 修饰键：Ctrl, Shift, Alt, Win (Cmd on macOS)
- 功能键：F1-F12
- 导航键：Home, End, PageUp, PageDown
- 方向键：Up, Down, Left, Right
- 特殊键：Enter, Tab, Backspace, Escape, Space

**适用场景**:
- 打开应用程序（Win+R）
- 复制粘贴操作
- 快捷键触发
- 自动化文本输入

##### 3. control_gpio

**描述**: 控制 ESP32 的 GPIO 输出状态

**参数**:

```json
{
  "gpio": "led1 | led2 | led3 | gpio1 | gpio2",
  "state": true | false
}
```

**示例**:

```json
{
  "tool": "control_gpio",
  "args": {
    "gpio": "led1",
    "state": true
  }
}
```

**适用场景**:
- 远程控制设备指示灯
- 触发外部继电器
- 控制自定义硬件

### 7.4 对话历史

#### 功能说明

NOOX 会自动保存最近的对话内容，使 AI 能够记住上下文，实现多轮对话。

**容量**: 40 条消息（约 20 轮对话）

**存储位置**: PSRAM（不占用 DRAM）

**共享性**: Web 界面和 USB Shell 共享同一历史记录

#### 示例

**有历史记录**:

```
User: 你喜欢黑塞吗？
AI: 是的，赫尔曼·黑塞是一位伟大的德国作家...

User: 讲讲他的作品
AI: 黑塞的代表作品包括《悉达多》、《荒原狼》...  ← AI 知道"他"指黑塞
```

**无历史记录**:

```
User: 你喜欢黑塞吗？
AI: 是的，赫尔曼·黑塞是一位伟大的德国作家...

[清除历史]

User: 讲讲他的作品
AI: 请问您想了解哪位作家的作品？  ← AI 不知道"他"是谁
```

#### 清除历史

**触发时机**:
- 对话主题切换
- 上下文混乱
- 希望减少 token 消耗

**操作方法**:

**Web 界面**:

1. 进入设置面板
2. 找到 "对话历史" 部分
3. 点击 **清除历史**
4. 确认操作

**WebSocket 命令**:

```json
{
  "type": "clear_history"
}
```

**效果**: 所有历史消息被删除，下一次对话将不包含任何上下文。

### 7.5 性能优化

#### Token 优化

**什么是 Token？**

Token 是 LLM 处理文本的基本单位，约等于 0.75 个英文单词或 1.5 个中文字符。

**Token 计算**:

```
"Hello World" ≈ 2 tokens
"你好世界" ≈ 4 tokens
```

**优化建议**:

1. **清除无关历史**: 定期清除对话历史
2. **使用简洁提问**: 避免冗长描述
3. **选择合适模型**: 
   - 简单任务：deepseek-chat
   - 复杂推理：deepseek-reasoner

#### 响应时间优化

| 因素 | 影响 | 优化方法 |
|------|------|----------|
| **网络延迟** | 中 | 使用稳定的 WiFi，选择地理位置近的提供商 |
| **模型大小** | 高 | 小模型响应更快（如 deepseek-chat） |
| **对话历史长度** | 中 | 定期清除历史 |
| **工具调用次数** | 高 | 高级模式下，复杂任务可能需要多次调用 |

#### 内存管理

**DRAM 使用**:

- 空闲：~80 KB
- LLM 调用：~180 KB
- 峰值：~200 KB

**PSRAM 使用**:

- 对话历史：~100 KB
- LLM 响应缓冲：~300 KB
- 峰值：~350 KB

**监控内存**:

OLED 显示屏实时显示内存使用情况：

```
Mem: 45% (280KB)
     ↑        ↑
  已使用%  剩余DRAM
```

**内存不足症状**:
- 设备频繁重启
- LLM 调用失败
- 响应延迟增加

**解决方法**:
- 清除对话历史
- 重启设备
- 减少并发 WebSocket 连接

---

## 8. 故障排除

### 8.1 WiFi 连接问题

#### 问题：设备无法连接 WiFi

**症状**:
- OLED 显示 "WiFi: Failed"
- 串口输出 "WiFi connection timed out"

**可能原因**:

1. **SSID 或密码错误**
   - 检查 `config.json` 中的 WiFi 配置
   - 注意区分大小写

2. **路由器仅支持 5GHz**
   - ESP32-S3 仅支持 2.4GHz WiFi
   - 确认路由器已启用 2.4GHz 频段

3. **WiFi 信号弱**
   - 将设备靠近路由器
   - 检查路由器天线

4. **路由器 MAC 地址过滤**
   - 查看设备 MAC 地址（串口输出）
   - 将 MAC 地址添加到路由器白名单

**解决方法**:

```bash
# 通过串口监视器检查日志
pio device monitor

# 检查连接状态
WiFi.begin(ssid, password);
// 观察串口输出
```

#### 问题：连接后频繁断线

**症状**:
- OLED 显示 IP 地址后又变为 "Connecting"
- 串口输出 "WiFi connection lost"

**可能原因**:

1. **路由器负载过高**
   - 减少同时连接的设备数量

2. **省电模式冲突**
   - WiFi 省电模式可能导致断连

**解决方法**:

在 `src/wifi_manager.cpp` 中禁用省电模式：

```cpp
void AppWiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);  // 禁用 WiFi 省电
    connectToLastSSID();
}
```

### 8.2 LLM API 调用失败

#### 问题：API 调用超时

**症状**:
- Web 界面长时间无响应
- 串口输出 "HTTPS request timeout"

**可能原因**:

1. **网络延迟高**
   - 测试网络速度：`ping api.deepseek.com`
   
2. **API 端点无法访问**
   - 尝试在浏览器访问 API 端点
   
3. **防火墙阻止**
   - 检查路由器防火墙设置

**解决方法**:

增加超时时间（`src/llm_manager.cpp`）：

```cpp
const unsigned long NETWORK_TIMEOUT = 60000;  // 增加到 60 秒
const unsigned long STREAM_TIMEOUT = 60000;
```

#### 问题：API Key 无效

**症状**:
- 串口输出 "API key invalid"
- HTTP 响应码 401

**解决方法**:

1. 登录提供商网站验证 API Key 是否有效
2. 检查 API Key 是否过期
3. 确认 API Key 格式正确（无多余空格）

#### 问题：模型不存在

**症状**:
- 串口输出 "Model not found"
- HTTP 响应码 404

**解决方法**:

1. 检查模型名称拼写
2. 确认提供商支持该模型
3. 更新模型列表（访问提供商文档）

### 8.3 OLED 显示问题

#### 问题：OLED 无显示

**可能原因**:

1. **I2C 接线错误**
   - 检查 SDA (GPIO4) 和 SCL (GPIO5) 连接
   
2. **OLED 地址错误**
   - 默认地址 0x3C，部分模块使用 0x3D

**解决方法**:

**I2C 扫描代码**:

```cpp
#include <Wire.h>

void scanI2C() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    
    Serial.println("Scanning I2C bus...");
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        byte error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("I2C device found at address 0x%02X\n", address);
        }
    }
    Serial.println("Scan complete.");
}
```

**修改 OLED 地址**:

在 `include/hardware_config.h` 中：

```cpp
#define OLED_ADDR 0x3D  // 修改为扫描到的地址
```

#### 问题：OLED 显示乱码

**可能原因**:

1. **字体不支持中文**
   - U8g2 默认字体仅支持英文

**解决方法**:

使用中文字体（`src/ui_manager.cpp`）：

```cpp
hardware.getDisplay().setFont(u8g2_font_wqy12_t_gb2312);  // 中文字体
```

### 8.4 USB Shell 问题

#### 问题：主机代理无法连接设备

**症状**:
- 代理程序输出 "Failed to connect to ESP32"

**可能原因**:

1. **CDC 驱动未安装** (Windows)
   - 检查设备管理器，是否显示 "未知设备"

2. **串口权限不足** (Linux/macOS)
   - 当前用户没有访问串口的权限

**解决方法**:

**Windows**:

1. 安装 CP2102 或 CH340 驱动（根据开发板）
2. 重新插拔 USB

**Linux**:

```bash
# 添加用户到 dialout 组
sudo usermod -a -G dialout $USER

# 注销并重新登录
```

**macOS**:

```bash
# 授予终端完全磁盘访问权限
系统偏好设置 → 安全性与隐私 → 完全磁盘访问权限 → 添加终端
```

#### 问题：Shell 命令无输出

**症状**:
- AI 响应 "命令已执行"，但没有输出

**可能原因**:

1. **命令本身无输出**
   - 例如 `cd` 命令成功执行但不产生输出

2. **命令执行失败**
   - 检查 exitCode 是否为 0

**解决方法**:

查看主机代理的详细日志：

```bash
./agent --debug
```

### 8.5 性能问题

#### 问题：设备频繁重启

**症状**:
- 串口输出 "Brownout detector was triggered"
- 设备在 LLM 调用时重启

**可能原因**:

1. **供电不足**
   - USB 端口电流不足（< 500mA）

2. **内存耗尽**
   - DRAM 使用率过高

**解决方法**:

**供电**:

- 使用质量好的 USB 电源适配器（5V 2A）
- 避免使用 USB Hub
- 更换 USB 数据线

**内存**:

- 清除对话历史
- 减少 WebSocket 连接数
- 重启设备

#### 问题：响应速度慢

**症状**:
- LLM 调用耗时超过 20 秒
- Web 界面卡顿

**可能原因**:

1. **网络延迟高**
2. **对话历史过长**
3. **模型选择不当**

**解决方法**:

1. **优化网络**:
   - 使用有线连接到路由器（通过 WiFi 桥接）
   - 选择地理位置近的 LLM 提供商

2. **清除历史**:
   - 定期清除对话历史

3. **选择快速模型**:
   - DeepSeek-chat 比 DeepSeek-reasoner 快
   - 避免使用超大模型

---

## 9. 最佳实践

### 9.1 日常使用建议

#### 1. 定期重启设备

**频率**: 每周 1 次

**原因**: 清理内存碎片，防止内存泄漏

**操作**: 拔插 USB 或通过 Web 界面重启

#### 2. 管理对话历史

**建议**:
- 对话主题切换时清除历史
- 长期使用后清除历史
- 感觉 AI 回复不对劲时清除历史

#### 3. 选择合适的模式

| 任务类型 | 推荐模式 |
|----------|----------|
| 闲聊、咨询 | 聊天模式 |
| 知识问答 | 聊天模式 |
| 文件操作 | 高级模式 |
| 系统控制 | 高级模式 |
| 设备控制 | 高级模式 |


#### 2. 调试技巧

**启用详细日志**:

```cpp
// 在 platformio.ini 中
build_flags = 
  -DCORE_DEBUG_LEVEL=5  // 0=无, 5=详细
```

**监控串口输出**:

```bash
pio device monitor --baud 115200 --filter direct
```

**使用断点调试**:

PlatformIO 支持 GDB 调试（需要 JTAG 适配器）

#### 3. 性能分析

**监控内存使用**:

```cpp
void printMemoryUsage() {
    Serial.printf("Free DRAM: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
}
```

**监控任务状态**:

```cpp
void printTaskInfo() {
    Serial.printf("Task: %s, Stack: %d, Priority: %d\n",
                  pcTaskGetName(NULL),
                  uxTaskGetStackHighWaterMark(NULL),
                  uxTaskPriorityGet(NULL));
}
```


---

**文档结束**

感谢使用 NOOX 智能硬件平台！如有任何问题或建议，请联系开发团队。

