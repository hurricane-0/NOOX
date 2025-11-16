// NOOX 主机代理程序
// 该程序负责与 ESP32-S3 设备通过 CDC 串口进行通信，实现Shell命令执行和AI交互功能
package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"strings"
	"sync"
	"time"

	"github.com/google/uuid"

	"go.bug.st/serial"
	serial_enumerator "go.bug.st/serial/enumerator"
)

const defaultBanner = `
███╗   ██╗ ██████╗  ██████╗ ██╗  ██╗
████╗  ██║██╔═══██╗██╔═══██╗╚██╗██╔╝
██╔██╗ ██║██║   ██║██║   ██║ ╚███╔╝ 
██║╚██╗██║██║   ██║██║   ██║ ██╔██╗ 
██║ ╚████║╚██████╔╝╚██████╔╝██╔╝ ██╗
╚═╝  ╚═══╝ ╚═════╝  ╚═════╝ ╚═╝  ╚═╝
`

// loadBanner returns the in-code default banner (banner.txt is not used)
func loadBanner() string {
	return defaultBanner
}

func displayStartupInfo() {
	fmt.Println()
	fmt.Println(loadBanner())
	fmt.Println()
	fmt.Println("Connected and initialized. You can now type commands to send to the device.")
	fmt.Println("Tips:")
	fmt.Println(" - Type any text and press Enter to send a userInput message to the device.")
	fmt.Println(" - To execute a shell command on the host from the device, the device will send a 'shellCommand' message.")
	fmt.Println(" - To exit this agent, press Ctrl+C.")
	fmt.Println()
}

// 以下是与 ESP32 通信所使用的 JSON 消息结构体定义
// Shell命令输出的负载结构体
// 包含命令本身、标准输出、标准错误、执行状态和退出码
type ShellOutputPayload struct {
	Command  string `json:"command,omitempty"`  // 执行的命令
	Stdout   string `json:"stdout,omitempty"`   // 标准输出
	Stderr   string `json:"stderr,omitempty"`   // 标准错误
	Status   string `json:"status,omitempty"`   // 执行状态
	ExitCode int    `json:"exitCode,omitempty"` // 命令退出码
}

// runCommand请求的负载结构体
// 包含要执行的命令和可选的shell类型
type RunCommandPayload struct {
	Command string `json:"command"` // 要执行的命令
	Shell   string `json:"shell,omitempty"` // Shell类型（如powershell, pwsh, cmd, bash等）
}

// 主机发送给 ESP32 的通用消息结构体
// 使用 interface{} 作为 Payload 以支持不同类型的负载
type HostMessage struct {
	RequestId string      `json:"requestId"` // 请求ID，用于跟踪消息
	Type      string      `json:"type"`      // 消息类型
	Payload   interface{} `json:"payload,omitempty"`
}

// ESP32 响应的消息结构体
type ESP32Response struct {
	RequestId string      `json:"requestId"` // 对应请求的ID
	Type      string      `json:"type"`      // 响应类型
	Payload   interface{} `json:"payload,omitempty"`
	Status    string      `json:"status,omitempty"`  // 响应状态
	Content   string      `json:"content,omitempty"` // 响应内容
}

var (
	serialPort serial.Port // 串口通信接口
	portName   string      // 串口设备名称
	mu         sync.Mutex  // 串口写入互斥锁
)

// 主函数
func main() {
	log.Println("NOOX Host Agent starting...")

	// 首先尝试自动发现并连接 ESP32
	err := connectToESP32()
	if err != nil {
		log.Printf("Auto-discovery failed: %v", err)

		// 提示用户输入串口端口名（在启动阶段，readFromStdin 尚未运行，因此可以安全读取）
		reader := bufio.NewReader(os.Stdin)
		for {
			fmt.Print("Enter serial port (e.g., COM3 or /dev/ttyUSB0): ")
			input, _ := reader.ReadString('\n')
			input = strings.TrimSpace(input)
			if input == "" {
				continue
			}
			portName = input
			mode := &serial.Mode{
				BaudRate: 115200,
				Parity:   serial.NoParity,
				DataBits: 8,
				StopBits: serial.OneStopBit,
			}
			serialPort, err = serial.Open(portName, mode)
			if err != nil {
				log.Printf("Failed to open serial port %s: %v", portName, err)
				continue
			}
			break
		}
	}

	defer serialPort.Close()
	log.Printf("Connected to ESP32 on %s", portName)

	// Display friendly startup banner and tips immediately after connection
	displayStartupInfo()

	go readFromESP32()

	performInitialDeviceSetup()

	go readFromStdin()

	select {}
}

func generateUUID() string {
	return uuid.New().String()
}

// 执行初始设备设置
// 包括通信测试和WiFi连接状态检查
func performInitialDeviceSetup() {
	log.Println("Performing initial device setup...")

	// 发送链路测试请求
	linkTestReq := HostMessage{
		RequestId: generateUUID(),
		Type:      "linkTest",
		Payload:   "ping",
	}
	sendToESP32(linkTestReq)

	// 等待一秒以确保链路测试完成
	time.Sleep(1 * time.Second)
}

// WiFi connect functionality removed from host agent.

// 连接到ESP32设备
// 该函数会枚举所有串口设备，查找并连接到ESP32-S3的CDC串口
func connectToESP32() error {
	// 获取系统中所有串口设备的详细信息
	ports, err := serial_enumerator.GetDetailedPortsList()
	if err != nil {
		return fmt.Errorf("failed to enumerate serial ports: %w", err)
	}

	// 遍历所有串口，查找ESP32-S3设备
	// 通过设备名称、产品描述或VID来识别目标设备
	for _, p := range ports {
		if strings.Contains(p.Product, "ESP32-S3") || strings.Contains(p.Product, "USB Serial Device") || strings.Contains(p.VID, "303A") {
			log.Printf("Found potential ESP32-S3: %s (%s)", p.Name, p.Product)
			portName = p.Name
			break
		}
	}

	// 如果没有找到合适的设备，返回错误
	if portName == "" {
		return fmt.Errorf("no ESP32-S3 CDC serial port found; please ensure the device is connected and drivers are installed")
	}

	mode := &serial.Mode{
		BaudRate: 115200,
		Parity:   serial.NoParity,
		DataBits: 8,
		StopBits: serial.OneStopBit,
	}

	serialPort, err = serial.Open(portName, mode)
	if err != nil {
		return fmt.Errorf("failed to open serial port %s: %w", portName, err)
	}
	return nil
}

// 从ESP32读取数据的后台协程
// 持续监听串口数据，解析JSON消息并处理响应
func readFromESP32() {
	reader := bufio.NewReader(serialPort)
	for {
		// 按行读取串口数据（以换行符为分隔）
		line, err := reader.ReadString('\n')
		if err != nil {
			if err != io.EOF {
				log.Printf("Error reading from serial port: %v", err)
			}
			return
		}

		// 去除行首尾的空白字符
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		// 将收到的JSON数据解析为ESP32Response结构体
		var espResponse ESP32Response
		err = json.Unmarshal([]byte(line), &espResponse)
		if err != nil {
			log.Printf("Error unmarshalling ESP32 response: %v, Raw: %s", err, line)
			fmt.Printf("[NOOX Device] Raw: %s\n", line)
			continue
		}

		// 处理解析后的响应消息
		handleESP32Response(espResponse)
	}
}

// 处理来自ESP32的响应消息
// 根据消息类型执行相应的操作：执行Shell命令、显示AI回复等
func handleESP32Response(resp ESP32Response) {
	switch resp.Type {
	case "shellCommand":
		// 处理Shell命令请求（旧版兼容）
		// Payload应该是一个需要在本地执行的命令字符串
		command, ok := resp.Payload.(string)
		if !ok {
			log.Printf("Error: shellCommand payload is not a string: %v", resp.Payload)
			return
		}
		fmt.Printf("[NOOX Shell] Executing: %s\n", command)
		executeLocalShellCommand(command)
	case "runCommand":
		// 处理runCommand请求（支持指定shell类型）
		// Payload是一个包含command和可选shell字段的对象
		payloadBytes, err := json.Marshal(resp.Payload)
		if err != nil {
			log.Printf("Error marshalling runCommand payload: %v", err)
			return
		}
		var runCmdPayload RunCommandPayload
		err = json.Unmarshal(payloadBytes, &runCmdPayload)
		if err != nil {
			log.Printf("Error unmarshalling runCommand payload: %v", err)
			return
		}
		if runCmdPayload.Command == "" {
			log.Printf("Error: runCommand payload missing command field")
			return
		}
		shellInfo := "auto"
		if runCmdPayload.Shell != "" {
			shellInfo = runCmdPayload.Shell
		}
		fmt.Printf("[NOOX Shell] Executing: %s (shell: %s)\n", runCmdPayload.Command, shellInfo)
		executeLocalShellCommandWithShell(runCmdPayload.Command, runCmdPayload.Shell, resp.RequestId)
	case "aiResponse":
		// 处理AI回复消息
		// Payload应该是AI生成的回复文本
		aiResponse, ok := resp.Payload.(string)
		if !ok {
			log.Printf("Error: aiResponse payload is not a string: %v", resp.Payload)
			return
		}
		fmt.Printf("[NOOX AI] %s\n", aiResponse)
	case "linkTestResult":
		// Payload is the linkTest result string (e.g., "pong")
		linkTestResult, ok := resp.Payload.(string)
		if !ok {
			log.Printf("Error: linkTestResult payload is not a string: %v", resp.Payload)
			return
		}
		fmt.Printf("[NOOX Device] Link Test Result (RequestId: %s): %s - %s\n", resp.RequestId, resp.Status, linkTestResult)
	case "error":
		// Generic error from ESP32
		errMsg := ""
		if resp.Content != "" {
			errMsg = resp.Content
		} else if resp.Payload != nil {
			errMsg = fmt.Sprintf("%v", resp.Payload)
		} else {
			errMsg = "Unknown error from ESP32"
		}
		fmt.Printf("[NOOX Device Error] (RequestId: %s) %s\n", resp.RequestId, errMsg)
	default:
		fmt.Printf("[NOOX Device] Unknown response type: %s (RequestId: %s, Status: %s, Content: %s, Payload: %v)\n",
			resp.Type, resp.RequestId, resp.Status, resp.Content, resp.Payload)
	}
}

// 从标准输入读取用户输入的后台协程
// 将用户输入转发给ESP32设备
func readFromStdin() {
	scanner := bufio.NewScanner(os.Stdin)
	for {
		fmt.Print("noox> ")
		if !scanner.Scan() {
			break
		}
		// 获取用户输入
		input := scanner.Text()
		if input == "" {
			continue
		}

		// 构造用户输入消息并发送给ESP32
		msg := HostMessage{
			RequestId: generateUUID(), // 生成唯一的请求ID
			Type:      "userInput",    // 消息类型为用户输入
			Payload:   input,          // 消息内容为用户输入的文本
		}
		sendToESP32(msg)
	}

	// 处理可能的扫描错误
	if err := scanner.Err(); err != nil {
		log.Printf("Error reading from stdin: %v", err)
	}
}

// 在本地执行Shell命令（使用默认shell，向后兼容）
// 根据操作系统类型选择合适的Shell，执行命令并收集输出结果
func executeLocalShellCommand(command string) {
	executeLocalShellCommandWithShell(command, "", generateUUID())
}

// 在本地执行Shell命令（支持指定shell类型）
// 根据指定的shell类型或操作系统类型选择合适的Shell，执行命令并收集输出结果
func executeLocalShellCommandWithShell(command string, shell string, requestId string) {
	var cmd *exec.Cmd

	// 如果指定了shell类型，使用指定的shell
	if shell != "" {
		shellLower := strings.ToLower(shell)
		switch shellLower {
		case "pwsh":
			// PowerShell Core (跨平台)
			cmd = exec.Command("pwsh", "-Command", command)
		case "powershell":
			// Windows PowerShell
			cmd = exec.Command("powershell", "-Command", command)
		case "cmd":
			// CMD (Windows)
			cmd = exec.Command("cmd", "/C", command)
		case "bash":
			// Bash (Unix/Linux/Mac)
			cmd = exec.Command("bash", "-c", command)
		case "sh":
			// Sh (Unix/Linux)
			cmd = exec.Command("sh", "-c", command)
		default:
			// 未知的shell类型，回退到自动检测
			log.Printf("Warning: Unknown shell type '%s', falling back to auto-detection", shell)
			shell = ""
		}
	}

	// 如果没有指定shell或指定失败，根据操作系统自动选择
	if cmd == nil {
		if os.Getenv("OS") == "Windows_NT" {
			// Windows系统默认使用cmd.exe
			cmd = exec.Command("cmd", "/C", command)
		} else {
			// Unix类系统默认使用bash
			cmd = exec.Command("bash", "-c", command)
		}
	}

	// 创建用于捕获命令输出的缓冲区
	var stdout, stderr strings.Builder
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run()

	status := "success"
	exitCode := 0
	if err != nil {
		status = "error"
		if exitError, ok := err.(*exec.ExitError); ok {
			exitCode = exitError.ExitCode()
		} else {
			exitCode = 1
		}
		log.Printf("Error executing command '%s': %v", command, err)
	}

	// 在终端显示命令输出，方便用户查看
	if stdout.Len() > 0 {
		fmt.Printf("[NOOX Shell Output]\n%s\n", stdout.String())
	}
	if stderr.Len() > 0 {
		fmt.Printf("[NOOX Shell Error]\n%s\n", stderr.String())
	}
	if status == "error" {
		fmt.Printf("[NOOX Shell] Command failed with exit code: %d\n", exitCode)
	}

	// 限制输出大小，防止CDC缓冲区溢出（限制为20KB，更保守）
	const maxOutputSize = 20 * 1024
	stdoutStr := stdout.String()
	stderrStr := stderr.String()
	
	if len(stdoutStr) > maxOutputSize {
		stdoutStr = stdoutStr[:maxOutputSize] + "\n\n... (output truncated, too large - " + fmt.Sprintf("%d bytes", stdout.Len()) + " total)"
		log.Printf("Warning: stdout truncated from %d to %d bytes", stdout.Len(), maxOutputSize)
	}
	if len(stderrStr) > maxOutputSize {
		stderrStr = stderrStr[:maxOutputSize] + "\n\n... (output truncated, too large - " + fmt.Sprintf("%d bytes", stderr.Len()) + " total)"
		log.Printf("Warning: stderr truncated from %d to %d bytes", stderr.Len(), maxOutputSize)
	}

	shellPayload := ShellOutputPayload{
		Command:  command,
		Stdout:   stdoutStr,
		Stderr:   stderrStr,
		Status:   status,
		ExitCode: exitCode,
	}

	hostMsg := HostMessage{
		RequestId: requestId,
		Type:      "shellCommandResult",
		Payload:   shellPayload,
	}

	sendToESP32(hostMsg)
}

// 向ESP32发送消息
// 将消息结构体转换为JSON格式并通过串口发送
// 使用互斥锁确保串口写入的线程安全
func sendToESP32(msg HostMessage) {
	// 将消息转换为JSON格式
	jsonData, err := json.Marshal(msg)
	if err != nil {
		log.Printf("Error marshalling host message: %v", err)
		return
	}

	// 获取互斥锁，确保串口写入的线程安全
	mu.Lock()
	defer mu.Unlock()

	// 发送JSON数据，并在末尾添加换行符
	_, err = serialPort.Write(append(jsonData, '\n'))
	if err != nil {
		log.Printf("Error writing to serial port: %v", err)
	}
}
