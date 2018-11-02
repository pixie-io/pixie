package controllers_test

import (
	"context"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	authpb "pixielabs.ai/pixielabs/services/auth/proto"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
	"pixielabs.ai/pixielabs/services/gateway/controllers"
	"pixielabs.ai/pixielabs/services/gateway/controllers/testutils"
	"pixielabs.ai/pixielabs/services/gateway/gwenv"
	"pixielabs.ai/pixielabs/utils/testingutils"
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

func getTestCookie(t *testing.T, env gwenv.GatewayEnv) string {
	// Make a fake request to create a cookie with fake user credentials.
	req, err := http.NewRequest("GET", "/", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()
	session, err := env.CookieStore().Get(req, "default-session")
	assert.Nil(t, err)
	session.Values["_at"] = testingutils.GenerateTestJWTToken(t, "jwt-key")
	session.Save(req, rr)
	cookies, ok := rr.Header()["Set-Cookie"]
	assert.True(t, ok)
	assert.Equal(t, 1, len(cookies))
	return cookies[0]
}

func TestWithSessionAuthMiddlware(t *testing.T) {
	env, _, cleanup := testutils.CreateTestGatewayEnv(t)
	defer cleanup()

	cookie := getTestCookie(t, env)

	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()
	req.Header.Add("Cookie", cookie)

	sCtx := sessioncontext.New()
	ctx := sessioncontext.NewContext(req.Context(), sCtx)

	handler := controllers.WithSessionAuthMiddleware(env, callOKTestHandler(t))
	handler.ServeHTTP(rr, req.WithContext(ctx))

	assert.Equal(t, http.StatusOK, rr.Code)
}

func TestWithSessionAuthMiddlware_NoSession(t *testing.T) {
	env, _, cleanup := testutils.CreateTestGatewayEnv(t)
	defer cleanup()

	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	handler := controllers.WithSessionAuthMiddleware(env, callFailsTestHandler(t))

	sCtx := sessioncontext.New()
	ctx := sessioncontext.NewContext(req.Context(), sCtx)
	handler.ServeHTTP(rr, req.WithContext(ctx))

	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestWithAugmentedAuthMiddleware(t *testing.T) {
	env, mockAuthClient, cleanup := testutils.CreateTestGatewayEnv(t)
	defer cleanup()

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

	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)

	rr := httptest.NewRecorder()
	handler := controllers.WithAugmentedAuthMiddleware(env, callOKTestHandler(t))

	sCtx := sessioncontext.New()
	// The token that the auth middleware is supposed to extract.
	sCtx.AuthToken = "authpb-token"
	ctx := sessioncontext.NewContext(req.Context(), sCtx)
	handler.ServeHTTP(rr, req.WithContext(ctx))

	assert.Equal(t, http.StatusOK, rr.Code)
	// Make sure env was update with user info.
	assert.Equal(t, "test", sCtx.Claims.UserID)
	assert.Equal(t, "test@test.com", sCtx.Claims.Email)
	assert.Equal(t, testAugmentedToken, sCtx.AuthToken)
}
