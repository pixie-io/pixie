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

package controllers

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("auth0_host", "https://pixie-labs.auth0.com", "The auth0 hostname")
	pflag.String("auth0_client_id", "", "Auth0 client ID")
	pflag.String("auth0_client_secret", "", "Auth0 client secret")
}

// Returns an DomainName for the identity according to the IdentityProvider.
// If the returned string is empty, the identity does not belong to an org.
func (a *Auth0Connector) retrieveHostedDomain(ident *auth0Identity) (string, error) {
	// We only implement for google-oauth2 connections for now.
	if ident.Provider != googleIdentityProvider {
		return "", nil
	}
	return retrieveGoogleHostedDomain(ident.AccessToken)
}

type auth0Identity struct {
	Provider    string `json:"provider,omitempty"`
	AccessToken string `json:"access_token,omitempty"`
}

// auth0UserInfo tracks the returned auth0 info.
type auth0UserInfo struct {
	Email         string           `json:",omitempty"`
	EmailVerified bool             `json:"email_verified,omitempty"`
	FirstName     string           `json:"given_name,omitempty"`
	LastName      string           `json:"family_name,omitempty"`
	UserID        string           `json:"user_id,omitempty"`
	Name          string           `json:",omitempty"`
	Picture       string           `json:",omitempty"`
	Sub           string           `json:"sub,omitempty"`
	Identities    []*auth0Identity `json:"identities,omitempty"`
}

// Auth0Config is the config data required for Auth0.
type Auth0Config struct {
	Auth0Host               string
	Auth0MgmtAPI            string
	Auth0OAuthTokenEndpoint string
	Auth0UserInfoEndpoint   string
	Auth0ClientID           string
	Auth0ClientSecret       string
}

// NewAuth0Config generates and Auth0Config based on env vars and flags.
func NewAuth0Config() Auth0Config {
	auth0Host := viper.GetString("auth0_host")
	cfg := Auth0Config{
		Auth0Host:               auth0Host,
		Auth0MgmtAPI:            auth0Host + "/api/v2",
		Auth0OAuthTokenEndpoint: auth0Host + "/oauth/token",
		Auth0UserInfoEndpoint:   auth0Host + "/userinfo",
		Auth0ClientID:           viper.GetString("auth0_client_id"),
		Auth0ClientSecret:       viper.GetString("auth0_client_secret"),
	}
	return cfg
}

// Auth0Connector implements the AuthProvider interface for Auth0.
type Auth0Connector struct {
	cfg Auth0Config
}

// NewAuth0Connector provides an implementation of an Auth0Connector.
func NewAuth0Connector(cfg Auth0Config) (*Auth0Connector, error) {
	ac := &Auth0Connector{cfg: cfg}
	err := ac.init()
	if err != nil {
		return nil, err
	}
	return ac, nil
}

func (a *Auth0Connector) init() error {
	if a.cfg.Auth0ClientID == "" {
		return errors.New("auth0 Client ID missing")
	}

	if a.cfg.Auth0ClientSecret == "" {
		return errors.New("auth0 Client secret missing")
	}

	return nil
}

// GetUserIDFromToken returns the UserID for the particular token.
func (a *Auth0Connector) GetUserIDFromToken(token string) (string, error) {
	client := &http.Client{}

	req, err := http.NewRequest("GET", a.cfg.Auth0UserInfoEndpoint, nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("Authorization",
		fmt.Sprintf("Bearer %s", token))
	req.Header.Set("Content-Type", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	if resp.StatusCode != http.StatusOK {
		return "", errors.New("bad response from auth0")
	}

	body, err := io.ReadAll(resp.Body)
	defer resp.Body.Close()

	if err != nil {
		return "", err
	}

	var userInfo struct {
		Sub string `json:"sub,omitempty"`
	}
	if err = json.Unmarshal(body, &userInfo); err != nil {
		return "", err
	}
	return userInfo.Sub, nil
}

func (a *Auth0Connector) getManagementToken() (string, error) {
	payload := map[string]interface{}{
		"grant_type":    "client_credentials",
		"client_id":     a.cfg.Auth0ClientID,
		"client_secret": a.cfg.Auth0ClientSecret,
		"audience":      a.cfg.Auth0MgmtAPI + "/",
	}

	jsonPayload, err := json.Marshal(payload)
	if err != nil {
		return "", err
	}

	log.Debug("Requesting AUTH0 management token.")
	resp, err := http.Post(a.cfg.Auth0OAuthTokenEndpoint, "application/json", bytes.NewReader(jsonPayload))
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}

	type TokenError struct {
		Error            string `json:"error"`
		ErrorDescription string `json:"error_description"`
	}

	var tokenError TokenError
	if err = json.Unmarshal(body, &tokenError); err != nil {
		return "", err
	}
	if tokenError.Error != "" {
		return "", fmt.Errorf("Error when getting management token: %s", tokenError.ErrorDescription)
	}

	type TokenInfo struct {
		AccessToken string `json:"access_token"`
	}
	var tokenInfo TokenInfo
	if err = json.Unmarshal(body, &tokenInfo); err != nil {
		return "", err
	}
	return tokenInfo.AccessToken, nil
}

// GetUserInfo returns the UserInfo for this userID.
func (a *Auth0Connector) GetUserInfo(userID string) (*UserInfo, error) {
	getPath := fmt.Sprintf("%s/users/%s", a.cfg.Auth0MgmtAPI, userID)

	client := &http.Client{}
	req, err := http.NewRequest("GET", getPath, nil)
	if err != nil {
		return nil, err
	}

	managementToken, err := a.getManagementToken()
	if err != nil {
		return nil, err
	}
	req.Header.Set("Authorization", fmt.Sprintf("Bearer %s", managementToken))
	req.Header.Set("Content-Type", "application/json")
	resp, err := client.Do(req)

	if err != nil {
		return nil, err
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("bad response when getting user app metadata: %d", resp.StatusCode)
	}

	body, err := io.ReadAll(resp.Body)
	defer resp.Body.Close()

	if err != nil {
		return nil, err
	}

	userInfo := &auth0UserInfo{}
	if err = json.Unmarshal(body, userInfo); err != nil {
		return nil, err
	}

	var idp string
	if len(userInfo.Identities) >= 1 {
		idp = userInfo.Identities[0].Provider
	}
	// Log the case when auth0.Identities > 1 so we can account for it later.
	if len(userInfo.Identities) > 1 {
		idps := make([]string, len(userInfo.Identities))
		for i, ident := range userInfo.Identities {
			idps[i] = ident.Provider
		}
		log.WithField("idps", strings.Join(idps, ",")).Error("User has multiple idproviders")
		return nil, fmt.Errorf("User has multiple idproviders: %s", strings.Join(idps, ","))
	}

	hostedDomain, err := a.retrieveHostedDomain(userInfo.Identities[0])
	if err != nil {
		return nil, err
	}

	// Convert auth0UserInfo to UserInfo.
	u := &UserInfo{
		Email:            userInfo.Email,
		EmailVerified:    userInfo.EmailVerified,
		FirstName:        userInfo.FirstName,
		LastName:         userInfo.LastName,
		Name:             userInfo.Name,
		Picture:          userInfo.Picture,
		IdentityProvider: idp,
		AuthProviderID:   userInfo.UserID,
		HostedDomain:     hostedDomain,
	}
	return u, nil
}

// GetUserInfoFromAccessToken fetches and returns the UserInfo for the given access token.
func (a *Auth0Connector) GetUserInfoFromAccessToken(accessToken string) (*UserInfo, error) {
	userID, err := a.GetUserIDFromToken(accessToken)
	if err != nil {
		return nil, err
	}
	return a.GetUserInfo(userID)
}

// CreateInviteLink implements the AuthProvider interface, but we don't support this functionatlity with Auth0 at the time.
func (a *Auth0Connector) CreateInviteLink(authProviderID string) (*CreateInviteLinkResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support inviting users with InviteLinks")
}

// CreateIdentity implements the AuthProvider interface, but we don't support this functionatlity with Auth0 at the time.
func (a *Auth0Connector) CreateIdentity(string) (*CreateIdentityResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support creating identities")
}
