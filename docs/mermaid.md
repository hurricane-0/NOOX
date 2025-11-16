## 架构图
flowchart LR
  %% Detailed module interaction map (Browser and Cloud split into separate blocks)
  %% Note: texts are wrapped in quotes and use <br/> for line breaks

  subgraph "ESP32-S3 Device"
    direction TB

    subgraph "Storage (LittleFS)"
      LFS_CONF["/config.json"]
      LFS_WEB["/index.html.gz<br/>/style.css.gz<br/>/script.js.gz"]
      LFS_AGENT["/agent/noox-host-agent(.exe|-macos|-linux)"]
    end

    subgraph "Core Managers"
      CFGM["ConfigManager<br/>- load/save config.json"]
      WIFIM["WiFiManager<br/>- connect/disconnect<br/>- status/IP"]
      HWM["HardwareManager<br/>- GPIO/LEDs/OLED bus"]
      HIDM["HIDManager<br/>- keyboard/mouse macros<br/>- PowerShell bootstrap"]
      USM["UsbShellManager<br/>- USB CDC JSON bridge<br/>- input buffer 64KB cap"]
      WM["WebManager (HTTP+WS)<br/>- /, /style.css, /script.js<br/>- /api/config (GET/POST)<br/>- /api/wifi/(connect|disconnect|delete)<br/>- /api/agent/download?platform=..."]
      LLMM["LLMManager<br/>- OpenAI-compatible HTTP(S)<br/>- tools: run_command, hid_keyboard_*, gpio_set<br/>- history ring buffer (40 msgs)"]
    end

    subgraph "FreeRTOS Tasks"
      T_WEB["WebTask<br/>loop: WebManager.loop()<br/>period ~10ms"]
      T_USB["USBTask<br/>loop: UsbShellManager.loop()<br/>period ~3ms"]
      T_UI["UITask<br/>loop: UIManager.update()<br/>period ~10ms"]
      T_LLM["LLMTask<br/>loop: LLMManager.loop()<br/>period ~10ms"]
    end

    subgraph "Queues"
      Q_REQ["llmRequestQueue (depth 3)"]
      Q_RES["llmResponseQueue (depth 3)"]
    end

    UIM["UIManager<br/>- OLED + Buttons<br/>- menus/status"]
  end

  subgraph "Host Computer"
    direction TB
    HOST_OS["Host OS"]
    AGENT["NOOX Host Agent<br/>(noox-agent.exe)"]
    SHELL["Host Shell<br/>(powershell/pwsh/cmd/bash/sh)"]
    TERM["Terminal Window<br/>(user sees logs)"]
  end

  subgraph "Web Browser"
    direction TB
    BROWSER["Web Client"]
  end

  subgraph "Cloud Providers"
    direction TB
    LLM_API["LLM Providers<br/>(OpenRouter / DeepSeek / OpenAI)"]
  end

  %% Filesystem relations
  CFGM --- LFS_CONF
  WM --- LFS_WEB
  WM --- LFS_AGENT

  %% Config dependencies
  CFGM <--> WIFIM
  CFGM <--> LLMM

  %% Task bindings
  T_WEB --> WM
  T_USB --> USM
  T_UI --> UIM
  T_LLM --> LLMM

  %% UI interactions
  UIM --> HWM
  UIM --> WIFIM
  UIM --> LLMM

  %% Web interactions (Browser ↔ WebManager)
  BROWSER <--> |"HTTP: '/', '/style.css', '/script.js'"| WM
  BROWSER <--> |"WebSocket: '/ws'"| WM

  %% REST APIs
  BROWSER --> |"GET '/api/config'"| WM
  BROWSER --> |"POST '/api/config' (deferred apply)"| WM
  BROWSER --> |"POST '/api/wifi/connect' (ssid,password)"| WM
  BROWSER --> |"POST '/api/wifi/disconnect'"| WM
  BROWSER --> |"POST '/api/wifi/delete' (ssid)"| WM

  %% WebManager to managers
  WM --> |"get/set config"| CFGM
  WM --> |"wifi ops"| WIFIM
  WM --> |"broadcast LLM responses via WS"| BROWSER

  %% HID bootstrap for Agent download
  HIDM --> |"Win+R → 'powershell'"| HOST_OS
  HOST_OS --> |"PowerShell window"| TERM
  HIDM --> |"Type Invoke-WebRequest<br/>GET '/api/agent/download?platform=windows'<br/>→ $env:TEMP\\noox-agent.exe"| HOST_OS
  HOST_OS --> |"Launch agent: & noox-agent.exe"| AGENT

  %% Agent downloads binary via WebManager
  AGENT --> |"HTTP GET '/api/agent/download?platform=...'"| WM
  WM --> |"serve binary (LittleFS)"| AGENT

  %% USB CDC JSON channel (Agent ↔ UsbShellManager)
  AGENT <--> |"USB CDC JSON<br/>{requestId,type,payload}\n"| USM

  %% Typical message types
  USM --> |"aiResponse"| AGENT
  USM --> |"runCommand (payload:{command,shell?})"| AGENT
  AGENT --> |"shellCommandResult (payload:{command,stdout,stderr,status,exitCode})"| USM
  AGENT <--> |"linkTest / linkTestResult"| USM
  AGENT --> |"userInput (from terminal)"| USM

  %% Agent executes commands on Host
  AGENT --> |"exec command"| SHELL
  SHELL --> |"stdout/stderr/exitCode"| AGENT
  AGENT --> |"print logs & AI reply"| TERM

  %% LLM flow
  USM --> |"processUserInput/processShellOutput"| LLMM
  LLMM --> |"HTTPS chat completions"| LLM_API
  LLM_API --> LLMM
  LLMM --> |"tool_calls: run_command/hid_keyboard_*/gpio_set"| USM
  LLMM --> |"status updates"| UIM

  %% WiFi status and IP exposure
  WIFIM --> |"IP/status"| WM
  WIFIM --> |"IP/status"| UIM

  %% Hardware control path from tools
  LLMM --> |"gpio_set"| HWM
  LLMM --> |"hid keyboard actions"| HIDM

## LLM交互时序图
sequenceDiagram
  title "USB Shell AI runCommand 时序（初始化后）"
  participant U as "User"
  participant A as "Host Agent"
  participant E as "ESP32 UsbShellManager"
  participant L as "LLMManager"
  participant S as "Host OS Shell"
  participant C as "LLM API (Cloud)"

  opt 初始化握手（可选）
    A->>E: {type:"linkTest"}
    E-->>A: {type:"linkTestResult", status:"success", payload:"pong"}
  end

  U->>A: 在终端输入自然语言
  A->>E: {type:"userInput", requestId, payload:text}
  E->>L: processUserInput(requestId, text)

  par LLM 决策
    L->>C: HTTPS Chat Completions (含系统提示/历史)
    C-->>L: 响应（含 tool_calls）
  end

  L-->>E: tool_calls → run_command(command, shell?)
  E->>A: {type:"runCommand", payload:{command, shell?}}
  A->>S: 执行命令(command, shell?)
  S-->>A: stdout/stderr/exitCode
  A->>E: {type:"shellCommandResult", payload:{command, stdout, stderr, status, exitCode}}
  E->>L: processShellOutput(requestId, command, stdout, stderr, status, exitCode)

  par 结果分析
    L->>C: HTTPS（带入命令输出作为上下文）
    C-->>L: 自然语言回复/后续工具调用
  end

  alt 直接回复
    L-->>E: aiResponse(text)
    E->>A: {type:"aiResponse", payload:text}
    A-->>U: 打印 AI 回复（并展示命令输出摘要）
  else 继续行动
    L-->>E: 下一步 tool_calls（例如再次 run_command / HID / gpio_set）
    E-->>A: 相应 JSON 指令（循环继续）
  end

## 代理下载与启动时序图
sequenceDiagram
  title "代理下载与启动时序（Web 拉取架构）"
  participant D as "ESP32 HIDManager"
  participant W as "ESP32 WebManager"
  participant E as "ESP32 UsbShellManager (CDC)"
  participant OS as "Host OS (Windows)"
  participant PS as "PowerShell"
  participant A as "NOOX Host Agent\n(noox-agent.exe)"

  rect rgb(40,49,51)
    note over D,W: 设备启动
    D->>W: 查询 WiFi 状态
    alt WiFi 未连接
      note over D,W: 建议通过 Web 配置或 HID 注入 SSID|Password
    else 已连接
      note over D: 继续下载流程
    end
  end

  rect rgb(40,49,51)
    note over D,OS: HID 键盘自动化启动
    D->>OS: Win+R 打开运行窗口
    D->>OS: 输入 "powershell" 并回车
    OS->>PS: 启动 PowerShell 窗口
  end

  rect rgb(40,49,51)
    note over PS,W: 通过 Web 下载代理
    PS->>W: HTTP GET /api/agent/download?platform=windows
    W-->>PS: 200 application/octet-stream (noox-agent.exe)
    PS->>OS: 保存到 $env:TEMP\noox-agent.exe
    PS->>OS: 执行 & $env:TEMP\noox-agent.exe
    OS->>A: 启动代理进程
    PS-->>OS: exit（可选，关闭窗口）
  end

  rect rgb(40,49,51)
    note over A,E: CDC 握手与测试
    A->>E: {type:"linkTest", requestId}
    E-->>A: {type:"linkTestResult", status:"success", payload:"pong"}
    note over A: 进入交互循环，监听终端输入与设备指令
  end