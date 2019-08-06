package controller

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"github.com/dgrijalva/jwt-go"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/services/auth/proto"
	commonenv "pixielabs.ai/pixielabs/src/services/common/env"
	"pixielabs.ai/pixielabs/src/services/common/handler"
	pbjwt "pixielabs.ai/pixielabs/src/services/common/proto"
	"pixielabs.ai/pixielabs/src/services/common/utils"
)

// GetServiceCredentials returns JWT credentials for inter-service requests.
func GetServiceCredentials(signingKey string) (string, error) {
	pbClaims := pbjwt.JWTClaims{
		Subject:   "AuthService",
		Issuer:    "PL",
		ExpiresAt: time.Now().Add(time.Minute * 10).Unix(),
	}
	claims := utils.PBToMapClaims(&pbClaims)
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	return token.SignedString([]byte(signingKey))

}

// AuthLoginHandler make requests to the authpb service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLoginHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	session, err := GetDefaultSession(apiEnv, r)
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	// Bail early if the session is valid.
	if len(session.Values) != 0 && session.Values["_at"] != nil {
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

	serviceAuthToken, err := GetServiceCredentials(env.JWTSigningKey())
	if err != nil {
		log.WithError(err).Error("Service authpb failure")
		return handler.NewStatusError(http.StatusInternalServerError,
			"failed to get service authpb")
	}

	rpcReq := &authpb.LoginRequest{
		AccessToken: params.AccessToken,
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))
	resp, err := apiEnv.AuthClient().Login(ctxWithCreds, rpcReq)
	if err != nil {
		log.WithError(err).Errorf("RPC request to authpb service failed")
		s, ok := status.FromError(err)
		if ok {
			if s.Code() == codes.Unauthenticated {
				return handler.NewStatusError(http.StatusUnauthorized, s.Message())
			}
		}
		return handler.NewStatusError(http.StatusInternalServerError, "failed to login")
	}

	// Set session cookie.
	session.Values["_at"] = resp.Token
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Save(r, w)

	var payload struct {
		Token     string `json:"token"`
		ExpiresAt int64  `json:"expiresAt"`
	}
	payload.Token = resp.Token
	payload.ExpiresAt = resp.ExpiresAt

	json.NewEncoder(w).Encode(payload)
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLogoutHandler deletes existing sessions.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLogoutHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	session, err := GetDefaultSession(apiEnv, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}
	// Delete the cookie.
	session.Values["_at"] = ""
	session.Options.MaxAge = -1
	session.Save(r, w)
	w.WriteHeader(http.StatusOK)
	return nil
}
