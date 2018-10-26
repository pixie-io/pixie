package controllers

import (
	"errors"

	"github.com/gorilla/sessions"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/services/common"
)

func init() {
	pflag.String("session_key", "", "Cookie session key")
}

// GatewayEnv store the contextual environment used for gateway requests.
type GatewayEnv struct {
	*common.Env
	CookieStore *sessions.CookieStore
}

// NewGatewayEnv creates a new gateway environment.
func NewGatewayEnv() (*GatewayEnv, error) {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		return nil, errors.New("session_key is required for cookie store")
	}
	sessionStore := sessions.NewCookieStore([]byte(viper.GetString("session_key")))
	return &GatewayEnv{
		Env:         common.NewEnv(),
		CookieStore: sessionStore,
	}, nil
}
