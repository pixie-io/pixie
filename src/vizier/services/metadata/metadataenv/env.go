package metadataenv

import "pixielabs.ai/pixielabs/src/services/common/env"

// MetadataEnv is the interface for the Metadata service environment.
type MetadataEnv interface {
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
