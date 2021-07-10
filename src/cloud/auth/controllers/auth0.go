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
	"io/ioutil"
	"net/http"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("auth0_host", "https://pixie-labs.auth0.com", "The auth0 hostname")
	pflag.String("auth0_client_id", "", "Auth0 client ID")
	pflag.String("auth0_client_secret", "", "Auth0 client secret")
}

func getIdentityProvider(auth0 *auth0UserInfo) string {
	if len(auth0.Identities) != 1 {
		return ""
	}
	return auth0.Identities[0].Provider
}

func transformAuth0UserInfoToUserInfo(auth0 *auth0UserInfo, clientID string) (*UserInfo, error) {
	// If user does not exist in Auth0, then create a new user if specified.
	u := &UserInfo{
		Email:            auth0.Email,
		FirstName:        auth0.FirstName,
		LastName:         auth0.LastName,
		Name:             auth0.Name,
		Picture:          auth0.Picture,
		IdentityProvider: getIdentityProvider(auth0),
		AuthProviderID:   auth0.UserID,
	}
	if !(auth0.AppMetadata == nil || auth0.AppMetadata[clientID] == nil) {
		u.PLUserID = auth0.AppMetadata[clientID].PLUserID
		u.PLOrgID = auth0.AppMetadata[clientID].PLOrgID
	}
	return u, nil
}

// auth0UserMetadata is a part of the Auth0 response.
type auth0UserMetadata struct {
	PLUserID string `json:"pl_user_id,omitempty"`
	PLOrgID  string `json:"pl_org_id,omitempty"`
}

type auth0Identity struct {
	Provider string `json:"provider,omitempty"`
}

// auth0UserInfo tracks the returned auth0 info.
type auth0UserInfo struct {
	Email       string                        `json:",omitempty"`
	FirstName   string                        `json:"given_name,omitempty"`
	LastName    string                        `json:"family_name,omitempty"`
	UserID      string                        `json:"user_id,omitempty"`
	Name        string                        `json:",omitempty"`
	Picture     string                        `json:",omitempty"`
	Sub         string                        `json:"sub,omitempty"`
	AppMetadata map[string]*auth0UserMetadata `json:"app_metadata,omitempty"`
	Identities  []*auth0Identity              `json:"identities,omitempty"`
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

	body, err := ioutil.ReadAll(resp.Body)
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

	body, err := ioutil.ReadAll(resp.Body)
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

	body, err := ioutil.ReadAll(resp.Body)
	defer resp.Body.Close()

	if err != nil {
		return nil, err
	}

	userInfo := &auth0UserInfo{}
	if err = json.Unmarshal(body, userInfo); err != nil {
		return nil, err
	}

	return transformAuth0UserInfoToUserInfo(userInfo, a.cfg.Auth0ClientID)
}

// SetPLMetadata sets the pixielabs related metadata in the auth0 client.
func (a *Auth0Connector) SetPLMetadata(userID, plOrgID, plUserID string) error {
	appMetadata := make(map[string]*auth0UserMetadata)
	appMetadata[a.cfg.Auth0ClientID] = &auth0UserMetadata{
		PLUserID: plUserID,
		PLOrgID:  plOrgID,
	}

	userInfo := &auth0UserInfo{
		AppMetadata: appMetadata,
	}

	jsonStr, err := json.Marshal(userInfo)
	if err != nil {
		return err
	}

	client := &http.Client{}
	patchPath := fmt.Sprintf("%s/users/%s", a.cfg.Auth0MgmtAPI, userID)
	log.Printf(patchPath)
	req, err := http.NewRequest("PATCH", patchPath, bytes.NewBuffer(jsonStr))
	if err != nil {
		return err
	}

	managementToken, err := a.getManagementToken()
	if err != nil {
		return err
	}

	req.Header.Set("Authorization", fmt.Sprintf("Bearer %s", managementToken))
	req.Header.Set("Content-Type", "application/json")
	resp, err := client.Do(req)
	if err != nil {
		return err
	}
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("bad response from auth0: %d", resp.StatusCode)
	}
	return nil
}

// CreateInviteLink implements the AuthProvider interface, but we don't support this functionatlity with Auth0 at the time.
func (a *Auth0Connector) CreateInviteLink(authProviderID string) (*CreateInviteLinkResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support inviting users with InviteLinks")
}

// CreateIdentity implements the AuthProvider interface, but we don't support this functionatlity with Auth0 at the time.
func (a *Auth0Connector) CreateIdentity(string) (*CreateIdentityResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support creating identities")
}
