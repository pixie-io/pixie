package controllers

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"net/http"

	log "github.com/sirupsen/logrus"
	pflag "github.com/spf13/pflag"
	"github.com/spf13/viper"
)

func init() {
	pflag.String("auth0_host", "https://pixie-labs.auth0.com", "The auth0 hostname")
	pflag.String("auth0_client_id", "", "Auth0 client ID")
	pflag.String("auth0_client_secret", "", "Auth0 client secret")
}

// UserMetadata is a part of the Auth0 response.
type UserMetadata struct {
	PLUserID string `json:"pl_user_id,omitempty"`
	PLOrgID  string `json:"pl_org_id,omitempty"`
}

// UserInfo tracks the returned auth0 info.
type UserInfo struct {
	Email       string                   `json:",omitempty"`
	FirstName   string                   `json:"given_name,omitempty"`
	LastName    string                   `json:"family_name,omitempty"`
	UserID      string                   `json:"user_id,omitempty"`
	Name        string                   `json:",omitempty"`
	Picture     string                   `json:",omitempty"`
	Sub         string                   `json:"sub,omitempty"`
	AppMetadata map[string]*UserMetadata `json:"app_metadata,omitempty"`
}

// Auth0Connector defines an interface we use to access information from Auth0.
type Auth0Connector interface {
	Init() error
	GetUserIDFromToken(token string) (string, error)
	GetUserInfo(userID string) (*UserInfo, error)
	SetPLMetadata(userID, plOrgID, plUserID string) error
	GetClientID() string
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

type auth0ConnectorImpl struct {
	cfg             Auth0Config
	managementToken string
}

// NewAuth0Connector provides an implementation of an Auth0Connector.
func NewAuth0Connector(cfg Auth0Config) Auth0Connector {
	return &auth0ConnectorImpl{cfg: cfg}
}

func (a *auth0ConnectorImpl) Init() error {
	if a.cfg.Auth0ClientID == "" {
		return errors.New("auth0 Client ID missing")
	}

	if a.cfg.Auth0ClientSecret == "" {
		return errors.New("auth0 Client secret missing")
	}

	return nil
}

func (a *auth0ConnectorImpl) GetUserIDFromToken(token string) (string, error) {
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

func (a *auth0ConnectorImpl) getManagementToken() (string, error) {
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
	err = json.Unmarshal(body, &tokenError)
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

func (a *auth0ConnectorImpl) GetUserInfo(userID string) (*UserInfo, error) {
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

	userInfo := &UserInfo{}
	if err = json.Unmarshal(body, userInfo); err != nil {
		return nil, err
	}
	return userInfo, nil
}

func (a *auth0ConnectorImpl) GetClientID() string {
	return a.cfg.Auth0ClientID
}

func (a *auth0ConnectorImpl) SetPLMetadata(userID, plOrgID, plUserID string) error {
	appMetadata := make(map[string]*UserMetadata)
	appMetadata[a.cfg.Auth0ClientID] = &UserMetadata{
		PLUserID: plUserID,
		PLOrgID:  plOrgID,
	}

	userInfo := &UserInfo{
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
