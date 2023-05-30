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
	"errors"
	"fmt"
	"net/http"
	"net/url"
	"strings"

	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/api/apienv"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/httpmiddleware"
	"px.dev/pixie/src/shared/services/utils"
)

var (
	// ErrGetAuthTokenFailed occurs when we are unable to get a token from the cookie or bearer.
	ErrGetAuthTokenFailed = errors.New("failed to get auth token: either a bearer auth or valid cookie session must exist")
	// ErrFetchAugmentedTokenFailedInternal occurs when making a request for the augmented auth results in an internal error.
	ErrFetchAugmentedTokenFailedInternal = errors.New("failed to fetch token - internal")
	// ErrFetchAugmentedTokenFailedUnauthenticated occurs when making a request for the augmented token results in an authentication error.
	ErrFetchAugmentedTokenFailedUnauthenticated = errors.New("failed to fetch token - unauthenticated")
	// ErrParseAuthToken occurs when we are unable to parse the augmented token with the signing key.
	ErrParseAuthToken = errors.New("Failed to parse token")
	// ErrCSRFOriginCheckFailed occurs when a request with seesion cookie is missing the origin field, or is invalid.
	ErrCSRFOriginCheckFailed = errors.New("CSRF check missing origin")
	// TODO(zasgar): enable after we add this in the UI.
	// ErrCSRFTokenCheckFailed csrf double submit cookie was missing.
	// ErrCSRFTokenCheckFailed = errors.New("CSRF check missing token")
)

// GetTokenFromSession gets a token from the session store using cookies.
func GetTokenFromSession(env apienv.APIEnv, r *http.Request) (string, bool) {
	session, err := GetDefaultSession(env, r)
	if err != nil {
		return "", false
	}

	accessToken, ok := session.Values["_at"].(string)
	if !ok {
		return "", ok
	}

	return accessToken, true
}

// WithAugmentedAuthMiddleware augments auth by send minimal token to the auth server and
// using returned data to augment the session.
func WithAugmentedAuthMiddleware(env apienv.APIEnv, next http.Handler) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		ctx, err := getAugmentedAuthHTTP(env, r)
		if err != nil {
			if err == ErrFetchAugmentedTokenFailedUnauthenticated || err == ErrGetAuthTokenFailed ||
				err == ErrCSRFOriginCheckFailed {
				http.Error(w, err.Error(), http.StatusUnauthorized)
			} else {
				http.Error(w, err.Error(), http.StatusInternalServerError)
			}
			return
		}
		next.ServeHTTP(w, r.WithContext(ctx))
	}
	return http.HandlerFunc(f)
}

// getTokenFromRequest grabs the token according to the parameters in the request.
func getTokenFromRequest(env apienv.APIEnv, r *http.Request) (string, error) {
	referer, err := url.Parse(r.Referer())
	if err != nil {
		return "", err
	}
	// If referrer is set, force the origin check. This mitigates CSRF issues if the browser uses
	// bearer auth.
	if len(referer.Host) != 0 {
		if !checkOrigin(referer) {
			return "", ErrCSRFOriginCheckFailed
		}
	}
	// If the header "X-Use-Bearer is true we force the use of Bearer auth and ignore sessions.
	// This is needed to prevent logged in pixie sessions to show up in embedded versions.
	forceBearer := strings.ToLower(r.Header.Get("X-Use-Bearer")) == "true"
	var token string
	var ok bool

	// Try to get it from bearer.
	token, ok = httpmiddleware.GetTokenFromBearer(r)
	if !ok && !forceBearer && len(token) == 0 {
		// Try fallback session auth.
		token, ok = GetTokenFromSession(env, r)
		if ok {
			// We need to validate origin.
			if !checkOrigin(referer) {
				return "", ErrCSRFOriginCheckFailed
			}
		}
	}
	if len(token) == 0 {
		// No auth available.
		return "", ErrFetchAugmentedTokenFailedUnauthenticated
	}
	return token, nil
}

func getAugmentedToken(env apienv.APIEnv, r *http.Request) (string, error) {
	referer, err := url.Parse(r.Referer())
	if err != nil {
		return "", err
	}
	// If referrer is set, force the origin check. This mitigates CSRF issues if the browser uses
	// bearer auth.
	if len(referer.Host) != 0 {
		if !checkOrigin(referer) {
			return "", ErrCSRFOriginCheckFailed
		}
	}
	// Steps:
	// 1. Check if header contains a pixie-api-key. If so, generate augmented auth from the API Key.
	// 2. Try to get the token out of session.
	// 3. If not try to get the session out bearer
	// 4. Generate augmented auth.
	apiHeader := r.Header.Get("pixie-api-key")
	if apiHeader != "" {
		// Try to get augmented token.
		svcJWT := utils.GenerateJWTForService("APIService", viper.GetString("domain_name"))
		svcClaims, err := utils.SignJWTClaims(svcJWT, env.JWTSigningKey())
		if err != nil {
			return "", ErrGetAuthTokenFailed
		}
		ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
			fmt.Sprintf("bearer %s", svcClaims))

		apiKeyResp, err := env.AuthClient().GetAugmentedTokenForAPIKey(ctxWithCreds, &authpb.GetAugmentedTokenForAPIKeyRequest{
			APIKey: apiHeader,
		})
		if err == nil {
			// Get user/org info from augmented token.
			aCtx := authcontext.New()
			if err := aCtx.UseJWTAuth(env.JWTSigningKey(), apiKeyResp.Token, viper.GetString("domain_name")); err != nil {
				return "", ErrGetAuthTokenFailed
			}

			return apiKeyResp.Token, nil
		}
	}

	// If the header "X-Use-Bearer is true we force the use of Bearer auth and ignore sessions.
	// This is needed to prevent logged in pixie sessions to show up in embedded versions.
	forceBearer := false
	if strings.ToLower(r.Header.Get("X-Use-Bearer")) == "true" {
		forceBearer = true
	}
	var token string
	var ok bool

	// Try to get it from bearer.
	token, ok = httpmiddleware.GetTokenFromBearer(r)
	if !ok && !forceBearer && len(token) == 0 {
		// Try fallback session auth.
		token, ok = GetTokenFromSession(env, r)
		if ok {
			// We need to validate origin.
			if !checkOrigin(referer) {
				return "", ErrCSRFOriginCheckFailed
			}
		}
	}

	if len(token) == 0 {
		// No auth available.
		return "", ErrFetchAugmentedTokenFailedUnauthenticated
	}
	// Make a request to the Auth service to get an augmented token.
	// We don't need to check the token validity since the Auth service will just reject bad tokens.
	req := &authpb.GetAugmentedAuthTokenRequest{
		Token: token,
	}

	ctxWithCreds := metadata.AppendToOutgoingContext(r.Context(), "authorization",
		fmt.Sprintf("bearer %s", token))

	resp, err := env.AuthClient().GetAugmentedToken(ctxWithCreds, req)
	if err != nil {
		grpcCode := status.Code(err)
		if grpcCode == codes.Unauthenticated {
			return "", ErrFetchAugmentedTokenFailedUnauthenticated
		}
		return "", ErrFetchAugmentedTokenFailedInternal
	}
	return resp.Token, nil
}

func getAugmentedAuthHTTP(env apienv.APIEnv, r *http.Request) (context.Context, error) {
	token, err := getAugmentedToken(env, r)
	if err != nil {
		return nil, err
	}
	aCtx := authcontext.New()
	err = aCtx.UseJWTAuth(env.JWTSigningKey(), token, viper.GetString("domain_name"))
	if err != nil {
		return nil, ErrParseAuthToken
	}

	newCtx := authcontext.NewContext(r.Context(), aCtx)
	ctxWithAugmentedAuth := metadata.AppendToOutgoingContext(newCtx, "authorization",
		fmt.Sprintf("bearer %s", token))
	return ctxWithAugmentedAuth, nil
}

// GetAugmentedTokenGRPC gets the augmented token for a grpc context.
func GetAugmentedTokenGRPC(ctx context.Context, env apienv.APIEnv) (string, error) {
	md, ok := metadata.FromIncomingContext(ctx)
	if !ok {
		return "", errors.New("Could not get metadata from incoming md")
	}

	// The authcontext contains the GRPC path.
	sCtx, err := authcontext.FromContext(ctx)
	var urlPath *url.URL
	if err == nil {
		urlPath, _ = url.Parse(sCtx.Path)
	}

	r := &http.Request{Header: http.Header{}, URL: urlPath}
	for k, v := range md {
		for _, val := range v {
			r.Header.Add(k, val)
		}
	}
	return getAugmentedToken(env, r)
}

// sameOrigin returns true if URLs a and b share the same origin (but not subdomain). The same
// origin is defined as host (which includes the port) and scheme.
func sameOrigin(a, b *url.URL) bool {
	aParts := strings.Split(a.Host, ".")
	bParts := strings.Split(b.Host, ".")

	if len(aParts) < 2 || len(bParts) < 2 {
		return false
	}

	aHost := aParts[len(aParts)-2] + "." + aParts[len(aParts)-1]
	bHost := bParts[len(bParts)-2] + "." + bParts[len(bParts)-1]

	aHost = strings.Split(aHost, ":")[0]
	bHost = strings.Split(bHost, ":")[0]
	return (a.Scheme == b.Scheme && aHost == bHost)
}

func checkOrigin(a *url.URL) bool {
	expectedHost := &url.URL{
		Scheme: "https",
		Host:   viper.GetString("domain_name"),
	}
	return sameOrigin(a, expectedHost)
}
