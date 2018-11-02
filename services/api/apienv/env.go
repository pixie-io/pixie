package apienv

import "pixielabs.ai/pixielabs/services/common/env"

// APIEnv is the interface for the API service environment.
type APIEnv interface {
	env.Env
}

// Impl is an implementation of the ApiEnv interface
type Impl struct {
	*env.BaseEnv
}

// New creates a new api env.
func New() (*Impl, error) {
	return &Impl{env.New()}, nil
}
