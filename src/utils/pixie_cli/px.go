package main

// This is the Pixie Admin CLI.
// It will be responsible for managing and deploy Pixie on a cluster.

import (
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/getsentry/sentry-go"
	log "github.com/sirupsen/logrus"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/cmd"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/sentryhook"
	cliLog "pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
)

const sentryDSN = "https://ef3a781b5e7b42e282706fc541077f3a@sentry.io/4090453"

func main() {
	// Disable Sentry in dev mode.
	selectedDSN := sentryDSN
	if version.GetVersion().IsDev() {
		selectedDSN = ""
	}

	err := sentry.Init(sentry.ClientOptions{
		Dsn:              selectedDSN,
		AttachStacktrace: true,
		Release:          version.GetVersion().ToString(),
		Environment:      runtime.GOOS,
		MaxBreadcrumbs:   10,
	})
	if err != nil {
		log.WithError(err).Trace("Cannot initialize sentry")
	} else {
		tags := map[string]string{
			"version":  version.GetVersion().ToString(),
			"clientID": pxconfig.Cfg().UniqueClientID,
		}
		hook := sentryhook.New([]log.Level{
			log.ErrorLevel, log.PanicLevel, log.FatalLevel,
		}, sentryhook.WithTags(tags))
		log.AddHook(hook)
	}
	defer sentry.Flush(2 * time.Second)
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
	cliLog.Info("Pixie CLI")
	cmd.Execute()
}
