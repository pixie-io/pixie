package controllers

import (
	"pixielabs.ai/pixielabs/services/common"
)

// AuthEnv is the environment use for the Authentication service.
type AuthEnv struct {
	*common.Env
}

// GetBaseEnv returns a pointer to the common base environment.
func (a *AuthEnv) GetBaseEnv() *common.Env {
	return a.Env
}
