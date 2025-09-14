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

	"go.bug.st/serial"
	serial_enumerator "go.bug.st/serial/enumerator" // Alias for enumerator
)

// JSON message structures for communication with ESP32
// New struct for shell output payload
type ShellOutputPayload struct {
	Command string `json:"command,omitempty"`
	Stdout  string `json:"stdout,omitempty"`
	Stderr  string `json:"stderr,omitempty"`
}

// Modified HostMessage struct for flexible payloads
type HostMessage struct {
	Type    string      `json:"type"`
	Payload interface{} `json:"payload,omitempty"` // Use interface{} for flexible payload
}

type ESP32Response struct {
	Type        string      `json:"type"`
	AIResponse  string      `json:"ai_response,omitempty"`
	ShellCommand string      `json:"shell_command,omitempty"`
	Content     string      `json:"content,omitempty"` // For error messages (e.g., "Invalid JSON")
	Payload     interface{} `json:"payload,omitempty"` // For generic error payloads (e.g., "Unknown message type")
}

var (
	serialPort serial.Port
	portName   string
	mu         sync.Mutex // Mutex for serial port access
)

func main() {
	log.Println("AIHi Host Agent starting...")

	// 1. Discover and connect to ESP32 serial port
	err := connectToESP32()
	if err != nil {
		log.Fatalf("Failed to connect to ESP32: %v", err)
	}
	defer serialPort.Close()
	log.Printf("Connected to ESP32 on %s", portName)

	// Goroutine to read from ESP32 serial port
	go readFromESP32()

	// Goroutine to read from stdin and send to ESP32
	go readFromStdin()

	// Keep main goroutine alive
	select {}
}

func connectToESP32() error {
	ports, err := serial_enumerator.GetDetailedPortsList() // 修正方法名
	if err != nil {
		return fmt.Errorf("failed to enumerate serial ports: %w", err)
	}

	for _, p := range ports {
		// Heuristic to identify ESP32-S3. This might need adjustment.
		// Look for common ESP32-S3 USB CDC descriptions or vendor/product IDs.
		// Example: "USB Serial Device", "ESP32-S3 CDC"
		if strings.Contains(p.Product, "ESP32-S3") || strings.Contains(p.Product, "USB Serial Device") || strings.Contains(p.VID, "303A") { // Common ESP32 VID
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
			fmt.Printf("[AIHi Device] Raw: %s\n", line) // Print raw if unmarshalling fails
			continue
		}

		handleESP32Response(espResponse)
	}
}

func handleESP32Response(resp ESP32Response) {
	switch resp.Type {
	case "combined_response":
		if resp.AIResponse != "" {
			fmt.Printf("[AIHi AI] %s\n", resp.AIResponse)
		}
		if resp.ShellCommand != "" {
			fmt.Printf("[AIHi Shell] Executing: %s\n", resp.ShellCommand)
			executeLocalShellCommand(resp.ShellCommand)
		}
	case "error":
		if resp.Content != "" {
			fmt.Printf("[AIHi Device Error] %s\n", resp.Content)
		} else if resp.Payload != nil {
			// Attempt to print the payload if Content is empty
			fmt.Printf("[AIHi Device Error] Payload: %v\n", resp.Payload)
		} else {
			fmt.Printf("[AIHi Device Error] Unknown error\n")
		}
	default:
		if resp.Content != "" {
			fmt.Printf("[AIHi Device] %s\n", resp.Content) // Generic content
		} else if resp.Payload != nil {
			fmt.Printf("[AIHi Device] Payload: %v\n", resp.Payload) // Generic payload
		} else {
			fmt.Printf("[AIHi Device] Unknown response\n")
		}
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
			Type:    "userInput", // Changed from "user_input" to match C++
			Payload: input,       // Content is now directly the payload
		}
		sendToESP32(msg)
	}
	if err := scanner.Err(); err != nil {
		log.Printf("Error reading from stdin: %v", err)
	}
}

func executeLocalShellCommand(command string) {
	var cmd *exec.Cmd

	// Determine OS and prepare command
	if os.Getenv("OS") == "Windows_NT" {
		cmd = exec.Command("cmd", "/C", command)
	} else {
		cmd = exec.Command("bash", "-c", command)
	}

	var stdout, stderr strings.Builder
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr

	err := cmd.Run() // Use Run() to wait for command to complete
	
	shellPayload := ShellOutputPayload{
		Command: command,
		Stdout:  stdout.String(),
		Stderr:  stderr.String(),
	}

	if err != nil {
		log.Printf("Error executing command '%s': %v", command, err)
		if shellPayload.Stderr == "" { // If stderr is empty, capture the Go error
			shellPayload.Stderr = err.Error()
		}
	}

	hostMsg := HostMessage{
		Type:    "shellResult", // Changed from "shell_output" to match C++
		Payload: shellPayload,  // Shell output is now the payload
	}

	if err != nil {
		log.Printf("Error executing command '%s': %v", command, err)
		if hostMsg.Stderr == "" { // If stderr is empty, capture the Go error
			hostMsg.Stderr = err.Error()
		}
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

	_, err = serialPort.Write(append(jsonData, '\n')) // Append newline for ESP32 ReadString
	if err != nil {
		log.Printf("Error writing to serial port: %v", err)
	}
}
