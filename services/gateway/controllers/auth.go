package controllers

import (
	"encoding/json"
	"net/http"
)

// AuthLoginHandler make requests to the auth service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLoginHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "not a post request", http.StatusMethodNotAllowed)
		return
	}

	session, err := GetDefaultSession(r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}

	// Bail early if the session is valid.
	if session.Values["token"] != nil {
		w.WriteHeader(http.StatusOK)
		return
	}
	// Extract params.
	var params struct {
		AccessToken string
		State       string
	}
	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		http.Error(w, "failed to decode json request", http.StatusBadRequest)
		return
	}
	// TODO(zasgar): Make GRPC request to auth service.
	token := "ThisWillBeTheTokenFromRPCRequest"

	// Set session cookie.
	session.Values["token"] = token
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Save(r, w)
	w.WriteHeader(http.StatusOK)
}

// AuthLogoutHandler deletes existing sessions.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLogoutHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "not a post request", http.StatusMethodNotAllowed)
		return
	}

	session, err := GetDefaultSession(r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
	}
	// Delete the cookie.
	session.Values["token"] = ""
	session.Options.MaxAge = -1
	session.Save(r, w)
	w.WriteHeader(http.StatusOK)
}
