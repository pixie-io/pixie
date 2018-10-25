package controllers

import (
	"github.com/gorilla/sessions"
	"pixielabs.ai/pixielabs/services/common"
)

// GatewayEnv store the contextual environment used for gateway requests.
type GatewayEnv struct {
	common.Env
	CookieStore *sessions.CookieStore
}
