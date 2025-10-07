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

// JSON message structures for communication with ESP32
// New struct for shell output payload
type ShellOutputPayload struct {
	Command  string `json:"command,omitempty"`
	Stdout   string `json:"stdout,omitempty"`
	Stderr   string `json:"stderr,omitempty"`
	Status   string `json:"status,omitempty"`
	ExitCode int    `json:"exitCode,omitempty"`
}

// Modified HostMessage struct for flexible payloads
type HostMessage struct {
	RequestId string      `json:"requestId"`
	Type      string      `json:"type"`
	Payload   interface{} `json:"payload,omitempty"`
}

// New struct for connectToWifi payload
type ConnectToWifiPayload struct {
	SSID     string `json:"ssid"`
	Password string `json:"password"`
}

type ESP32Response struct {
	RequestId string      `json:"requestId"`
	Type      string      `json:"type"`
	Payload   interface{} `json:"payload,omitempty"`
	Status    string      `json:"status,omitempty"`
	Content   string      `json:"content,omitempty"`
}

var (
	serialPort serial.Port
	portName   string
	mu         sync.Mutex
	wifiStatus string
)

func init() {
	flag.StringVar(&wifiStatus, "wifi-status", "unknown", "Initial WiFi status from ESP32 (connected/disconnected/unknown)")
}

func main() {
	flag.Parse()
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

func performInitialDeviceSetup() {
	log.Println("Performing initial device setup...")

	linkTestReq := HostMessage{
		RequestId: generateUUID(),
		Type:      "linkTest",
		Payload:   "ping",
	}
	sendToESP32(linkTestReq)

	time.Sleep(1 * time.Second)

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

func connectToESP32() error {
	ports, err := serial_enumerator.GetDetailedPortsList()
	if err != nil {
		return fmt.Errorf("failed to enumerate serial ports: %w", err)
	}

	for _, p := range ports {
		if strings.Contains(p.Product, "ESP32-S3") || strings.Contains(p.Product, "USB Serial Device") || strings.Contains(p.VID, "303A") {
			log.Printf("Found potential ESP32-S3: %s (%s)", p.Name, p.Product)
			portName = p.Name
			break
		}
	}

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

func readFromESP32() {
	reader := bufio.NewReader(serialPort)
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			if err != io.EOF {
				log.Printf("Error reading from serial port: %v", err)
			}
			return
		}
		line = strings.TrimSpace(line)
		if line == "" {
			continue
		}

		var espResponse ESP32Response
		err = json.Unmarshal([]byte(line), &espResponse)
		if err != nil {
			log.Printf("Error unmarshalling ESP32 response: %v, Raw: %s", err, line)
			fmt.Printf("[NOOX Device] Raw: %s\n", line)
			continue
		}

		handleESP32Response(espResponse)
	}
}

func handleESP32Response(resp ESP32Response) {
	switch resp.Type {
	case "shellCommand":
		// Payload is the command string
		command, ok := resp.Payload.(string)
		if !ok {
			log.Printf("Error: shellCommand payload is not a string: %v", resp.Payload)
			return
		}
		fmt.Printf("[NOOX Shell] Executing: %s\n", command)
		executeLocalShellCommand(command)
	case "aiResponse":
		// Payload is the AI response string
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

func readFromStdin() {
	scanner := bufio.NewScanner(os.Stdin)
	for scanner.Scan() {
		input := scanner.Text()
		if input == "" {
			continue
		}

		msg := HostMessage{
			RequestId: generateUUID(),
			Type:      "userInput",
			Payload:   input,
		}
		sendToESP32(msg)
	}
	if err := scanner.Err(); err != nil {
		log.Printf("Error reading from stdin: %v", err)
	}
}

func executeLocalShellCommand(command string) {
	var cmd *exec.Cmd

	if os.Getenv("OS") == "Windows_NT" {
		cmd = exec.Command("cmd", "/C", command)
	} else {
		cmd = exec.Command("bash", "-c", command)
	}

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

func sendToESP32(msg HostMessage) {
	jsonData, err := json.Marshal(msg)
	if err != nil {
		log.Printf("Error marshalling host message: %v", err)
		return
	}

	mu.Lock()
	defer mu.Unlock()

	_, err = serialPort.Write(append(jsonData, '\n'))
	if err != nil {
		log.Printf("Error writing to serial port: %v", err)
	}
}
