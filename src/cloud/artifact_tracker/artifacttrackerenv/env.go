package artifacttrackerenv

import (
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/shared/services/env"
)

// ArtifactTrackerEnv is the environment used for the artifacttracker service.
type ArtifactTrackerEnv interface {
	env.Env
}

// Impl is an implementation of the ArtifactTrackerEnv interface
type Impl struct {
	*env.BaseEnv
}

// New creates a new artifacttracker env.
func New() *Impl {
	return &Impl{env.New(viper.GetString("domain_name"))}
}
