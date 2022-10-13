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

func SetupOIDCViperEnvironment(t *testing.T, hostname string) func() {
	viper.Reset()

	viper.Set("oidc_host", hostname)
	viper.Set("oidc_client_id", "client-id")
	viper.Set("oidc_client_secret", "client-secret")
	viper.Set("google_oauth_userinfo_url", fmt.Sprintf("%s/google/oauth2/userinfo", hostname))
	viper.Set("oidc_userinfo_endpoint", fmt.Sprintf("%s/oauth2/userinfo", hostname))

	return func() {
		viper.Reset()
	}
}

func TestNewOIDCConn(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	conn, err := controllers.NewOIDCConnector()

	require.NoError(t, err)
	assert.Equal(t, "http://test_path", conn.Issuer)
	assert.Equal(t, "client-id", conn.ClientID)
	assert.Equal(t, "client-secret", conn.ClientSecret)
	assert.Equal(t, "http://test_path/.well-known/openid-configuration", conn.MetadataEndpoint)
	assert.Equal(t, "", conn.Metadata.Issuer)
	assert.Equal(t, "", conn.Metadata.AuthEndpoint)
	assert.Equal(t, "", conn.Metadata.TokenEndpoint)
	assert.Equal(t, "http://test_path/oauth2/userinfo", conn.Metadata.UserinfoEndpoint)
	assert.Equal(t, "", conn.IDProviderClaim)
	assert.Equal(t, "", conn.GoogleIdentityProvider)
	assert.Equal(t, "", conn.GoogleAccessTokenClaim)
}

func TestOIDCConnectorImpl_Init_MissingClientSecret(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_client_secret", "")
	_, err := controllers.NewOIDCConnector()

	assert.EqualError(t, err, "OIDC Client secret missing")
}

func TestOIDCConnectorImpl_Init_MissingClientID(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_client_id", "")
	_, err := controllers.NewOIDCConnector()

	assert.EqualError(t, err, "OIDC Client ID missing")
}

func TestOIDCConnectorImpl_Init_MissingHost(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_host", "")
	_, err := controllers.NewOIDCConnector()

	assert.EqualError(t, err, "OIDC issuer missing")
}

func TestOIDCConnectorImpl_Init_FetchEndpoints(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		assert.Equal(t, "/.well-known/openid-configuration", r.URL.String())
		_, err := w.Write([]byte(`{
			"issuer": "iss",
			"authorization_endpoint": "/auth",
		  "token_endpoint": "/token",
			"userinfo_endpoint": "/userinfo"
		}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	viper.Set("oidc_userinfo_endpoint", "")
	conn, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	assert.Equal(t, conn.Metadata.TokenEndpoint, "/token")
	assert.Equal(t, conn.Metadata.UserinfoEndpoint, "/userinfo")
}

func TestOIDCConnectorImpl_Init_OverrideFetchEndpoints(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		assert.Equal(t, "/meta", r.URL.String())
		_, err := w.Write([]byte(`{
			"issuer": "iss",
			"authorization_endpoint": "/auth",
		  "token_endpoint": "/oauth/token",
			"userinfo_endpoint": "/oauth/userinfo"
		}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	viper.Set("oidc_metadata_url", fmt.Sprintf("%s/meta", server.URL))
	viper.Set("oidc_userinfo_endpoint", "")
	conn, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	assert.Equal(t, conn.Metadata.TokenEndpoint, "/oauth/token")
	assert.Equal(t, conn.Metadata.UserinfoEndpoint, "/oauth/userinfo")
}

func TestOIDCConnectorImpl_Init_FetchEndpointsBadResp(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		assert.Equal(t, "/.well-known/openid-configuration", r.URL.String())
		_, err := w.Write([]byte(`{truncated......`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	viper.Set("oidc_userinfo_endpoint", "/userinfo2")
	conn, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	assert.Equal(t, conn.Metadata.TokenEndpoint, "")
	assert.Equal(t, conn.Metadata.UserinfoEndpoint, "/userinfo2")
}

func TestOIDCConnectorImpl_Init_MissingUserinfo(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_userinfo_endpoint", "")
	_, err := controllers.NewOIDCConnector()
	assert.ErrorContains(t, err, "Userinfo endpoint missing")
}

func TestOIDCConnectorImpl_Init_OverrideEndpoints(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_token_endpoint", "http://oidc.test_path/token")
	viper.Set("oidc_userinfo_endpoint", "http://test_path/v3/userinfo")

	conn, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	assert.Equal(t, conn.Metadata.TokenEndpoint, "http://oidc.test_path/token")
	assert.Equal(t, conn.Metadata.UserinfoEndpoint, "http://test_path/v3/userinfo")
}

func TestOIDCConnectorImpl_Init_GoogleMissingIDProviderKey(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_google_access_token_claim", "http://px.dev/google_access_token")

	_, err := controllers.NewOIDCConnector()
	assert.EqualError(t, err, "must set oidc_idprovider_claim and oidc_google_idprovider_value when setting oidc_google_access_token_claim")
}

func TestOIDCConnectorImpl_Init_SetIDPKey(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_idprovider_claim", "http://px.dev/identityProvider")

	conn, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	assert.Equal(t, conn.IDProviderClaim, "http://px.dev/identityProvider")
}

func TestOIDCConnectorImpl_Init_SetGoogleIDPValue(t *testing.T) {
	cleanup := SetupOIDCViperEnvironment(t, "http://test_path")
	defer cleanup()

	viper.Set("oidc_google_idprovider_value", "oidc-google-oauth")

	conn, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	assert.Equal(t, conn.GoogleIdentityProvider, "oidc-google-oauth")
}

func TestOIDCConnectorImpl_GetUserIDFromToken_BadResponse(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.String() == "/.well-known/openid-configuration" {
			_, _ = w.Write([]byte(`{}`))
			return
		}
		callCount++
		_, err := w.Write([]byte(`{"sub": `))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	c, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	userInfo, err := c.GetUserInfoFromAccessToken("test_token")
	assert.Equal(t, 1, callCount)
	assert.EqualError(t, err, "unexpected end of JSON input")
	assert.Nil(t, userInfo)
}

func TestOIDCConnectorImpl_GetUserInfoUnauthorizedToken(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.String() == "/.well-known/openid-configuration" {
			_, _ = w.Write([]byte(`{}`))
			return
		}
		callCount++
		// Return an unauthorized error.
		_, err := w.Write([]byte(`{"error": "access_denied", "error_description": "Unauthorized"}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	c, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	userInfo, err := c.GetUserInfoFromAccessToken("test_token")
	assert.Equal(t, 1, callCount)
	assert.EqualError(t, err, "invalid userinfo from OIDC")
	assert.Nil(t, userInfo)
}

func TestOIDCConnectorImpl_GetUserInfo(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.String() == "/.well-known/openid-configuration" {
			_, _ = w.Write([]byte(`{}`))
			return
		}
		callCount++
		assert.Equal(t, "/oauth2/userinfo", r.URL.String())
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))
		_, err := w.Write([]byte(`{
			"email": "testuser@test.com",
			"email_verified": false,
			"name": "Test User",
			"picture": "picture.jpg",
			"sub": "123990813094",
			"http://px.dev/identityProvider": "github"
		}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()
	viper.Set("oidc_idprovider_claim", "http://px.dev/identityProvider")

	c, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	userInfo, err := c.GetUserInfoFromAccessToken("test_token")
	require.NoError(t, err)
	assert.Equal(t, 1, callCount)
	assert.Equal(t, "testuser@test.com", userInfo.Email)
	assert.False(t, userInfo.EmailVerified)
	assert.Equal(t, "Test User", userInfo.Name)
	assert.Equal(t, "picture.jpg", userInfo.Picture)
	assert.Equal(t, "oidc", userInfo.IdentityProvider)
	assert.Equal(t, "123990813094", userInfo.AuthProviderID)
	assert.Equal(t, "", userInfo.HostedDomain)
}

func TestOIDCConnectorImpl_GetUserInfo_BadBools(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.String() == "/.well-known/openid-configuration" {
			_, _ = w.Write([]byte(`{}`))
			return
		}
		callCount++
		assert.Equal(t, "/oauth2/userinfo", r.URL.String())
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))
		_, err := w.Write([]byte(`{
			"email_verified": "false",
			"sub": "123990813094"
		}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()
	viper.Set("oidc_idprovider_claim", "http://px.dev/identityProvider")

	c, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	userInfo, err := c.GetUserInfoFromAccessToken("test_token")
	require.NoError(t, err)
	assert.Equal(t, 1, callCount)
	assert.False(t, userInfo.EmailVerified)
	assert.Equal(t, "123990813094", userInfo.AuthProviderID)
}

func TestOIDCConnectorImpl_GetUserInfo_GoogleOAuth(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.String() == "/.well-known/openid-configuration" {
			_, _ = w.Write([]byte(`{}`))
			return
		}
		callCount++
		if r.URL.String() == "/google/oauth2/userinfo" {
			_, err := w.Write([]byte(`{"hd": "test.com"}`))
			require.NoError(t, err)
			assert.Equal(t, "Bearer google-token", r.Header.Get("Authorization"))
			return
		}

		assert.Equal(t, "/oauth2/userinfo", r.URL.String())
		assert.Equal(t, "Bearer test_token", r.Header.Get("Authorization"))
		_, err := w.Write([]byte(`{
			"email": "testuser@test.com",
			"email_verified": true,
			"name": "Test User",
			"picture": "picture.jpg",
			"sub": "123990813094",
			"http://px.dev/google_access_token": "google-token",
			"http://px.dev/identityProvider": "oidc-google-oauth"
		}`))
		require.NoError(t, err)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	viper.Set("oidc_google_access_token_claim", "http://px.dev/google_access_token")
	viper.Set("oidc_idprovider_claim", "http://px.dev/identityProvider")
	viper.Set("oidc_google_idprovider_value", "oidc-google-oauth")

	c, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	userInfo, err := c.GetUserInfoFromAccessToken("test_token")
	require.NoError(t, err)
	assert.Equal(t, 2, callCount)
	assert.Equal(t, "testuser@test.com", userInfo.Email)
	assert.True(t, userInfo.EmailVerified)
	assert.Equal(t, "Test User", userInfo.Name)
	assert.Equal(t, "picture.jpg", userInfo.Picture)
	assert.Equal(t, "oidc", userInfo.IdentityProvider)
	assert.Equal(t, "123990813094", userInfo.AuthProviderID)
	assert.Equal(t, "test.com", userInfo.HostedDomain)
}

func TestOIDCConnectorImpl_GetUserInfo_BadResponse(t *testing.T) {
	callCount := 0
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.String() == "/.well-known/openid-configuration" {
			_, _ = w.Write([]byte(`{}`))
			return
		}
		callCount++
		if r.URL.String() != "/oauth2/userinfo" {
			require.Failf(t, "unexpected call to server URL: %s", r.URL.String())
			return
		}
		http.Error(w, "badness", http.StatusInternalServerError)
	}))
	defer server.Close()

	cleanup := SetupOIDCViperEnvironment(t, server.URL)
	defer cleanup()

	c, err := controllers.NewOIDCConnector()
	require.NoError(t, err)

	_, err = c.GetUserInfoFromAccessToken("token")
	assert.Equal(t, 1, callCount)
	assert.EqualError(t, err, "bad response from OIDC userinfo")
}
