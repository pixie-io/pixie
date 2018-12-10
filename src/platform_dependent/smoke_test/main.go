package main

import (
	"log"
	"pixielabs.ai/pixielabs/src/platform_dependent/smoke_test/commands"
)

func main() {
	if err := commands.PixieCmd.Execute(); err != nil {
		log.Fatalf("Error executing Pixie CLI command, %v", err)
	}
}
