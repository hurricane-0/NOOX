// NOOX 主机代理程序
// 该程序负责与 ESP32-S3 设备通过 CDC 串口进行通信，实现Shell命令执行和AI交互功能
package main

import (
	"bufio"
	"encoding/json"
	"flag"
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

// 主机发送给 ESP32 的通用消息结构体
// 使用 interface{} 作为 Payload 以支持不同类型的负载
type HostMessage struct {
	RequestId string      `json:"requestId"` // 请求ID，用于跟踪消息
	Type      string      `json:"type"`      // 消息类型
	Payload   interface{} `json:"payload,omitempty"`
}

// WiFi连接请求的负载结构体
type ConnectToWifiPayload struct {
	SSID     string `json:"ssid"`     // WiFi名称
	Password string `json:"password"` // WiFi密码
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
	wifiStatus string      // WiFi连接状态
)

// 初始化函数，设置命令行参数
func init() {
	// 添加 wifi-status 命令行参数，用于指定 ESP32 的初始 WiFi 状态
	flag.StringVar(&wifiStatus, "wifi-status", "unknown", "Initial WiFi status from ESP32 (connected/disconnected/unknown)")
}

// 主函数
func main() {
	flag.Parse() // 解析命令行参数
	log.Println("NOOX Host Agent starting...")
	log.Printf("Initial WiFi Status from ESP32: %s", wifiStatus)

	err := connectToESP32()
	if err != nil {
		log.Fatalf("Failed to connect to ESP32: %v", err)
	}
	defer serialPort.Close()
	log.Printf("Connected to ESP32 on %s", portName)

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

	// 检查WiFi连接状态
	// 如果未连接，尝试获取主机WiFi信息并发送给ESP32
	if wifiStatus == "disconnected" {
		log.Println("ESP32 reported WiFi disconnected. Attempting to get host WiFi info and send to ESP32.")
		ssid, password, err := getWifiInfoFromHost()
		if err != nil {
			log.Printf("Failed to get host WiFi info: %v. Skipping WiFi connection attempt.", err)
			return
		}

		log.Printf("Sending connectToWifi to ESP32 for SSID: %s", ssid)
		connectWifiReq := HostMessage{
			RequestId: generateUUID(),
			Type:      "connectToWifi",
			Payload: ConnectToWifiPayload{
				SSID:     ssid,
				Password: password,
			},
		}
		sendToESP32(connectWifiReq)
	} else {
		log.Printf("ESP32 reported WiFi status: %s. No host WiFi connection needed.", wifiStatus)
	}
}

func getWifiInfoFromHost() (ssid, password string, err error) {
	log.Println("WARNING: getWifiInfoFromHost is a placeholder. Actual implementation needed.")
	return "MyHomeNetwork", "MyWiFiPassword", nil
}

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
		return fmt.Errorf("no ESP32-S3 CDC serial port found. Please ensure the device is connected and drivers are installed.")
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
		// 处理Shell命令请求
		// Payload应该是一个需要在本地执行的命令字符串
		command, ok := resp.Payload.(string)
		if !ok {
			log.Printf("Error: shellCommand payload is not a string: %v", resp.Payload)
			return
		}
		fmt.Printf("[NOOX Shell] Executing: %s\n", command)
		executeLocalShellCommand(command)
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
	case "wifiConnectStatus":
		// Payload is the WiFi connection status message
		wifiStatusMsg, ok := resp.Payload.(string)
		if !ok {
			log.Printf("Error: wifiConnectStatus payload is not a string: %v", resp.Payload)
			return
		}
		fmt.Printf("[NOOX Device] WiFi Connect Status (RequestId: %s): %s - %s\n", resp.RequestId, resp.Status, wifiStatusMsg)
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
	for scanner.Scan() {
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

// 在本地执行Shell命令
// 根据操作系统类型选择合适的Shell，执行命令并收集输出结果
func executeLocalShellCommand(command string) {
	var cmd *exec.Cmd

	// 根据操作系统选择合适的Shell
	if os.Getenv("OS") == "Windows_NT" {
		// Windows系统使用cmd.exe
		cmd = exec.Command("cmd", "/C", command)
	} else {
		// Unix类系统使用bash
		cmd = exec.Command("bash", "-c", command)
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

	shellPayload := ShellOutputPayload{
		Command:  command,
		Stdout:   stdout.String(),
		Stderr:   stderr.String(),
		Status:   status,
		ExitCode: exitCode,
	}

	hostMsg := HostMessage{
		RequestId: generateUUID(),
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
