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
	"fmt"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/auth/controllers"
)

func SetupViperEnvironment(t *testing.T, hostname string) func() {
	viper.Reset()
	viper.Set("auth0_host", hostname)
	viper.Set("auth0_client_id", "foo")
	viper.Set("auth0_client_secret", "bar")
	viper.Set("google_oauth_userinfo_url", fmt.Sprintf("%s/%s", hostname, "google/oauth2/userinfo"))

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
		_, err := w.Write([]byte(`{"sub": "myfakeuser"}`))
		require.NoError(t, err)
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
		_, err := w.Write([]byte(`{"sub": `))
		require.NoError(t, err)
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
			_, err := w.Write([]byte(`{"error": "access_denied", "error_description": "Unauthorized"}`))
			require.NoError(t, err)
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
			_, err := w.Write([]byte(`{"access_token": "test_token"}`))
			require.NoError(t, err)
			return
		}

		assert.Equal(t, "/api/v2/users/abcd", r.URL.String())
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))
		_, err := w.Write([]byte(`
         {
						"email": "testuser@test.com",
						"email_verified": false,
						"name": "Test User",
						"picture": "picture.jpg",
						"user_id": "github|123990813094",
						"app_metadata": {
							"foo": {
								"pl_user_id": "test_pl_user_id"
							}
						},
						"identities": [{
							"provider": "github"
						}]
         }`))
		require.NoError(t, err)
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
	assert.False(t, userInfo.EmailVerified)
	assert.Equal(t, "Test User", userInfo.Name)
	assert.Equal(t, "picture.jpg", userInfo.Picture)
	assert.Equal(t, "github", userInfo.IdentityProvider)
	assert.Equal(t, "github|123990813094", userInfo.AuthProviderID)
	assert.Equal(t, "", userInfo.HostedDomain)
}

func TestAuth0ConnectorImpl_GetUserInfo_GoogleOAuth(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		// Return valid management token.
		if r.URL.String() == "/oauth/token" {
			_, err := w.Write([]byte(`{"access_token": "test_token"}`))
			require.NoError(t, err)
			return
		}

		if r.URL.String() == "/google/oauth2/userinfo" {
			_, err := w.Write([]byte(`{"hd": "test.com"}`))
			require.NoError(t, err)
			assert.Equal(t, "Bearer google-token", r.Header.Get("Authorization"))
			return
		}

		assert.Equal(t, "/api/v2/users/abcd", r.URL.String())
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))
		_, err := w.Write([]byte(`
			{
				"email": "testuser@test.com",
				"email_verified": true,
				"name": "Test User",
				"picture": "picture.jpg",
				"user_id": "google-oauth2|123990813094",
				"app_metadata": {
					"foo": {
						"pl_user_id": "test_pl_user_id"
					}
				},
				"identities": [{
					"provider": "google-oauth2",
					"access_token": "google-token"
				}]
			}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupViperEnvironment(t, server.URL)
	defer cleanup()

	cfg := controllers.NewAuth0Config()
	c, err := controllers.NewAuth0Connector(cfg)
	require.NoError(t, err)

	userInfo, err := c.GetUserInfo("abcd")
	assert.Equal(t, 3, callCount)
	require.NoError(t, err)
	assert.Equal(t, "testuser@test.com", userInfo.Email)
	assert.True(t, userInfo.EmailVerified)
	assert.Equal(t, "Test User", userInfo.Name)
	assert.Equal(t, "picture.jpg", userInfo.Picture)
	assert.Equal(t, "google-oauth2", userInfo.IdentityProvider)
	assert.Equal(t, "google-oauth2|123990813094", userInfo.AuthProviderID)
	assert.Equal(t, "test.com", userInfo.HostedDomain)
}

func TestAuth0ConnectorImpl_GetUserInfo_BadResponse(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		// Return valid management token.
		if r.URL.String() == "/oauth/token" {
			_, err := w.Write([]byte(`{"access_token": "test_token"}`))
			require.NoError(t, err)
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
