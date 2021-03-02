package services

import (
	"os"
	"runtime"
	"time"

	"github.com/getsentry/sentry-go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/shared/services/sentryhook"
	version "pixielabs.ai/pixielabs/src/shared/version/go"
)

func init() {
	pflag.String("sentry_dsn", "", "The sentry DSN. Empty disables sentry")
}

// InitDefaultSentry initialize sentry with default options. The options are set based on values
// from viper.
func InitDefaultSentry(id string) func() {
	dsn := viper.GetString("sentry_dsn")
	podName := viper.GetString("pod_name")
	executable, _ := os.Executable()

	err := sentry.Init(sentry.ClientOptions{
		Dsn:              dsn,
		AttachStacktrace: true,
		Release:          version.GetVersion().ToString(),
		Environment:      runtime.GOOS,
		MaxBreadcrumbs:   10,
	})
	if err != nil {
		log.WithError(err).Trace("Cannot initialize sentry")
	} else {
		tags := map[string]string{
			"version":    version.GetVersion().ToString(),
			"ID":         id,
			"executable": executable,
			"pod_name":   podName,
		}
		hook := sentryhook.New([]log.Level{
			log.ErrorLevel, log.PanicLevel, log.FatalLevel,
		}, sentryhook.WithTags(tags))
		log.AddHook(hook)
	}

	return func() {
		sentry.Flush(2 * time.Second)
	}
}
