package httpmiddleware_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	"pixielabs.ai/pixielabs/src/shared/services/httpmiddleware"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestWithBearerAuthMiddleware(t *testing.T) {
	viper.Set("jwt_signing_key", "jwt-key")
	e := env.New()

	tests := []struct {
		Name          string
		Authorization string
		Path          string

		ExpectAuthSuccess bool
		// Only valid if auth is successful.
		ExpectHandlerAuthError bool
		ExpectHandlerUserID    string
	}{
		{
			Name:          "Auth Success With Bearer",
			Authorization: "Bearer " + testingutils.GenerateTestJWTToken(t, "jwt-key"),
			Path:          "/api/users",

			ExpectAuthSuccess:      true,
			ExpectHandlerAuthError: false,
			ExpectHandlerUserID:    testingutils.TestUserID,
		},
		{
			Name:          "Auth Success With Bearer",
			Authorization: "bearer " + testingutils.GenerateTestJWTToken(t, "jwt-key"),
			Path:          "/api/users",

			ExpectAuthSuccess:      true,
			ExpectHandlerAuthError: false,
			ExpectHandlerUserID:    testingutils.TestUserID,
		},
		{
			Name: "/healthz auth bypass",
			Path: "/healthz",

			ExpectAuthSuccess: true,
			// Not actually authorized, just bypass.
			ExpectHandlerAuthError: true,
		},
		{
			Name: "/healthz/subpath auth bypass",
			Path: "/healthz/subpath",

			ExpectAuthSuccess: true,
			// Not actually authorized, just bypass.
			ExpectHandlerAuthError: true,
		},
		{
			Name:              "Bad Bearer",
			Path:              "/api/users",
			Authorization:     "Bearr " + testingutils.GenerateTestJWTToken(t, "jwt-key"),
			ExpectAuthSuccess: false,
		},
		{
			Name:              "Missing Authorization",
			Path:              "/api/users",
			ExpectAuthSuccess: false,
		},
		{
			Name:              "Bad Token",
			Path:              "/api/users",
			Authorization:     "Bearer badtoken",
			ExpectAuthSuccess: false,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			handlerCallCount := 0
			testHandler := func(w http.ResponseWriter, r *http.Request) {
				handlerCallCount++
				sCtx, err := authcontext.FromContext(r.Context())
				if !test.ExpectHandlerAuthError {
					require.NoError(t, err)
					assert.NotNil(t, sCtx)
					assert.Equal(t, test.ExpectHandlerUserID, sCtx.Claims.GetUserClaims().UserID)

				} else {
					assert.NotNil(t, err)
				}
				w.WriteHeader(http.StatusOK)
			}
			req, err := http.NewRequest("GET", test.Path, nil)
			if len(test.Authorization) > 0 {
				req.Header.Add("Authorization", test.Authorization)
			}
			require.NoError(t, err)
			rr := httptest.NewRecorder()

			handler := httpmiddleware.WithBearerAuthMiddleware(
				e, http.HandlerFunc(testHandler))
			handler.ServeHTTP(rr, req)
			if test.ExpectAuthSuccess {
				assert.Equal(t, http.StatusOK, rr.Code)
				assert.Equal(t, 1, handlerCallCount)
			} else {
				assert.Equal(t, http.StatusUnauthorized, rr.Code)
				assert.Equal(t, 0, handlerCallCount)
			}
		})
	}

}
