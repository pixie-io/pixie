package httpmiddleware_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	commonenv "pixielabs.ai/pixielabs/services/common/env"
	"pixielabs.ai/pixielabs/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
	"pixielabs.ai/pixielabs/utils/testingutils"
)

func TestWithNewSessionMiddleware(t *testing.T) {
	checkNewSessionHandler := func(w http.ResponseWriter, r *http.Request) {
		sCtx, err := sessioncontext.FromContext(r.Context())
		assert.Nil(t, err)
		assert.NotNil(t, sCtx)
		w.WriteHeader(http.StatusOK)
	}

	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithNewSessionMiddleware(http.HandlerFunc(checkNewSessionHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)
}

func TestWithBearerAuthMiddleware(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := commonenv.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		sCtx, err := sessioncontext.FromContext(r.Context())
		assert.Nil(t, err)
		assert.NotNil(t, sCtx)
		assert.Equal(t, "test", sCtx.Claims.UserID)
		w.WriteHeader(http.StatusOK)
	}
	testToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	req, err := http.NewRequest("GET", "/api/users", nil)
	req.Header.Add("Authorization", "Bearer "+testToken)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithNewSessionMiddleware(
		httpmiddleware.WithBearerAuthMiddleware(
			env, http.HandlerFunc(testHandler)))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusOK, rr.Code)
}

func TestWithBearerAuthMiddleware_BadBearer(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := commonenv.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		t.Fatal("Should not get here")
	}
	testToken := testingutils.GenerateTestJWTToken(t, "jwt-key")
	req, err := http.NewRequest("GET", "/api/users", nil)
	req.Header.Add("Authorization", "Bearr "+testToken)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithNewSessionMiddleware(
		httpmiddleware.WithBearerAuthMiddleware(
			env, http.HandlerFunc(testHandler)))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}

func TestWithBearerAuthMiddleware_MissingAuthorization(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := commonenv.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		t.Fatal("Should not get here")
	}
	req, err := http.NewRequest("GET", "/api/users", nil)
	assert.Nil(t, err)
	rr := httptest.NewRecorder()

	handler := httpmiddleware.WithNewSessionMiddleware(
		httpmiddleware.WithBearerAuthMiddleware(
			env, http.HandlerFunc(testHandler)))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}
