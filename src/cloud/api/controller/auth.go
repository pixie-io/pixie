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

package controller

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"time"

	"github.com/gorilla/sessions"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"gopkg.in/segmentio/analytics-go.v3"

	"px.dev/pixie/src/cloud/api/apienv"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/shared/services"
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
		AccessToken string
		IDToken     string
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
		IdToken:     params.IDToken,
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
			UserId: utils.UUIDFromProtoOrNil(resp.UserID).String(),
			Event:  events.OrgCreated,
			Properties: analytics.NewProperties().
				Set("org_id", orgIDStr),
		})
	}

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w, http.SameSiteStrictMode)

	err = sendSignupUserInfo(w, resp.UserInfo, resp.Token, resp.ExpiresAt, resp.OrgCreated)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLoginEmbedHandler make requests to the authpb service and sets session cookies.
// Request-type: application/json.
// Params: accessToken (auth0 accessToken), redirectURI (relative path)
func AuthLoginEmbedHandler(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
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

	err := r.ParseForm()
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		return nil
	}
	accessToken := r.FormValue("accessToken")
	redirectURI := r.FormValue("redirectURI")

	if len(redirectURI) == 0 {
		http.Error(w, "redirectURI must be specified", http.StatusBadRequest)
		return nil
	}
	parsedURI, err := url.Parse(redirectURI)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadRequest)
		return nil
	}
	if len(parsedURI.Host) != 0 || len(parsedURI.Path) == 0 {
		http.Error(w, "redirectURI must be a relative path", http.StatusBadRequest)
		return nil
	}

	ctxWithCreds, err := attachCredentialsToContext(env, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	rpcReq := &authpb.LoginRequest{
		AccessToken:           accessToken,
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

	userIDStr := utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  events.UserLoggedIn,
	})

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w, http.SameSiteNoneMode)
	http.Redirect(w, r, "https://work."+viper.GetString("domain_name")+redirectURI, http.StatusSeeOther)
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
		OrgName     string `json:"orgName"`
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

	rpcReq := &authpb.LoginRequest{
		AccessToken:           params.AccessToken,
		CreateUserIfNotExists: true,
		OrgName:               params.OrgName,
		IdToken:               params.IDToken,
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

	userIDStr := utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
	ev := events.UserLoggedIn
	if resp.UserCreated {
		ev = events.UserSignedUp

		events.Client().Enqueue(&analytics.Identify{
			UserId: userIDStr,
			Traits: analytics.NewTraits().
				SetFirstName(resp.UserInfo.FirstName).
				SetLastName(resp.UserInfo.LastName).
				SetEmail(resp.UserInfo.Email),
		})
	}

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  ev,
	})

	setSessionCookie(session, resp.Token, resp.ExpiresAt, r, w, http.SameSiteNoneMode)

	err = sendUserInfo(w, resp.UserInfo, resp.OrgInfo, resp.Token, resp.ExpiresAt, resp.UserCreated)
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		return err
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	return nil
}

// AuthLoginHandlerEmbedNew is the replacement embed login handler.
// Request-type: application/json.
// Params: accessToken (auth0 accessToken), state.
func AuthLoginHandlerEmbedNew(env commonenv.Env, w http.ResponseWriter, r *http.Request) error {
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

	// If logging in using an API key, just get the augmented token.
	apiKey := r.Header.Get("pixie-api-key")
	if apiKey != "" {
		apiKeyResp, err := env.(apienv.APIEnv).AuthClient().GetAugmentedTokenForAPIKey(ctxWithCreds, &authpb.GetAugmentedTokenForAPIKeyRequest{
			APIKey: apiKey,
		})
		if err != nil {
			return services.HTTPStatusFromError(err, "Failed to login using API key")
		}

		err = sendUserInfo(w, nil, nil, apiKeyResp.Token, apiKeyResp.ExpiresAt, false)
		if err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			return err
		}

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		return nil
	}

	rpcReq := &authpb.LoginRequest{
		AccessToken:           params.AccessToken,
		CreateUserIfNotExists: false,
		OrgName:               "",
		IdToken:               params.IDToken,
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

	userIDStr := utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String()
	ev := events.UserLoggedIn

	events.Client().Enqueue(&analytics.Track{
		UserId: userIDStr,
		Event:  ev,
	})

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
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	// Delete the cookie.
	session.Values["_at"] = ""
	session.Values["_expires_at"] = 0
	session.Options.MaxAge = -1
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.Domain = viper.GetString("domain_name")

	err = session.Save(r, w)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}
	w.WriteHeader(http.StatusOK)
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
