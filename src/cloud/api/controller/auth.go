package controller

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/gorilla/sessions"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/shared/services"
	commonenv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
)

// GetServiceCredentials returns JWT credentials for inter-service requests.
func GetServiceCredentials(signingKey string) (string, error) {
	claims := utils.GenerateJWTForService("AuthService")
	return utils.SignJWTClaims(claims, signingKey)
}

// AuthLoginHandler make requests to the authpb service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLoginHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	session, err := getSessionFromEnv(env, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	// Bail early if the session is valid.
	if len(session.Values) != 0 && session.Values["_at"] != nil {
		expiresAt, ok := session.Values["_expires_at"].(int64)
		if !ok {
			http.Error(w, "failed to get session expiration", http.StatusInternalServerError)
			return nil
		}
		// Check if token is still valid.
		if expiresAt > time.Now().Unix() {
			w.WriteHeader(http.StatusOK)
			return nil
		}
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string
		State       string
		DomainName  string
		UserEmail   string
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	domainName, err := GetDomainNameFromEmail(params.UserEmail)
	if err != nil {
		return services.HTTPStatusFromError(err, "failed to get domain from email")
	}

	rpcReq := &authpb.LoginRequest{
		AccessToken: params.AccessToken,
		SiteName:    params.DomainName,
		DomainName:  domainName,
	}

	resp, err := env.(apienv.APIEnv).AuthClient().Login(ctxWithCreds, rpcReq)
	if err != nil {
		log.WithError(err).Errorf("RPC request to authpb service failed")
		s, ok := status.FromError(err)
		if ok {
			if s.Code() == codes.Unauthenticated {
				return handler.NewStatusError(http.StatusUnauthorized, s.Message())
			}
		}

		return services.HTTPStatusFromError(err, "failed to login")
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w)

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
	session.Values["_expires_at"] = 0
	session.Options.MaxAge = -1
	session.Save(r, w)
	w.WriteHeader(http.StatusOK)
	return nil
}

func getSessionFromEnv(env commonenv.Env, r *http.Request) (*sessions.Session, error) {
	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return nil, errors.New("failed to get environment")
	}

	session, err := GetDefaultSession(apiEnv, r)
	if err != nil {
		return nil, err
	}

	return session, nil
}

func attachCredentialsToContext(env commonenv.Env, r *http.Request) (context.Context, error) {
	serviceAuthToken, err := GetServiceCredentials(env.JWTSigningKey())
	if err != nil {
		log.WithError(err).Error("Service authpb failure")
		return nil, errors.New("failed to get service authpb")
	}

	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", serviceAuthToken))

	return ctxWithCreds, nil
}

func setSessionCookie(session *sessions.Session, token string, expiresAt int64, r *http.Request, w http.ResponseWriter) {
	// Set session cookie.
	session.Values["_at"] = token
	session.Values["_expires_at"] = expiresAt
	session.Options.MaxAge = int(time.Unix(expiresAt, 0).Sub(time.Now()).Seconds())
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.SameSite = http.SameSiteStrictMode
	session.Options.Domain = viper.GetString("domain_name")

	session.Save(r, w)

	var payload struct {
		Token     string `json:"token"`
		ExpiresAt int64  `json:"expiresAt"`
	}
	payload.Token = token
	payload.ExpiresAt = expiresAt

	json.NewEncoder(w).Encode(payload)
}
