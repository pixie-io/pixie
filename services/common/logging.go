package common

import (
	"os"

	log "github.com/sirupsen/logrus"
)

// SetupServiceLogging sets up a consistent logging environment for all services.
func SetupServiceLogging() {
	// Setup logging.
	log.SetOutput(os.Stdout)
	log.SetLevel(log.InfoLevel)
}
