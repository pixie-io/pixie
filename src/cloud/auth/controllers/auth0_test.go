package controllers_test

import (
	"io/ioutil"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/cloud/auth/controllers"
)

func SetupViperEnvironment(t *testing.T, hostname string) func() {
	viper.Reset()
	viper.Set("auth0_host", hostname)
	viper.Set("auth0_client_id", "foo")
	viper.Set("auth0_client_secret", "bar")

	return func() {
		viper.Reset()
	}
}

func TestNewAuth0Config(t *testing.T) {
	cleanup := SetupViperEnvironment(t, "http://test_path")
	defer cleanup()

	auth0Cfg := controllers.NewAuth0Config()

	assert.Equal(t, "http://test_path", auth0Cfg.Auth0Host)
	assert.Equal(t, "http://test_path/api/v2", auth0Cfg.Auth0MgmtAPI)
	assert.Equal(t, "http://test_path/oauth/token", auth0Cfg.Auth0OAuthTokenEndpoint)
	assert.Equal(t, "http://test_path/userinfo", auth0Cfg.Auth0UserInfoEndpoint)
	assert.Equal(t, "foo", auth0Cfg.Auth0ClientID)
	assert.Equal(t, "bar", auth0Cfg.Auth0ClientSecret)
}

func TestAuth0ConnectorImpl_Init_MissingSecret(t *testing.T) {
	cleanup := SetupViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("auth0_client_secret", "")
	auth0Cfg := controllers.NewAuth0Config()
	_, err := controllers.NewAuth0Connector(auth0Cfg)

	assert.EqualError(t, err, "auth0 Client secret missing")
}

func TestAuth0ConnectorImpl_Init_MissingClientID(t *testing.T) {
	cleanup := SetupViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("auth0_client_id", "")
	auth0Cfg := controllers.NewAuth0Config()
	_, err := controllers.NewAuth0Connector(auth0Cfg)

	assert.EqualError(t, err, "auth0 Client ID missing")
}

func TestAuth0ConnectorImpl_GetUserIDFromToken(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, "/userinfo", r.URL.String())
		callCount++
		assert.Equal(t, "Bearer abcd", r.Header.Get("Authorization"))
		w.Write([]byte(`{"sub": "myfakeuser"}`))
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	userID, err := c.GetUserIDFromToken("abcd")
	assert.Equal(t, 1, callCount)
	require.NoError(t, err)
	assert.Equal(t, "myfakeuser", userID)
}

func TestAuth0ConnectorImpl_GetUserIDFromToken_BadStatus(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		http.Error(w, "badness", http.StatusInternalServerError)
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	_, err = c.GetUserIDFromToken("abcd")
	assert.EqualError(t, err, "bad response from auth0")
}

func TestAuth0ConnectorImpl_GetUserIDFromToken_BadResponse(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte(`{"sub": `))
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	_, err = c.GetUserIDFromToken("abcd")
	assert.NotNil(t, err)
}

func TestAuth0ConnectorImpl_GetUserInfoUnauthorizedToken(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		// Return an unauthorized error.
		if r.URL.String() == "/oauth/token" {
			w.Write([]byte(`{"error": "access_denied", "error_description": "Unauthorized"}`))
			return
		}
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	userInfo, err := c.GetUserInfo("abcd")
	assert.NotNil(t, err)
	assert.Nil(t, userInfo)
}

func TestAuth0ConnectorImpl_GetUserInfo(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		// Return valid management token.
		if r.URL.String() == "/oauth/token" {
			w.Write([]byte(`{"access_token": "test_token"}`))
			return
		}

		assert.Equal(t, "/api/v2/users/abcd", r.URL.String())
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))
		w.Write([]byte(`
         {
              "email": "testuser@test.com",
              "name": "Test User",
              "picture": "picture.jpg",
              "sub": "test_sub",
              "app_metadata": {
          			"foo": {
          				"pl_user_id": "test_pl_user_id"
          			}
              }
         }
        `))
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	userInfo, err := c.GetUserInfo("abcd")
	assert.Equal(t, 2, callCount)
	require.NoError(t, err)
	assert.Equal(t, "testuser@test.com", userInfo.Email)
	assert.Equal(t, "Test User", userInfo.Name)
	assert.Equal(t, "picture.jpg", userInfo.Picture)
	assert.Equal(t, "test_pl_user_id", userInfo.PLUserID)
}

func TestAuth0ConnectorImpl_GetUserInfo_BadResponse(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		// Return valid management token.
		if r.URL.String() == "/oauth/token" {
			w.Write([]byte(`{"access_token": "test_token"}`))
			return
		}
		http.Error(w, "badness", http.StatusInternalServerError)
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	_, err = c.GetUserInfo("abcd")
	assert.Equal(t, 2, callCount)
	assert.NotNil(t, err)
}

func TestAuth0ConnectorImpl_SetPLMetadata(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		// Return valid management token.
		if r.URL.String() == "/oauth/token" {
			w.Write([]byte(`{"access_token": "test_token"}`))
			return
		}

		assert.Equal(t, "/api/v2/users/abcd", r.URL.String())
		assert.Equal(t, "PATCH", r.Method)
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))

		body, err := ioutil.ReadAll(r.Body)
		require.NoError(t, err)
		defer r.Body.Close()

		assert.JSONEq(t,
			`{"app_metadata":{"foo":{"pl_org_id":"test_pl_org_id", "pl_user_id":"test_pl_user_id"}}}`, string(body))
		w.Write([]byte(`OK`))
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	err = c.SetPLMetadata("abcd", "test_pl_org_id", "test_pl_user_id")
	assert.Equal(t, 2, callCount)
	require.NoError(t, err)
}
