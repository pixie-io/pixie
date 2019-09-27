package controller_test

import (
	"context"
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	mock_auth "pixielabs.ai/pixielabs/src/cloud/auth/proto/mock"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
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

func getTestCookie(t *testing.T, env apienv.APIEnv, domain string) string {
	// Make a fake request to create a cookie with fake user credentials.
	req, err := http.NewRequest("GET", "/", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()
	session, err := env.CookieStore().Get(req, "default-session")
	assert.Nil(t, err)
	session.Values["_at"] = "authpb-token"
	session.Values["_auth_domain"] = domain
	session.Save(req, rr)
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
		assert.Nil(t, err)
		assert.Equal(t, "test", aCtx.Claims.GetUserClaims().UserID)
		assert.Equal(t, "test@test.com", aCtx.Claims.GetUserClaims().Email)
		assert.Equal(t, testAugmentedToken, aCtx.AuthToken)

		md, ok := metadata.FromOutgoingContext(r.Context())
		assert.Equal(t, true, ok)
		assert.Equal(t, 1, len(md["authorization"]))
		assert.Equal(t, fmt.Sprintf("bearer %s", testAugmentedToken), md["authorization"][0])

		callOKTestHandler(t).ServeHTTP(w, r)
	}

	rr := httptest.NewRecorder()
	handler := controller.WithAugmentedAuthMiddleware(env, http.HandlerFunc(validateAuthInfo))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)
}

func failedRequestCheckHelper(t *testing.T, env apienv.APIEnv, mockAuthClient *mock_auth.MockAuthServiceClient, req *http.Request) {
	rr := httptest.NewRecorder()
	handler := controller.WithAugmentedAuthMiddleware(env, callFailsTestHandler(t))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestWithAugmentedAuthMiddlewareWithSession(t *testing.T) {
	env, mockAuthClient, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	assert.Nil(t, err)
	cookie := getTestCookie(t, env, "pixie")
	req.Header.Add("Cookie", cookie)

	validRequestCheckHelper(t, env, mockAuthClient, req)
}

func TestWithAugmentedAuthMiddlewareWithSession_IncorrectHost(t *testing.T) {
	env, mockAuthClient, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	assert.Nil(t, err)
	cookie := getTestCookie(t, env, "blah")
	req.Header.Add("Cookie", cookie)

	failedRequestCheckHelper(t, env, mockAuthClient, req)
}

func TestWithAugmentedAuthMiddlewareWithBearer(t *testing.T) {
	env, mockAuthClient, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	assert.Nil(t, err)
	req.Header.Add("Authorization", "Bearer authpb-token")

	validRequestCheckHelper(t, env, mockAuthClient, req)
}

func TestWithAugmentedAuthMiddlewareMissingAuth(t *testing.T) {
	env, mockAuthClient, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	assert.Nil(t, err)

	failedRequestCheckHelper(t, env, mockAuthClient, req)
}

func TestWithAugmentedAuthMiddlewareFailedAugmentation(t *testing.T) {
	env, mockAuthClient, _, _, _, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	mockAuthClient.EXPECT().GetAugmentedToken(
		gomock.Any(), gomock.Any()).Do(
		func(c context.Context, request *authpb.GetAugmentedAuthTokenRequest) {
			assert.Equal(t, "bad-token", request.Token)
		}).Return(
		nil, status.Error(codes.Unauthenticated, "failed auth check"))

	req, err := http.NewRequest("GET", "https://pixie.dev.pixielabs.dev/api/users", nil)
	assert.Nil(t, err)
	req.Header.Add("Authorization", "Bearer bad-token")

	failedRequestCheckHelper(t, env, mockAuthClient, req)
}
