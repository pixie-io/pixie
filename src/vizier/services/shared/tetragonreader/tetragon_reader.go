package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"time"
)

const (
	inputLogPath  = "/var/run/cilium/tetragon/tetragon.log"
	outputPath    = "/tmp/tetragon.parsed.jsonl"
)

type ParsedEvent struct {
	Time         string `json:"time"`
	NodeName     string `json:"node_name"`
	FunctionName string `json:"function_name,omitempty"`
	PID          int    `json:"pid,omitempty"`
	Binary       string `json:"binary,omitempty"`
}

func main() {
	inFile, err := os.Open(inputLogPath)
	if err != nil {
		fmt.Printf("❌ Failed to open input log: %v\n", err)
		return
	}
	defer inFile.Close()

	outFile, err := os.Create(outputPath)
	if err != nil {
		fmt.Printf("❌ Failed to create output file: %v\n", err)
		return
	}
	defer outFile.Close()

	scanner := bufio.NewScanner(inFile)
	writer := bufio.NewWriter(outFile)

	for scanner.Scan() {
		line := scanner.Bytes()
		var raw map[string]any
		if err := json.Unmarshal(line, &raw); err != nil {
			continue // skip invalid JSON
		}

		// Try to extract minimal data
		timeStr, _ := raw["time"].(string)
		node, _ := raw["node_name"].(string)

		// Get nested process info if available
		var functionName string
		var pid int
		var binary string

		if kprobe, ok := raw["process_kprobe"].(map[string]any); ok {
			functionName, _ = kprobe["function_name"].(string)

			if proc, ok := kprobe["process"].(map[string]any); ok {
				binary, _ = proc["binary"].(string)
				if p, ok := proc["pid"].(float64); ok {
					pid = int(p)
				}
			}
		} else if exec, ok := raw["process_exec"].(map[string]any); ok {
			if proc, ok := exec["process"].(map[string]any); ok {
				binary, _ = proc["binary"].(string)
				if p, ok := proc["pid"].(float64); ok {
					pid = int(p)
				}
			}
		}

		event := ParsedEvent{
			Time:         timeStr,
			NodeName:     node,
			FunctionName: functionName,
			PID:          pid,
			Binary:       binary,
		}

		jsonLine, _ := json.Marshal(event)
		writer.Write(jsonLine)
		writer.WriteString("\n")
	}

	writer.Flush()
	fmt.Printf("✅ Parsed log written to %s at %s\n", outputPath, time.Now().Format(time.RFC3339))
}
