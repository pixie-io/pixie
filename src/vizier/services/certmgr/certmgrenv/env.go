package certmgrenv

import "px.dev/pixie/src/shared/services/env"

// CertMgrEnv is the interface for the certmgr service environment.
type CertMgrEnv interface {
	env.Env
}

// Impl is an implementation of the CertMgrEnv interface
type Impl struct {
	*env.BaseEnv
}

// New creates a new certmgr env.
func New(audience string) *Impl {
	return &Impl{env.New(audience)}
}
