package pxanalytics

import (
	"runtime"
	"sync"

	"github.com/gofrs/uuid"
	"gopkg.in/segmentio/analytics-go.v3"
	version "pixielabs.ai/pixielabs/src/shared/version/go"
)

const (
	// Analytics Key for Segment. Note: these are basically public keys since they are shipped with the CLI.
	devAnalyticsKey = "DlP5FOZCLaPOukQN2FpkO0jdfxQrX13r"
	// Prod key is for prod/staging binaries. We can't really identify the difference.
	prodAnalyticsKey = "ehDrHWhR396KwcAQz0syA8YjwhwLXD1v"
)

var (
	client analytics.Client
	once   sync.Once
)

func isDevVersion() bool {
	return version.GetVersion().RevisionStatus() != "Distribution"
}

func getSegmentKey() string {
	if isDevVersion() {
		return devAnalyticsKey
	}
	return prodAnalyticsKey
}

// Client returns the default analytics client.
func Client() analytics.Client {
	once.Do(func() {
		client, _ = analytics.NewWithConfig(getSegmentKey(), analytics.Config{
			Endpoint: "https://segment.withpixie.ai",
			DefaultContext: &analytics.Context{
				App: analytics.AppInfo{
					Name:    "PX CLI",
					Version: version.GetVersion().ToString(),
					Build:   version.GetVersion().RevisionStatus(),
				},
				OS: analytics.OSInfo{
					Name: runtime.GOOS,
				},
				Extra: map[string]interface{}{
					"sessionID": uuid.Must(uuid.NewV4()).String(),
				},
			},
		})
	})
	return client
}
