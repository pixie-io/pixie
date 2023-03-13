/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"time"

	"github.com/gorilla/sessions"
	"github.com/lestrrat-go/jwx/jwt"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/api/apienv"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/authcontext"
	commonenv "px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/events"
	"px.dev/pixie/src/shared/services/handler"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

// GetServiceCredentials returns JWT credentials for inter-service requests.
func GetServiceCredentials(signingKey string) (string, error) {
	claims := srvutils.GenerateJWTForService("AuthService", viper.GetString("domain_name"))
	return srvutils.SignJWTClaims(claims, signingKey)
}

// AuthOAuthLoginHandler handles logins for OSS oauth support.
func AuthOAuthLoginHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}

	oa := apiEnv.IdentityProviderClient()
	session, err := apiEnv.CookieStore().Get(r, oa.SessionKey())
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
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}

	// GetDefaultSession, will always return a valid session, even if it is empty.
	// We don't check the err here because even if the preexisting
	// session cookie is expired or couldn't be decoded, we will overwrite it below anyway.
	session, _ := GetDefaultSession(apiEnv, r)
	// This should never be nil, but we check to be sure.
	if session == nil {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get session cookie")
	}

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string `json:"accessToken"`
		IDToken     string `json:"idToken"`
		InviteToken string `json:"inviteToken,omitempty"`
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	rpcReq := &authpb.SignupRequest{
		AccessToken: params.AccessToken,
		InviteToken: params.InviteToken,
	}

	resp, err := env.(apienv.APIEnv).AuthClient().Signup(ctxWithCreds, rpcReq)
	if err != nil {
		log.WithError(err).Errorf("RPC request to authpb service failed")
		s, ok := status.FromError(err)
		if ok {
			if s.Code() == codes.Unauthenticated {
				return handler.NewStatusError(http.StatusUnauthorized, s.Message())
			}

			if s.Code() == codes.NotFound {
				return handler.NewStatusError(http.StatusNotFound, s.Message())
			}
		}

		return services.HTTPStatusFromError(err, "Failed to signup")
	}

	userIDStr := utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
	orgIDStr := utils.UUIDFromProtoOrNil(resp.OrgID).String()

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  events.UserSignedUp,
		Properties: analytics.NewProperties().
			Set("org_id", orgIDStr).
			Set("identity_provider", resp.IdentityProvider).
			Set("had_invite", params.InviteToken != ""),
	})

	if resp.OrgCreated {
		events.Client().Enqueue(&analytics.Track{
			UserId: userIDStr,
			Event:  events.OrgCreated,
			Properties: analytics.NewProperties().
				Set("auto_created", true).
				Set("org_name", resp.OrgName).
				Set("org_id", orgIDStr),
		})
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w, http.SameSiteStrictMode)

	err = sendSignupUserInfo(w, resp.UserInfo, resp.Token, resp.ExpiresAt, resp.OrgCreated)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

	log.WithField("host", r.Host).
		WithField("timestamp", time.Now()).
		WithField("address", r.URL).
		WithField("browser", r.Header.Get("User-Agent")).
		WithField("user", userIDStr).
		WithField("referer", r.Header.Get("Referer")).
		Info("User signed up")

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLoginHandler make requests to the authpb service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 accessToken), state.
func AuthLoginHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}

	// GetDefaultSession, will always return a valid session, even if it is empty.
	// We don't check the err here because even if the preexisting
	// session cookie is expired or couldn't be decoded, we will overwrite it below anyway.
	session, _ := GetDefaultSession(apiEnv, r)
	// This should never be nil, but we check to be sure.
	if session == nil {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get session cookie")
	}

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string `json:"accessToken"`
		IDToken     string `json:"idToken"`
		State       string `json:"state"`
		InviteToken string `json:"inviteToken,omitempty"`
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	var userInfo *authpb.AuthenticatedUserInfo
	var orgInfo *authpb.LoginReply_OrgInfo
	var token string
	var expiresAt int64
	userCreated := false
	userID := ""
	var orgID string
	var identityProvider string

	// If logging in using an API key, just get the augmented token.
	token, expiresAt, err = loginWithAPIKey(ctxWithCreds, env, r, w)

	if err != nil {
		rpcReq := &authpb.LoginRequest{
			AccessToken:           params.AccessToken,
			CreateUserIfNotExists: true,
			InviteToken:           params.InviteToken,
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
		token = resp.Token
		expiresAt = resp.ExpiresAt
		userCreated = resp.UserCreated
		userInfo = resp.UserInfo
		orgInfo = resp.OrgInfo
		orgID = orgInfo.OrgID
		userID = utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
		identityProvider = resp.IdentityProvider
	} else {
		orgID, _ = parseOrgIDFromAPIKey(token, env.JWTSigningKey(), viper.GetString("domain_name"))
	}

	if userCreated {
		events.Client().Enqueue(&analytics.Track{
			UserId: userID,
			Event:  events.UserSignedUp,
			Properties: analytics.NewProperties().
				Set("org_id", orgID).
				Set("identity_provider", identityProvider).
				Set("had_invite", params.InviteToken != ""),
		})
	} else {
		events.Client().Enqueue(&analytics.Track{
			UserId: userID,
			Event:  events.UserLoggedIn,
			Properties: analytics.NewProperties().
				Set("org_id", orgID).
				Set("embedded", false).
				Set("identity_provider", identityProvider).
				Set("had_invite", params.InviteToken != ""),
		})
	}

	setSessionCookie(session, token, expiresAt, r, w, http.SameSiteNoneMode)

	err = sendUserInfo(w, userInfo, orgInfo, token, expiresAt, userCreated)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

	log.WithField("host", r.Host).
		WithField("timestamp", time.Now()).
		WithField("address", r.URL).
		WithField("browser", r.Header.Get("User-Agent")).
		WithField("user", userID).
		WithField("referer", r.Header.Get("Referer")).
		Info("User logged in")

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLoginHandlerEmbed is the replacement embed login handler.
// Request-type: application/json.
// Params: accessToken (auth0 accessToken), state.
func AuthLoginHandlerEmbed(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	// Extract params from the body which consists of the Auth0 ID token.
	var params struct {
		AccessToken string `json:"accessToken"`
		IDToken     string `json:"idToken"`
		State       string `json:"state"`
	}

	defer r.Body.Close()
	if err := json.NewDecoder(r.Body).Decode(&params); err != nil {
		return handler.NewStatusError(http.StatusBadRequest,
			"failed to decode json request")
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	var userInfo *authpb.AuthenticatedUserInfo
	var orgInfo *authpb.LoginReply_OrgInfo
	var token string
	var expiresAt int64
	userCreated := false
	userID := ""
	var orgID string
	var identityProvider string

	// If logging in using an API key, just get the augmented token.
	token, expiresAt, err = loginWithAPIKey(ctxWithCreds, env, r, w)

	if err != nil {
		rpcReq := &authpb.LoginRequest{
			AccessToken:           params.AccessToken,
			CreateUserIfNotExists: false,
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

		token = resp.Token
		expiresAt = resp.ExpiresAt
		userCreated = resp.UserCreated
		userInfo = resp.UserInfo
		orgInfo = resp.OrgInfo
		orgID = orgInfo.OrgID
		userID = utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
		identityProvider = resp.IdentityProvider
	} else {
		orgID, _ = parseOrgIDFromAPIKey(token, env.JWTSigningKey(), viper.GetString("domain_name"))
	}

	events.Client().Enqueue(&analytics.Track{
		UserId: userID,
		Event:  events.UserLoggedIn,
		Properties: analytics.NewProperties().
			Set("user_id", userID).
			Set("org_id", orgID).
			Set("identity_provider", identityProvider).
			Set("had_invite", false).
			Set("embedded", true),
	})

	err = sendUserInfo(w, userInfo, orgInfo, token, expiresAt, userCreated)
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
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	aCtx := authcontext.New()
	if _, ok := session.Values["_at"]; !ok {
		return handler.NewStatusError(http.StatusBadRequest, "missing auth token")
	}
	err = aCtx.UseJWTAuth(env.JWTSigningKey(), session.Values["_at"].(string), viper.GetString("domain_name"))
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}
	userID := aCtx.Claims.GetUserClaims().UserID

	// Delete the cookie.
	session.Values["_at"] = ""
	session.Values["_expires_at"] = 0
	session.Options.MaxAge = -1
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.Domain = viper.GetString("domain_name")

	log.WithField("host", r.Host).
		WithField("timestamp", time.Now()).
		WithField("address", r.URL).
		WithField("browser", r.Header.Get("User-Agent")).
		WithField("user", userID).
		WithField("referer", r.Header.Get("Referer")).
		Info("User logged out")

	err = session.Save(r, w)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthRefetchHandler return a new user token with updated claims.
// Request-type: application/json.
func AuthRefetchHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	if r.Method != http.MethodPost {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a post request")
	}

	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}

	token, err := getTokenFromRequest(apiEnv, r)
	if err != nil {
		return err
	}

	// Make a request to the Auth service to get an augmented token.
	// We don't need to check the token validity since the Auth service will just reject bad tokens.
	req := &authpb.RefetchTokenRequest{
		Token: token,
	}

	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", token))

	resp, err := apiEnv.AuthClient().RefetchToken(ctxWithCreds, req)
	if status.Code(err) == codes.Unauthenticated {
		return ErrFetchAugmentedTokenFailedUnauthenticated
	}
	if err != nil {
		return ErrFetchAugmentedTokenFailedInternal
	}

	// GetDefaultSession, will always return a valid session, even if it is empty.
	// We don't check the err here because even if the preexisting
	// session cookie is expired or couldn't be decoded, we will overwrite it below anyway.
	session, _ := GetDefaultSession(apiEnv, r)
	// This should never be nil, but we check to be sure.
	if session == nil {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get session cookie")
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w, http.SameSiteStrictMode)
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthConnectorHandler receives an auth connector request and redirects to the auth connector callback with the access token.
func AuthConnectorHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
	apiEnv, ok := env.(apienv.APIEnv)
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, "failed to get environment")
	}
	if r.Method != http.MethodGet {
		return handler.NewStatusError(http.StatusMethodNotAllowed, "not a get request")
	}

	session, err := GetDefaultSession(apiEnv, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", session.Values["_at"].(string)))

	clusterName := r.URL.Query().Get("clusterName")

	resp, err := env.(apienv.APIEnv).AuthClient().GetAuthConnectorToken(ctxWithCreds, &authpb.GetAuthConnectorTokenRequest{ClusterName: clusterName})
	if err != nil {
		return handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	http.Redirect(w, r, viper.GetString("auth_connector_callback_url")+fmt.Sprintf("?token=%s", resp.Token), http.StatusSeeOther)
	return nil
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

func setSessionCookie(session *sessions.Session, token string, expiresAt int64, r *http.Request, w http.ResponseWriter, cookieMode http.SameSite) {
	// Set session cookie.
	session.Values["_at"] = token
	session.Values["_expires_at"] = expiresAt
	session.Options.MaxAge = int(time.Until(time.Unix(expiresAt, 0)).Seconds())
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.SameSite = cookieMode
	session.Options.Domain = viper.GetString("domain_name")

	err := session.Save(r, w)
	if err != nil {
		log.WithError(err).Error("Failed to write session cookie")
	}
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
	if userInfo != nil {
		data.UserInfo.UserID = utils.UUIDFromProtoOrNil(userInfo.UserID).String()
		data.UserInfo.Email = userInfo.Email
		data.UserInfo.FirstName = userInfo.FirstName
		data.UserInfo.LastName = userInfo.LastName
	}
	data.UserCreated = userCreated
	if orgInfo != nil {
		data.OrgInfo.OrgID = orgInfo.OrgID
		data.OrgInfo.OrgName = orgInfo.OrgName
	}

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
	data.UserInfo.UserID = utils.UUIDFromProtoOrNil(userInfo.UserID).String()
	data.UserInfo.Email = userInfo.Email
	data.UserInfo.FirstName = userInfo.FirstName
	data.UserInfo.LastName = userInfo.LastName
	data.OrgCreated = orgCreated

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)

	return json.NewEncoder(w).Encode(&data)
}

// loginWithAPIKey tries to login using the pixie-api-key header. It will return an error upon failure.
func loginWithAPIKey(ctx context.Context, env commonenv.Env, r *http.Request, w http.ResponseWriter) (string, int64, error) {
	apiKey := r.Header.Get("pixie-api-key")
	if apiKey == "" {
		return "", 0, errors.New("Could not get API key from header")
	}

	apiKeyResp, err := env.(apienv.APIEnv).AuthClient().GetAugmentedTokenForAPIKey(ctx, &authpb.GetAugmentedTokenForAPIKeyRequest{
		APIKey: apiKey,
	})
	if err != nil {
		return "", 0, services.HTTPStatusFromError(err, "Failed to login using API key")
	}

	return apiKeyResp.Token, apiKeyResp.ExpiresAt, nil
}

func parseOrgIDFromAPIKey(token string, key string, audience string) (string, error) {
	t, err := jwt.Parse([]byte(token), jwt.WithAudience(audience))
	if err != nil {
		return "", err
	}
	orgID, _ := t.Get("OrgID")
	return orgID.(string), nil
}
