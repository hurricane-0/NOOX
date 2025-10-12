# NOOX - ESP32-S3 智能 AI 助手平台

<p align="center">
  <strong>基于 ESP32-S3 的智能硬件平台，集成大语言模型能力</strong>
</p>

<p align="center">
  <a href="#特性">特性</a> •
  <a href="#快速开始">快速开始</a> •
  <a href="#文档">文档</a> •
  <a href="#硬件">硬件</a> •
  <a href="#贡献">贡献</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=flat-square" alt="Platform"/>
  <img src="https://img.shields.io/badge/Framework-Arduino-orange?style=flat-square" alt="Framework"/>
  <img src="https://img.shields.io/badge/IDE-PlatformIO-brightgreen?style=flat-square" alt="IDE"/>
  <img src="https://img.shields.io/badge/License-MIT-yellow?style=flat-square" alt="License"/>
</p>

---

## 📖 项目概述

NOOX 是一个基于 **ESP32-S3** 的智能硬件平台，提供以下核心能力：

- 🤖 **LLM 集成**：支持 DeepSeek、OpenRouter、OpenAI 等多种大语言模型
- 💬 **多模态交互**：Web 界面、USB Shell、OLED + 按键
- ⌨️ **设备控制**：USB HID 模拟键盘/鼠标，实现计算机自动化
- 🔧 **工具调用**：AI 可执行 Shell 命令、控制 GPIO、操作键盘鼠标
- 📱 **即插即用**：无需安装依赖，连接即可使用

### 典型应用场景

智能桌面助手 • 文件管理自动化 • 开发者工具 • 智能家居集成

---

## ✨ 核心特性

### 双模式 LLM 交互

- **聊天模式**：简单问答交互，适合日常对话和知识咨询
- **高级模式**：支持工具调用（Function Calling），可执行 Shell 命令、控制键盘鼠标、操作 GPIO

### 多通道交互

```
┌──────────────┬──────────────┬──────────────┬──────────────┐
│  Web 浏览器  │  OLED + 按键 │  USB Shell   │  USB HID     │
│  (WebSocket) │  (本地显示)  │  (终端交互)  │  (设备控制)  │
└──────────────┴──────────────┴──────────────┴──────────────┘
                              ↓
                        [ ESP32-S3 AI 核心 ]
```

### USB 复合设备

- **HID**：模拟键盘和鼠标
- **CDC**：虚拟串口通信
- **MSD**：模拟 U 盘，存放主机代理程序

### 智能特性

- **对话历史管理**：自动保存最近 40 条消息，支持上下文理解
- **动态配置**：运行时修改 LLM 提供商、模型和 WiFi 配置
- **内存优化**：使用 PSRAM 存储大对象，确保系统稳定运行

---

## 🚀 快速开始


### 快速部署

#### 1. 克隆并编译

```bash
git clone https://github.com/your-repo/NOOX.git
cd NOOX

# 安装 PlatformIO

# 编译和上传
pio run --target upload
pio run --target uploadfs  # 首次使用必须执行
```


#### 2. 访问 Web 界面

1. 查看 OLED 屏幕显示的 IP 地址（如 `192.168.1.100`）
2. 在浏览器中打开该地址
3. 开始与 AI 对话！

---

## 📚 文档

| 文档 | 说明 |
|------|------|
| [技术规格文档](docs/TECHNICAL_SPECIFICATION.md) | 完整的系统架构、API 参考、数据结构 |
| [用户指南](docs/USER_GUIDE.md) | 详细使用说明、故障排除、最佳实践 |
| [对话历史指南](docs/conversation_history_guide.md) | 对话历史功能详解 |
| [Shell 功能指南](shell_ai_feature_guide.md) | USB Shell 交互使用说明 |
| [硬件说明](hardware.md) | 引脚定义、接口说明 |

---

## 🔧 硬件规格

### 主控芯片

**ESP32-S3-WROOM-1**
- CPU: Xtensa® 双核 32 位 LX7 @ 240 MHz
- SRAM: 512 KB | Flash: 16 MB | PSRAM: 8 MB
- WiFi: 802.11 b/g/n (2.4 GHz) | USB: USB 2.0 OTG

### 外设接口

| 类型 | 接口 | 引脚 |
|------|------|------|
| OLED 显示 | I2C | GPIO4 (SDA), GPIO5 (SCL) |
| 按键输入 | GPIO | GPIO47 (OK), GPIO21 (Up), GPIO38 (Down) |
| LED 输出 | GPIO | GPIO41/40/39 (单色), GPIO48 (RGB WS2812) |
| I2C 扩展 | I2C | GPIO1 (SDA), GPIO2 (SCL) |
| TF 卡 | SPI | GPIO6/15/16/7 (CS/SCK/MISO/MOSI) |
| UART 扩展 | UART | GPIO17 (TX), GPIO18 (RX) |
| 通用 GPIO | GPIO | GPIO8, GPIO9, GPIO10 |

完整引脚定义见 [`hardware.md`](hardware.md)

---

## 💻 软件架构

### 系统架构

```
用户交互层 (Web/OLED/USB Shell/USB HID)
            ↓
应用管理层 (WebManager/UIManager/UsbShellManager/HIDManager)
            ↓
   [ LLMManager - AI 核心引擎 ]
            ↓
基础服务层 (WiFiManager/ConfigManager/HardwareManager)
            ↓
       ESP32-S3 硬件
```

### 核心模块

| 模块 | 职责 |
|------|------|
| **LLMManager** | LLM API 调用、对话历史、工具调用 |
| **WebManager** | Web 服务器、WebSocket 通信 |
| **UIManager** | OLED 显示、按键输入、菜单系统 |
| **HIDManager** | USB 键盘/鼠标模拟 |
| **UsbShellManager** | CDC 串口、JSON 消息处理 |

### 技术栈

Arduino Framework • PlatformIO • LittleFS • FreeRTOS • U8g2 • FastLED • ESPAsyncWebServer • ArduinoJson

---

## 🗺️ 开发路线图

### ✅ 已完成

- 基础硬件驱动、WiFi 管理、LLM 集成
- Web 界面、USB 复合设备、对话历史
- 工具调用（Shell/HID/GPIO）、内存优化

### 🚧 进行中

- Web 身份验证、OTA 更新、日志记录

### 📋 计划中

- 语音输入/输出、图像识别、本地 LLM
- Python/Node.js SDK、桌面/移动客户端
- 完整的 REST API、视频教程

---

## 🤝 贡献

欢迎贡献代码、报告问题或提出建议！

### 快速贡献

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'feat: 添加某功能'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码规范

- 使用 4 空格缩进
- 遵循 Arduino 代码风格
- 使用 [Conventional Commits](https://www.conventionalcommits.org/) 提交规范
- 确保代码可编译且在硬件上测试通过

---

## ❓ 常见问题

<details>
<summary><b>无法连接 WiFi</b></summary>

- 确认是 2.4GHz WiFi（不支持 5GHz）
- 检查 SSID 和密码大小写
- 查看串口日志排查问题
</details>

<details>
<summary><b>LLM API 调用失败</b></summary>

- 验证 API Key 有效性
- 检查网络连接稳定性
- 查看串口输出的 HTTP 错误码
</details>

<details>
<summary><b>OLED 无显示</b></summary>

- 检查 I2C 接线（SDA=GPIO4, SCL=GPIO5）
- 确认 OLED 地址为 0x3C
- 运行 I2C 扫描代码检测
</details>

<details>
<summary><b>内存不足导致重启</b></summary>

- 清除对话历史
- 使用质量好的 USB 电源（5V 2A）
- 减少 WebSocket 连接数
</details>

更多问题请查看 [用户指南 - 故障排除](docs/USER_GUIDE.md#9-故障排除)


---

## 🙏 致谢

感谢以下开源项目和服务：

**开源库**: [ESP32 Arduino](https://github.com/espressif/arduino-esp32) • [PlatformIO](https://platformio.org/) • [U8g2](https://github.com/olikraus/u8g2) • [FastLED](https://github.com/FastLED/FastLED) • [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) • [ArduinoJson](https://arduinojson.org/)


<p align="center">
  如果这个项目对您有帮助，请给我们一个 ⭐️
</p>
