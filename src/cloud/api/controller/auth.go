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
	"pixielabs.ai/pixielabs/src/cloud/api/idprovider"
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

// AuthOAuthLoginHandler handles logins for OSS oauth support.
func AuthOAuthLoginHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}

	oa := apiEnv.IdentityProviderClient()
	session, err := apiEnv.CookieStore().Get(r, idprovider.IDProviderSessionKey)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}
	return oa.HandleLogin(session, w, r)
}

// AuthSignupHandler make requests to the authpb service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthSignupHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return &handler.StatusError{http.StatusInternalServerError, errors.New("failed to get environment")}
	}

	// GetDefaultSession, will always return a valid session, even if it is empty.
	// We don't check the err here because even if the preexisting
	// session cookie is expired or couldn't be decoded, we will overwrite it below anyway.
	session, _ := GetDefaultSession(apiEnv, r)
	// This should never be nil, but we check to be sure.
	if session == nil {
		return &handler.StatusError{http.StatusInternalServerError, errors.New("failed to get session cookie")}
	}

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{http.StatusInternalServerError, err}
	}

	rpcReq := &authpb.SignupRequest{
		AccessToken: params.AccessToken,
	}

	resp, err := env.(apienv.APIEnv).AuthClient().Signup(ctxWithCreds, rpcReq)
	if err != nil {
		log.WithError(err).Errorf("RPC request to authpb service failed")
		s, ok := status.FromError(err)
		if ok {
			if s.Code() == codes.Unauthenticated {
				return handler.NewStatusError(http.StatusUnauthorized, s.Message())
			}
		}

		return services.HTTPStatusFromError(err, "Failed to signup")
	}

	userIDStr := pbutils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
	orgIDStr := pbutils.UUIDFromProtoOrNil(resp.OrgID).String()

	// Get orgName for analytics events.
	pc := env.(apienv.APIEnv).ProfileClient()
	orgResp, err := pc.GetOrg(ctxWithCreds, resp.OrgID)
	if err != nil {
		return services.HTTPStatusFromError(err, "Failed to get org")
	}

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  events.UserSignedUp,
		Properties: analytics.NewProperties().
			Set("site_name", orgResp.OrgName),
	})

	if resp.OrgCreated {
		events.Client().Enqueue(&analytics.Group{
			UserId:  userIDStr,
			GroupId: orgIDStr,
			Traits: map[string]interface{}{
				"kind":        "organization",
				"name":        orgResp.OrgName,
				"domain_name": orgResp.DomainName,
			},
		})

		events.Client().Enqueue(&analytics.Track{
			UserId: pbutils.UUIDFromProtoOrNil(resp.UserID).String(),
			Event:  events.OrgCreated,
			Properties: analytics.NewProperties().
				Set("org_id", orgIDStr),
		})
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w)

	err = sendSignupUserInfo(w, resp.UserInfo, resp.Token, resp.ExpiresAt, resp.OrgCreated)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLoginHandler make requests to the authpb service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 idtoken), state.
func AuthLoginHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return &handler.StatusError{http.StatusInternalServerError, errors.New("failed to get environment")}
	}

	// GetDefaultSession, will always return a valid session, even if it is empty.
	// We don't check the err here because even if the preexisting
	// session cookie is expired or couldn't be decoded, we will overwrite it below anyway.
	session, _ := GetDefaultSession(apiEnv, r)
	// This should never be nil, but we check to be sure.
	if session == nil {
		return &handler.StatusError{http.StatusInternalServerError, errors.New("failed to get session cookie")}
	}

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string
		State       string
		OrgName     string
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
	// This should check the err returned by GetDefaultSession.
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
		CreateUserIfNotExists: true,
		OrgName:               params.OrgName,
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
	})

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w)

	err = sendUserInfo(w, resp.UserInfo, resp.OrgInfo, resp.Token, resp.ExpiresAt, resp.UserCreated)
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
}

func sendUserInfo(w http.ResponseWriter, userInfo *authpb.AuthenticatedUserInfo, orgInfo *authpb.LoginReply_OrgInfo, token string, expiresAt int64, userCreated bool) error {
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
		OrgInfo     struct {
			OrgID   string `json:"orgID"`
			OrgName string `json:"orgName"`
		} `json:"orgInfo"`
	}

	data.Token = token
	data.ExpiresAt = expiresAt
	data.UserInfo.UserID = pbutils.UUIDFromProtoOrNil(userInfo.UserID).String()
	data.UserInfo.Email = userInfo.Email
	data.UserInfo.FirstName = userInfo.FirstName
	data.UserInfo.LastName = userInfo.LastName
	data.UserCreated = userCreated
	data.OrgInfo.OrgID = orgInfo.OrgID
	data.OrgInfo.OrgName = orgInfo.OrgName

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return json.NewEncoder(w).Encode(&data)
}

func sendSignupUserInfo(w http.ResponseWriter, userInfo *authpb.AuthenticatedUserInfo, token string, expiresAt int64, orgCreated bool) error {
	var data struct {
		Token     string `json:"token"`
		ExpiresAt int64  `json:"expiresAt"`
		UserInfo  struct {
			UserID    string `json:"userID"`
			FirstName string `json:"firstName"`
			LastName  string `json:"lastName"`
			Email     string `json:"email"`
		} `json:"userInfo"`
		OrgCreated bool `json:"orgCreated"`
	}

	data.Token = token
	data.ExpiresAt = expiresAt
	data.UserInfo.UserID = pbutils.UUIDFromProtoOrNil(userInfo.UserID).String()
	data.UserInfo.Email = userInfo.Email
	data.UserInfo.FirstName = userInfo.FirstName
	data.UserInfo.LastName = userInfo.LastName
	data.OrgCreated = orgCreated

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return json.NewEncoder(w).Encode(&data)
}
