package controllers

import (
	"errors"
	"net/http"

	"github.com/gorilla/sessions"
	"pixielabs.ai/pixielabs/services/common"
)

// GetDefaultSession loads the default session from the request.
func GetDefaultSession(r *http.Request) (*sessions.Session, error) {
	env := r.Context().Value(common.EnvKey)
	gatewayEnv, ok := env.(*GatewayEnv)
	if !ok {
		return nil, errors.New("internal environment error")
	}

	store := gatewayEnv.CookieStore
	session, err := store.Get(r, "default-session")
	if err != nil {
		return nil, errors.New("error fetching session info")
	}
	return session, nil
}
