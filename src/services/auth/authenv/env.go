package authenv

import (
	"pixielabs.ai/pixielabs/src/services/common/env"
)

// AuthEnv is the authenv use for the Authentication service.
type AuthEnv interface {
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
