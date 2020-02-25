package httpmiddleware_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"

	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	env2 "pixielabs.ai/pixielabs/src/shared/services/env"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestWithBearerAuthMiddleware(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := env2.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		sCtx, err := authcontext.FromContext(r.Context())
		assert.Nil(t, err)
		assert.NotNil(t, sCtx)
		assert.Equal(t, "test", sCtx.Claims.GetUserClaims().UserID)
		w.WriteHeader(http.StatusOK)
	}
	testToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	req, err := http.NewRequest("GET", "/api/users", nil)
	req.Header.Add("Authorization", "Bearer "+testToken)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)
}

func TestWithBearerAuthMiddleware_HealthzPass(t *testing.T) {
	env := env2.New()
	count := 0
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		count++
	}
	req, err := http.NewRequest("GET", "/healthz", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)

	assert.Equal(t, 1, count)
}

func TestWithBearerAuthMiddleware_BadBearer(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := env2.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		t.Fatal("Should not get here")
	}
	testToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	req, err := http.NewRequest("GET", "/api/users", nil)
	req.Header.Add("Authorization", "Bearr "+testToken)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestWithBearerAuthMiddleware_MissingAuthorization(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := env2.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		t.Fatal("Should not get here")
	}
	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestWithBearerAuthMiddleware_BadToken(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := env2.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		t.Fatal("Should not get here")
	}
	req, err := http.NewRequest("GET", "/api/users", nil)
	req.Header.Add("Authorization", "Bearer badtoken")
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}
