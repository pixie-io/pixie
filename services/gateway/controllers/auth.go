package controllers

import (
	"encoding/json"
	"net/http"

	"pixielabs.ai/pixielabs/services/common/env"
	"pixielabs.ai/pixielabs/services/common/handler"
	"pixielabs.ai/pixielabs/services/gateway/gwenv"
)

// AuthLoginHandler make requests to the auth service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLoginHandler(env env.Env, w http.ResponseWriter, r *http.Request) error {
	gwEnv, ok := env.(gwenv.GatewayEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	session, err := GetDefaultSession(gwEnv, r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	// Bail early if the session is valid.
	if session.Values["token"] != nil {
		w.WriteHeader(http.StatusOK)
		return nil
	}
	// Extract params.
	var params struct {
		AccessToken string
		State       string
	}
	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}
	// TODO(zasgar): Make GRPC request to auth service.
	token := "ThisWillBeTheTokenFromRPCRequest"

	// Set session cookie.
	session.Values["token"] = token
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Save(r, w)
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLogoutHandler deletes existing sessions.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLogoutHandler(env env.Env, w http.ResponseWriter, r *http.Request) error {
	gwEnv, ok := env.(gwenv.GatewayEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	session, err := GetDefaultSession(gwEnv, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}
	// Delete the cookie.
	session.Values["token"] = ""
	session.Options.MaxAge = -1
	session.Save(r, w)
	w.WriteHeader(http.StatusOK)
	return nil
}
