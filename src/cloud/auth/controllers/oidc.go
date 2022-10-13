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
	"net/url"
	"strings"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"
)

const (
	oidcIdentityProvider = "oidc"
)

func init() {
	pflag.String("oidc_host", "", "The OIDC issuer URL")

	pflag.String("oidc_client_id", "", "OIDC client ID")
	pflag.String("oidc_client_secret", "", "OIDC client secret")

	// Defaults to <oidc_host>/.well-known/openid-configuration
	pflag.String("oidc_metadata_url", "", "OIDC discovery endpoint URL")

	// The following two params are fetched from the discovery endpoint automatically
	// but may be overridden as need be.
	pflag.String("oidc_token_endpoint", "", "OIDC token endpoint URL")
	pflag.String("oidc_userinfo_endpoint", "", "OIDC UserInfo endpoint URL")

	// The following three flags are to be used in conjunction. They control the fetching of the HostedDomain
	// param from Google Auth in case of Google backed accounts and then use that to group users into automated orgs.

	// A OIDC provider might be proxying multiple underlying identity providers.
	// If one needs to distinguish between these underlying providers, this flag must be set.
	pflag.String("oidc_idprovider_claim", "", "If set, this custom claim will be used as the ID Provider value")
	// This flag is the expected value of the idprovider claim to indicate that this is a google connection.
	pflag.String("oidc_google_idprovider_value", "", "The expected value for ID Provider that indicates this is a Google account")
	// This flag is the claim that includes the google auth token that can be used to retrieve the hosted domain value.
	pflag.String("oidc_google_access_token_claim", "", "The custom claim that includes the Google Access token")
}

// Some OIDC providers do not properly encode booleans in the JSON representation.
// So unfortunately this is a workaround to handle both bools and string representations of bools
// in the userinfo.
type boolLike bool

func (sb *boolLike) UnmarshalJSON(b []byte) error {
	switch strings.ToLower(string(b)) {
	case "true", `"true"`:
		*sb = true
		return nil
	case "false", `"false"`:
		*sb = false
		return nil
	default:
		return errors.New("invalid bool")
	}
}

func (sb boolLike) MarshalJSON() ([]byte, error) {
	return json.Marshal(bool(sb))
}

// userInfo tracks the returned info.
// Follows the standard claim spec https://openid.net/specs/openid-connect-core-1_0.html#StandardClaims
type userInfo struct {
	Sub           string   `json:",omitempty"`
	Name          string   `json:",omitempty"`
	FirstName     string   `json:"given_name,omitempty"`
	LastName      string   `json:"family_name,omitempty"`
	Picture       string   `json:",omitempty"`
	Email         string   `json:",omitempty"`
	EmailVerified boolLike `json:"email_verified,omitempty"`
}

// OIDPMetadata is used to parse the provider metadata.
// See spec https://openid.net/specs/openid-connect-discovery-1_0.html#ProviderMetadata
type OIDPMetadata struct {
	Issuer           string `json:"issuer"`
	AuthEndpoint     string `json:"authorization_endpoint"`
	TokenEndpoint    string `json:"token_endpoint,omitempty"`
	UserinfoEndpoint string `json:"userinfo_endpoint,omitempty"`
}

// OIDCConnector implements the AuthProvider interface for OIDC.
type OIDCConnector struct {
	Issuer           string
	MetadataEndpoint string

	ClientID     string
	ClientSecret string

	Metadata *OIDPMetadata

	IDProviderClaim        string
	GoogleIdentityProvider string
	GoogleAccessTokenClaim string

	client *http.Client
}

// NewOIDCConnector provides an implementation of an OIDCConnector.
func NewOIDCConnector() (*OIDCConnector, error) {
	issuer := viper.GetString("oidc_host")
	if issuer == "" {
		return nil, errors.New("OIDC issuer missing")
	}

	clientID := viper.GetString("oidc_client_id")
	if clientID == "" {
		return nil, errors.New("OIDC Client ID missing")
	}
	clientSecret := viper.GetString("oidc_client_secret")
	if clientSecret == "" {
		return nil, errors.New("OIDC Client secret missing")
	}

	var err error
	metadataEndpoint := viper.GetString("oidc_metadata_url")
	if metadataEndpoint == "" {
		metadataEndpoint, err = url.JoinPath(issuer, ".well-known/openid-configuration")
		if err != nil {
			return nil, err
		}
	}

	idProviderClaim := viper.GetString("oidc_idprovider_claim")
	googleIDProvider := viper.GetString("oidc_google_idprovider_value")
	googleAccessTokenClaim := viper.GetString("oidc_google_access_token_claim")

	if googleAccessTokenClaim != "" && (googleIDProvider == "" || idProviderClaim == "") {
		return nil, errors.New("must set oidc_idprovider_claim and oidc_google_idprovider_value when setting oidc_google_access_token_claim")
	}

	conn := &OIDCConnector{
		Issuer:                 issuer,
		ClientID:               clientID,
		ClientSecret:           clientSecret,
		MetadataEndpoint:       metadataEndpoint,
		IDProviderClaim:        idProviderClaim,
		GoogleIdentityProvider: googleIDProvider,
		GoogleAccessTokenClaim: googleAccessTokenClaim,
		client:                 &http.Client{},
	}

	conn.tryFetchMetadata()

	tokenEndpoint := viper.GetString("oidc_token_endpoint")
	if tokenEndpoint != "" {
		conn.Metadata.TokenEndpoint = tokenEndpoint
	}
	userinfoEndpoint := viper.GetString("oidc_userinfo_endpoint")
	if userinfoEndpoint != "" {
		conn.Metadata.UserinfoEndpoint = userinfoEndpoint
	}

	if conn.Metadata.UserinfoEndpoint == "" {
		return nil, errors.New("Userinfo endpoint missing")
	}
	return conn, nil
}

func (c *OIDCConnector) tryFetchMetadata() {
	c.Metadata = &OIDPMetadata{}
	req, err := http.NewRequest("GET", c.MetadataEndpoint, nil)
	if err != nil {
		return
	}
	req.Header.Set("Content-Type", "application/json")

	resp, err := c.client.Do(req)
	if err != nil {
		return
	}

	body, err := io.ReadAll(resp.Body)
	defer resp.Body.Close()

	if err != nil {
		return
	}

	_ = json.Unmarshal(body, c.Metadata)
}

// GetUserInfoFromAccessToken returns the UserID for the particular token.
func (c *OIDCConnector) GetUserInfoFromAccessToken(accessToken string) (*UserInfo, error) {
	req, err := http.NewRequest("GET", c.Metadata.UserinfoEndpoint, nil)
	if err != nil {
		return nil, err
	}
	req.Header.Set("Authorization",
		fmt.Sprintf("Bearer %s", accessToken))
	req.Header.Set("Content-Type", "application/json")

	resp, err := c.client.Do(req)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != http.StatusOK {
		return nil, errors.New("bad response from OIDC userinfo")
	}

	body, err := io.ReadAll(resp.Body)
	defer resp.Body.Close()

	if err != nil {
		return nil, err
	}

	info := &userInfo{}
	if err = json.Unmarshal(body, info); err != nil {
		return nil, err
	}

	if info.Sub == "" {
		return nil, errors.New("invalid userinfo from OIDC")
	}

	parsed := make(map[string]interface{})
	err = json.Unmarshal(body, &parsed)
	if err != nil {
		return nil, err
	}

	userInfo := &UserInfo{
		Email:            info.Email,
		EmailVerified:    bool(info.EmailVerified),
		FirstName:        info.FirstName,
		LastName:         info.LastName,
		Name:             info.Name,
		Picture:          info.Picture,
		IdentityProvider: oidcIdentityProvider,
		AuthProviderID:   info.Sub,
	}

	err = c.retrieveHostedDomain(parsed, userInfo)
	if err != nil {
		return nil, err
	}

	return userInfo, nil
}

// Populates the HostedDomain for the identity according to the IdentityProvider.
func (c *OIDCConnector) retrieveHostedDomain(parsed map[string]interface{}, userInfo *UserInfo) error {
	// This connector doesn't care about the HD behavior.
	if c.GoogleAccessTokenClaim == "" {
		return nil
	}

	idpVal := parsed[c.IDProviderClaim]
	if idpVal == "" {
		return fmt.Errorf("no claim for identitiy provider key %s found", c.IDProviderClaim)
	}
	idp, ok := idpVal.(string)
	if !ok {
		return fmt.Errorf("claim for identitiy provider key %s is not a string", c.IDProviderClaim)
	}

	if idp != c.GoogleIdentityProvider {
		return nil
	}

	googleTokenVal := parsed[c.GoogleAccessTokenClaim]
	if googleTokenVal == "" {
		return fmt.Errorf("no claim for google access token key %s found", c.GoogleAccessTokenClaim)
	}
	googleAccessToken, ok := googleTokenVal.(string)
	if !ok {
		return fmt.Errorf("claim for google access token key %s is not a string", c.GoogleAccessTokenClaim)
	}

	hd, err := retrieveGoogleHostedDomain(googleAccessToken)
	if err != nil {
		return err
	}
	userInfo.HostedDomain = hd
	return nil
}

// CreateInviteLink implements the AuthProvider interface, but we don't support this functionatlity with OIDC at the time.
func (c *OIDCConnector) CreateInviteLink(authProviderID string) (*CreateInviteLinkResponse, error) {
	return nil, errors.New("pixie's OIDC implementation does not support inviting users with InviteLinks")
}

// CreateIdentity implements the AuthProvider interface, but we don't support this functionatlity with OIDC at the time.
func (c *OIDCConnector) CreateIdentity(string) (*CreateIdentityResponse, error) {
	return nil, errors.New("pixie's OIDC implementation does not support creating identities")
}
