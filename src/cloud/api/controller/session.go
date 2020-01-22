package controller

import (
	"fmt"
	"net/http"

	"github.com/gorilla/sessions"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
)

// GetDefaultSession loads the default session from the request.
func GetDefaultSession(env apienv.APIEnv, r *http.Request) (*sessions.Session, error) {
	store := env.CookieStore()
	// TODO(zasgar/michelle): Figure out why our sessions aren't getting cleared and remove this hack.
	session, err := store.Get(r, "default-session4")
	if err != nil {
		return nil, fmt.Errorf("error fetching session info: %v", err)
	}
	return session, nil
}
