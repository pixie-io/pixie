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
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const (
	googleIdentityProvider = "google-oauth2"
)

func init() {
	pflag.String("google_oauth_userinfo_url", "https://www.googleapis.com/oauth2/v2/userinfo", "Google OAuth2 URL for userinfo.")
}

// retrieveGoogleHostedDomain returns a DomainName for a Google OAuth access token.
// This is set for users that are part of the Google Cloud org domain.
// It should be blank for GMail users.
func retrieveGoogleHostedDomain(accessToken string) (string, error) {
	client := &http.Client{}

	req, err := http.NewRequest("GET", viper.GetString("google_oauth_userinfo_url"), nil)
	if err != nil {
		return "", err
	}
	req.Header.Set("Authorization", fmt.Sprintf("Bearer %s", accessToken))
	req.Header.Set("Content-Type", "application/json")

	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	if resp.StatusCode != http.StatusOK {
		return "", errors.New("bad response from google")
	}

	body, err := io.ReadAll(resp.Body)
	defer resp.Body.Close()

	if err != nil {
		return "", err
	}

	var userInfo struct {
		// HostedDomain is the hosted G Suite domain of the user. Provided only if the user belongs to a hosted domain.
		// see https://developers.google.com/identity/protocols/oauth2/openid-connect#an-id-tokens-payload for more info.
		HostedDomain string `json:"hd,omitempty"`
	}
	if err = json.Unmarshal(body, &userInfo); err != nil {
		return "", err
	}
	return userInfo.HostedDomain, nil
}
