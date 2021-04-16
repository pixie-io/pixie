package controller

import (
	"fmt"
	"net/http"

	"github.com/gorilla/sessions"

	"px.dev/pixie/src/cloud/api/apienv"
)

// GetDefaultSession loads the default session from the request. It will always return a valid session.
// If it returns an error, there was a problem decoding the previous session, or the previous
// session has expired.
func GetDefaultSession(env apienv.APIEnv, r *http.Request) (*sessions.Session, error) {
	store := env.CookieStore()
	session, err := store.Get(r, "default-session4")
	if err != nil {
		// CookieStore().Get(...) will always return a valid session, even if it has
		// errored trying to decode the previous existing session.
		return session, fmt.Errorf("error fetching session info: %v", err)
	}
	return session, nil
}
