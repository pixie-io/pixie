package querybrokerenv

import "pixielabs.ai/pixielabs/src/services/common/env"

// QueryBrokerEnv is the interface for the Query Broker service environment.
type QueryBrokerEnv interface {
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
