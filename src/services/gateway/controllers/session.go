package controllers

import (
	"errors"
	"net/http"

	"github.com/gorilla/sessions"
	"pixielabs.ai/pixielabs/src/services/gateway/gwenv"
)

// GetDefaultSession loads the default session from the request.
func GetDefaultSession(env gwenv.GatewayEnv, r *http.Request) (*sessions.Session, error) {
	store := env.CookieStore()
	session, err := store.Get(r, "default-session")
	if err != nil {
		return nil, errors.New("error fetching session info")
	}
	return session, nil
}
