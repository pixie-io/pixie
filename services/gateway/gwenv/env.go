package gwenv

import (
	"errors"

	"github.com/gorilla/sessions"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	authpb "pixielabs.ai/pixielabs/services/auth/proto"
	"pixielabs.ai/pixielabs/services/common/env"
)

func init() {
	pflag.String("session_key", "", "Cookie session key")
}

// GatewayEnv store the contextual authenv used for gateway requests.
type GatewayEnv interface {
	env.Env
	CookieStore() *sessions.CookieStore
	AuthClient() authpb.AuthServiceClient
}

// Impl is an implementation of the GatewayEnv interface.
type Impl struct {
	*env.BaseEnv
	cookieStore *sessions.CookieStore
	authClient  authpb.AuthServiceClient
}

// New creates a new gateway env.
func New(ac authpb.AuthServiceClient) (GatewayEnv, error) {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		return nil, errors.New("session_key is required for cookie store")
	}
	sessionStore := sessions.NewCookieStore([]byte(sessionKey))
	return &Impl{env.New(), sessionStore, ac}, nil
}

// CookieStore returns the CookieStore from the environment.
func (e *Impl) CookieStore() *sessions.CookieStore {
	return e.cookieStore

}

// AuthClient returns an auth service client.
func (e *Impl) AuthClient() authpb.AuthServiceClient {
	return e.authClient
}
