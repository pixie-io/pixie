package main

// This is the Pixie Admin CLI.
// It will be responsible for managing and deploy Pixie on a cluster.

import (
	"os"
	"strings"

	log "github.com/sirupsen/logrus"
	"gopkg.in/segmentio/analytics-go.v3"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/cmd"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
)

func main() {
	defer pxanalytics.Client().Close()

	pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Exec Started",
		Properties: analytics.NewProperties().
			Set("cmd", strings.Join(os.Args, ",")),
	})

	defer pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Exec Complete",
	})

	log.SetOutput(os.Stderr)
	log.Info("Pixie CLI")
	cmd.Execute()
}
