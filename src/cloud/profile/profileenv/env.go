package profileenv

import "pixielabs.ai/pixielabs/src/shared/services/env"

// ProfileEnv is the environment used for the profile service.
type ProfileEnv interface {
	env.Env
}

// Impl is an implementation of the AuthEnv interface
type Impl struct {
	*env.BaseEnv
}

// New creates a new profile env.
func New() *Impl {
	return &Impl{env.New()}
}
