package sitemanagerenv

import (
	"pixielabs.ai/pixielabs/src/shared/services/env"
)

// SiteManagerEnv is the authenv use for the Authentication service.
type SiteManagerEnv interface {
	env.Env
}

// Impl is an implementation of the AuthEnv interface
type Impl struct {
	*env.BaseEnv
}

// New creates a new auth authenv.
func New() (*Impl, error) {
	return &Impl{env.New()}, nil
}
