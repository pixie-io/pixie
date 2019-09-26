package main

// This is the Pixie Admin CLI.
// It will be responsible for managing and deploy Pixie on a cluster.

import (
	"fmt"

	"github.com/fatih/color"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/cmd"
)

func main() {
	pixie := `
  ___  _       _
 | _ \(_)__ __(_) ___
 |  _/| |\ \ /| |/ -_)
 |_|  |_|/_\_\|_|\___|
`
	color.Set(color.FgHiGreen)
	fmt.Println(pixie)
	color.Unset()

	log.Info("Pixie Admin CLI")
	cmd.Execute()
}
