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

package controllers_test

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/api/apienv"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/auth/authpb"
	mock_auth "px.dev/pixie/src/cloud/auth/authpb/mock"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils/testingutils"
)

func callOKTestHandler(t *testing.T) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}
	return http.HandlerFunc(f)
}

func callFailsTestHandler(t *testing.T) http.Handler {
	f := func(w http.ResponseWriter, r *http.Request) {
		t.Fatal("This handler should never be called.")
	}
	return http.HandlerFunc(f)
}

func getTestCookie(t *testing.T, env apienv.APIEnv) string {
	// Make a fake request to create a cookie with fake user credentials.
	req, err := http.NewRequest("GET", "/", nil)
	require.NoError(t, err)
	rr := httptest.NewRecorder()
	session, err := env.CookieStore().Get(req, "default-session5")
	require.NoError(t, err)
	session.Values["_at"] = "authpb-token"
	err = session.Save(req, rr)
	require.NoError(t, err)
	cookies, ok := rr.Header()["Set-Cookie"]
	assert.True(t, ok)
	assert.Equal(t, 1, len(cookies))
	return cookies[0]
}

func validRequestCheckHelper(t *testing.T, env apienv.APIEnv, mockAuthClient *mock_auth.MockAuthServiceClient, req *http.Request) {
	testAugmentedToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	rpcResp := &authpb.GetAugmentedAuthTokenResponse{
		Token: testAugmentedToken,
	}

	mockAuthClient.EXPECT().GetAugmentedToken(
		gomock.Any(), gomock.Any()).Do(
		func(c context.Context, request *authpb.GetAugmentedAuthTokenRequest) {
			assert.Equal(t, "authpb-token", request.Token)
		}).Return(
		rpcResp, nil)

	// This function is an HTTP handler that will validate that the auth information is available
	// to handlers.
	validateAuthInfo := func(w http.ResponseWriter, r *http.Request) {
		aCtx, err := authcontext.FromContext(r.Context())
		require.NoError(t, err)
		assert.Equal(t, testingutils.TestUserID, aCtx.Claims.GetUserClaims().UserID)
		assert.Equal(t, "test@test.com", aCtx.Claims.GetUserClaims().Email)
		assert.Equal(t, testAugmentedToken, aCtx.AuthToken)

		md, ok := metadata.FromOutgoingContext(r.Context())
		assert.Equal(t, true, ok)
		assert.Equal(t, 1, len(md["authorization"]))
		assert.Equal(t, fmt.Sprintf("bearer %s", testAugmentedToken), md["authorization"][0])

		callOKTestHandler(t).ServeHTTP(w, r)
	}

	rr := httptest.NewRecorder()
	handler := controllers.WithAugmentedAuthMiddleware(env, http.HandlerFunc(validateAuthInfo))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)
}

func failedRequestCheckHelper(t *testing.T, env apienv.APIEnv, mockAuthClient *mock_auth.MockAuthServiceClient, req *http.Request) {
	rr := httptest.NewRecorder()
	handler := controllers.WithAugmentedAuthMiddleware(env, callFailsTestHandler(t))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestWithAugmentedAuthMiddlewareWithSession(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)

	req.Header.Set("Origin", "https://work.withpixie.ai")
	req.Header.Set("Referer", "https://work.withpixie.ai")
	require.NoError(t, err)
	cookie := getTestCookie(t, env)
	req.Header.Add("Cookie", cookie)

	validRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithSessionWithRefererPort(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)

	req.Header.Set("Origin", "https://work.withpixie.ai:8080")
	req.Header.Set("Referer", "https://work.withpixie.ai:8080")
	require.NoError(t, err)
	cookie := getTestCookie(t, env)
	req.Header.Add("Cookie", cookie)

	validRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithSessionForceBearer(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)

	req.Header.Set("Origin", "https://work.withpixie.ai")
	req.Header.Set("Referer", "https://work.withpixie.ai")
	req.Header.Set("X-Use-Bearer", "true")
	require.NoError(t, err)
	cookie := getTestCookie(t, env)
	req.Header.Add("Cookie", cookie)

	failedRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithSessionBadOrigin(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)

	req.Header.Set("Origin", "https://bad.com")
	req.Header.Set("Referer", "https://bad.com")
	require.NoError(t, err)
	cookie := getTestCookie(t, env)
	req.Header.Add("Cookie", cookie)

	failedRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithSessionMissingOrigin(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)
	cookie := getTestCookie(t, env)
	req.Header.Add("Cookie", cookie)

	failedRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithBearerWithOriginGood(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)
	req.Header.Add("Authorization", "Bearer authpb-token")

	req.Header.Set("Origin", "https://work.withpixie.ai")
	req.Header.Set("Referer", "https://work.withpixie.ai")

	validRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithBearerWithOriginBad(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)
	req.Header.Add("Authorization", "Bearer authpb-token")

	req.Header.Set("Origin", "https://bad.com")
	req.Header.Set("Referer", "https://bad.com")

	failedRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithBearer(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)
	req.Header.Add("Authorization", "Bearer authpb-token")

	validRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareMissingAuth(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)

	failedRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareFailedAugmentation(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	mockClients.MockAuth.EXPECT().GetAugmentedToken(
		gomock.Any(), gomock.Any()).Do(
		func(c context.Context, request *authpb.GetAugmentedAuthTokenRequest) {
			assert.Equal(t, "bad-token", request.Token)
		}).Return(
		nil, status.Error(codes.Unauthenticated, "failed auth check"))

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)
	req.Header.Add("Authorization", "Bearer bad-token")

	failedRequestCheckHelper(t, env, mockClients.MockAuth, req)
}

func TestWithAugmentedAuthMiddlewareWithAPIKey(t *testing.T) {
	env, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	testAugmentedToken := testingutils.GenerateTestJWTToken(t, "jwt-key")

	mockClients.MockAuth.EXPECT().GetAugmentedTokenForAPIKey(
		gomock.Any(), gomock.Any()).Do(
		func(c context.Context, request *authpb.GetAugmentedTokenForAPIKeyRequest) {
			assert.Equal(t, "test-api-key", request.APIKey)
		}).Return(
		&authpb.GetAugmentedTokenForAPIKeyResponse{
			Token: testAugmentedToken,
		}, nil)

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	require.NoError(t, err)
	req.Header.Add("pixie-api-key", "test-api-key")

	validateAuthInfo := func(w http.ResponseWriter, r *http.Request) {
		aCtx, err := authcontext.FromContext(r.Context())
		require.NoError(t, err)
		assert.Equal(t, testingutils.TestUserID, aCtx.Claims.GetUserClaims().UserID)
		assert.Equal(t, "test@test.com", aCtx.Claims.GetUserClaims().Email)
		assert.Equal(t, testAugmentedToken, aCtx.AuthToken)

		md, ok := metadata.FromOutgoingContext(r.Context())
		assert.Equal(t, true, ok)
		assert.Equal(t, 1, len(md["authorization"]))
		assert.Equal(t, fmt.Sprintf("bearer %s", testAugmentedToken), md["authorization"][0])

		callOKTestHandler(t).ServeHTTP(w, r)
	}

	rr := httptest.NewRecorder()
	handler := controllers.WithAugmentedAuthMiddleware(env, http.HandlerFunc(validateAuthInfo))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)
}
