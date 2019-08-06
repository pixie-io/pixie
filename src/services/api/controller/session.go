package controller

import (
	"errors"
	"net/http"

	"github.com/gorilla/sessions"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
)

// GetDefaultSession loads the default session from the request.
func GetDefaultSession(env apienv.APIEnv, r *http.Request) (*sessions.Session, error) {
	store := env.CookieStore()
	session, err := store.Get(r, "default-session")
	if err != nil {
		return nil, errors.New("error fetching session info")
	}
	return session, nil
}
