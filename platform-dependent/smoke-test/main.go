package main

import (
	"log"
	"pixielabs.ai/pixielabs/platform-dependent/smoke-test/commands"
)

func main() {
	if err := commands.PixieCmd.Execute(); err != nil {
		log.Fatalf("Error executing Pixie CLI command, %v", err)
	}
}
