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
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	"pixielabs.ai/pixielabs/src/shared/services"
	commonenv "pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/events"
	"pixielabs.ai/pixielabs/src/shared/services/handler"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	pbutils "pixielabs.ai/pixielabs/src/utils"
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

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string
		State       string
		SiteName    string
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	// TODO(zasgar/michelle): See if we can enable this again. This block of code provides an
	// optimization to return the existing cookie if a valid session exists.
	// However, This breaks the API contract since
	// we don't ship back the user information, token, ID. Adding this information might make it
	// just as expensive as performing the Login, so this needs to be investigated.
	//	// Bail early if the session is valid.
	//	if len(session.Values) != 0 && session.Values["_at"] != nil {
	//		expiresAt, ok := session.Values["_expires_at"].(int64)
	//		if !ok {
	//			http.Error(w, "failed to get session expiration", http.StatusInternalServerError)
	//			return nil
	//		}
	//		// Check if token is still valid.
	//		if expiresAt > time.Now().Unix() && session.Values["_auth_site"] == params.SiteName {
	//			w.WriteHeader(http.StatusOK)
	//			return nil
	//		}
	//	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	rpcReq := &authpb.LoginRequest{
		AccessToken:           params.AccessToken,
		SiteName:              params.SiteName,
		CreateUserIfNotExists: true,
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

		return services.HTTPStatusFromError(err, "Failed to login")
	}

	userIDStr := pbutils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
	ev := events.UserLoggedIn
	if resp.UserCreated {
		ev = events.UserSignedUp
	}

	// TODO(zasgar/michelle): Move this to the above block ~ mid dec, since we just need to associate users once.
	// This is here for now to help associate old users.
	// User created successfully, send an analytics event to identify the user.
	events.Client().Enqueue(&analytics.Identify{
		UserId: userIDStr,
		Traits: analytics.NewTraits().
			SetFirstName(resp.UserInfo.FirstName).
			SetLastName(resp.UserInfo.LastName).
			SetEmail(resp.UserInfo.Email),
	})

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  ev,
		Properties: analytics.NewProperties().
			Set("site_name", params.SiteName),
	})

	setSessionCookie(session, resp.Token, resp.ExpiresAt, params.SiteName, r, w)

	err = sendUserInfo(w, resp.UserInfo, resp.Token, resp.ExpiresAt, resp.UserCreated)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

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
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.SameSite = http.SameSiteStrictMode
	session.Options.Domain = viper.GetString("domain_name")

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

func setSessionCookie(session *sessions.Session, token string, expiresAt int64, siteName string, r *http.Request, w http.ResponseWriter) {
	// Set session cookie.
	session.Values["_at"] = token
	session.Values["_expires_at"] = expiresAt
	session.Values["_auth_site"] = siteName
	session.Options.MaxAge = int(time.Unix(expiresAt, 0).Sub(time.Now()).Seconds())
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.SameSite = http.SameSiteStrictMode
	session.Options.Domain = viper.GetString("domain_name")

	session.Save(r, w)
}

func sendUserInfo(w http.ResponseWriter, userInfo *authpb.UserInfo, token string, expiresAt int64, userCreated bool) error {
	var data struct {
		Token     string `json:"token"`
		ExpiresAt int64  `json:"expiresAt"`
		UserInfo  struct {
			UserID    string `json:"userID"`
			FirstName string `json:"firstName"`
			LastName  string `json:"lastName"`
			Email     string `json:"email"`
		} `json:"userInfo"`
		UserCreated bool `json:"userCreated"`
	}

	data.Token = token
	data.ExpiresAt = expiresAt
	data.UserInfo.UserID = pbutils.UUIDFromProtoOrNil(userInfo.UserID).String()
	data.UserInfo.Email = userInfo.Email
	data.UserInfo.FirstName = userInfo.FirstName
	data.UserInfo.LastName = userInfo.LastName
	data.UserCreated = userCreated

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return json.NewEncoder(w).Encode(&data)
}
