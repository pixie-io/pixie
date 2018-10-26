package controllers

import (
	"pixielabs.ai/pixielabs/services/common"
)

// AuthEnv is the environment use for the Authentication service.
type AuthEnv struct {
	*common.Env
}

// NewAuthEnv creates a new auth environment.
func NewAuthEnv() (*AuthEnv, error) {
	return &AuthEnv{
		Env: common.NewEnv(),
	}, nil
}

// GetBaseEnv returns a pointer to the common base environment.
func (a *AuthEnv) GetBaseEnv() *common.Env {
	return a.Env
}
