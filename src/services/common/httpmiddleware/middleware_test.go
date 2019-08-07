package httpmiddleware_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/services/common/authcontext"
	commonenv "pixielabs.ai/pixielabs/src/services/common/env"
	"pixielabs.ai/pixielabs/src/services/common/httpmiddleware"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestWithBearerAuthMiddleware(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	env := commonenv.New()
	testHandler := func(w http.ResponseWriter, r *http.Request) {
		sCtx, err := authcontext.FromContext(r.Context())
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

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
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

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
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

	handler := httpmiddleware.WithBearerAuthMiddleware(
		env, http.HandlerFunc(testHandler))
	handler.ServeHTTP(rr, req)
	assert.Equal(t, http.StatusUnauthorized, rr.Code)
}
