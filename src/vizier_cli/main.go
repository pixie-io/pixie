package main

import (
	"fmt"

	"github.com/fatih/color"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/vizier_cli/cli"
	"pixielabs.ai/pixielabs/src/vizier_cli/controller"
)

func main() {
	pixie := `
  ___  _       _         ___  _     ___
 | _ \(_)__ __(_) ___   / __|| |   |_ _|
 |  _/| |\ \ /| |/ -_) | (__ | |__  | |
 |_|  |_|/_\_\|_|\___|  \___||____||___|
`
	color.Set(color.FgYellow)
	fmt.Println(pixie)
	color.Unset()

	log.Info("Pixie interactive CLI.")
	c := controller.New()
	defer c.Shutdown()

	cli := cli.New(c)
	cli.Run()
}
