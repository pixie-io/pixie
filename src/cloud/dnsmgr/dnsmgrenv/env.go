package dnsmgrenv

import "pixielabs.ai/pixielabs/src/shared/services/env"

// DNSMgrEnv is the environment used for the dnsmgr service.
type DNSMgrEnv interface {
	env.Env
}

// Impl is an implementation of the DNSMgrEnv interface
type Impl struct {
	*env.BaseEnv
}

// New creates a new dnsmgr env.
func New() *Impl {
	return &Impl{env.New()}
}
