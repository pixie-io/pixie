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

package idprovider

import (
	"context"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/url"
	"strings"

	"github.com/go-openapi/runtime"
	httptransport "github.com/go-openapi/runtime/client"
	"github.com/go-openapi/strfmt"
	"github.com/gorilla/sessions"
	hydra "github.com/ory/hydra-client-go/client"
	hydraAdmin "github.com/ory/hydra-client-go/client/admin"
	hydraModels "github.com/ory/hydra-client-go/models"
	kratos "github.com/ory/kratos-client-go/client"
	kratosAdmin "github.com/ory/kratos-client-go/client/admin"
	kratosPublic "github.com/ory/kratos-client-go/client/public"
	kratosModels "github.com/ory/kratos-client-go/models"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	"px.dev/pixie/src/shared/services/handler"
)

func init() {
	pflag.String("hydra_public_host", "https://hydra.plc-dev.svc.cluster.local:4444", "The URL to access hydra public endpoint internally.")
	pflag.String("hydra_admin_host", "https://hydra.plc-dev.svc.cluster.local:4445", "The URL to access hydra admin endpoint internally.")
	pflag.String("hydra_browser_url", "https://work.dev.withpixie.dev/oauth/hydra", "The Hydra URL as available from the browser.")

	pflag.String("hydra_consent_path", "/oauth/auth/hydra/consent", "The path that hydra will send consent to.")
	pflag.String("hydra_client_id", "auth-code-client", "Hydra OAuth2 client ID that is the only allowed client to use this flow.")

	pflag.String("kratos_public_host", "http://kratos:4433", "The URL to access kratos internally.")
	pflag.String("kratos_admin_host", "http://kratos:4434", "The URL to access kratos admin internally.")
	pflag.String("kratos_browser_url", "https://work.dev.withpixie.dev/oauth/kratos", "The URL for kratos available from the browser.")
	pflag.String("kratos_recovery_link_lifetime", "12h", "How long a kratos invite/recovery link should remain valid")
	pflag.String("kratos_schema_id", "default", "The ID of the Kratos Schema we want to use.")
}

// IDProviderSessionKey is the key for the cookie session storing idp data.
const IDProviderSessionKey string = "ossidprovider"

// HydraKratosConfig is the configuration for the IDProvider using Kratos and Hydra.
type HydraKratosConfig struct {
	// Path to the Hydra Admin endpoint.
	HydraAdminHost string
	// Path to the Hydra Public endpoint.
	HydraPublicHost string
	// The browser-accessible URL for the Hydra instance. Used as part of the redirect authorization flows.
	HydraBrowserURL string
	// The browser-accessible URL for the Kratos instance. Used as part of the redirect login flows.
	KratosBrowserURL string
	// Path to the Kratos Public endpoint.
	KratosAdminHost string
	// Path to the Kratos Public endpoint.
	KratosPublicHost string
	// The path that Hydra redirects to when asking for consent.
	HydraConsentPath string
	// The OAuth client ID used to manage authorization with Hydra.
	HydraClientID string
	// Optional argument. If not set, will be created later on.
	HTTPClient *http.Client
}

// HydraLoginStateKey is the hydra login state key.
const HydraLoginStateKey string = "hydra_login_state"

type hydraAdminClientService interface {
	AcceptConsentRequest(params *hydraAdmin.AcceptConsentRequestParams) (*hydraAdmin.AcceptConsentRequestOK, error)
	AcceptLoginRequest(params *hydraAdmin.AcceptLoginRequestParams) (*hydraAdmin.AcceptLoginRequestOK, error)
	AcceptLogoutRequest(params *hydraAdmin.AcceptLogoutRequestParams) (*hydraAdmin.AcceptLogoutRequestOK, error)
	GetConsentRequest(params *hydraAdmin.GetConsentRequestParams) (*hydraAdmin.GetConsentRequestOK, error)
	GetLoginRequest(params *hydraAdmin.GetLoginRequestParams) (*hydraAdmin.GetLoginRequestOK, error)
	GetLogoutRequest(params *hydraAdmin.GetLogoutRequestParams) (*hydraAdmin.GetLogoutRequestOK, error)
	IntrospectOAuth2Token(params *hydraAdmin.IntrospectOAuth2TokenParams) (*hydraAdmin.IntrospectOAuth2TokenOK, error)
}
type kratosPublicClientService interface {
	CompleteSelfServiceBrowserSettingsOIDCSettingsFlow(params *kratosPublic.CompleteSelfServiceBrowserSettingsOIDCSettingsFlowParams) error
	CompleteSelfServiceLoginFlowWithPasswordMethod(params *kratosPublic.CompleteSelfServiceLoginFlowWithPasswordMethodParams) (*kratosPublic.CompleteSelfServiceLoginFlowWithPasswordMethodOK, error)
	CompleteSelfServiceRegistrationFlowWithPasswordMethod(params *kratosPublic.CompleteSelfServiceRegistrationFlowWithPasswordMethodParams) (*kratosPublic.CompleteSelfServiceRegistrationFlowWithPasswordMethodOK, error)

	GetSelfServiceLoginFlow(params *kratosPublic.GetSelfServiceLoginFlowParams) (*kratosPublic.GetSelfServiceLoginFlowOK, error)
	GetSelfServiceRecoveryFlow(params *kratosPublic.GetSelfServiceRecoveryFlowParams) (*kratosPublic.GetSelfServiceRecoveryFlowOK, error)
	GetSelfServiceRegistrationFlow(params *kratosPublic.GetSelfServiceRegistrationFlowParams) (*kratosPublic.GetSelfServiceRegistrationFlowOK, error)
	GetSelfServiceSettingsFlow(params *kratosPublic.GetSelfServiceSettingsFlowParams, authInfo runtime.ClientAuthInfoWriter) (*kratosPublic.GetSelfServiceSettingsFlowOK, error)

	InitializeSelfServiceBrowserLogoutFlow(params *kratosPublic.InitializeSelfServiceBrowserLogoutFlowParams) error

	InitializeSelfServiceLoginViaBrowserFlow(params *kratosPublic.InitializeSelfServiceLoginViaBrowserFlowParams) error
	InitializeSelfServiceRecoveryViaBrowserFlow(params *kratosPublic.InitializeSelfServiceRecoveryViaBrowserFlowParams) error
	InitializeSelfServiceRegistrationViaBrowserFlow(params *kratosPublic.InitializeSelfServiceRegistrationViaBrowserFlowParams) error
	Whoami(params *kratosPublic.WhoamiParams, authInfo runtime.ClientAuthInfoWriter) (*kratosPublic.WhoamiOK, error)
}
type kratosAdminClientService interface {
	GetIdentity(params *kratosAdmin.GetIdentityParams) (*kratosAdmin.GetIdentityOK, error)
	UpdateIdentity(params *kratosAdmin.UpdateIdentityParams) (*kratosAdmin.UpdateIdentityOK, error)
	CreateIdentity(params *kratosAdmin.CreateIdentityParams) (*kratosAdmin.CreateIdentityCreated, error)
	CreateRecoveryLink(params *kratosAdmin.CreateRecoveryLinkParams) (*kratosAdmin.CreateRecoveryLinkOK, error)
}

// HydraKratosClient implements the Client interface for the a Hydra and Kratos integration.
type HydraKratosClient struct {
	Config             *HydraKratosConfig
	httpClient         *http.Client
	hydraAdminClient   hydraAdminClientService
	kratosPublicClient kratosPublicClientService
	kratosAdminClient  kratosAdminClientService
}

func loadRootCA() (*x509.CertPool, error) {
	tlsCACert := viper.GetString("tls_ca_cert")
	certPool := x509.NewCertPool()
	ca, err := ioutil.ReadFile(tlsCACert)
	if err != nil {
		return nil, fmt.Errorf("failed to read CA cert: %s", err.Error())
	}

	// Append the client certificates from the CA.
	if ok := certPool.AppendCertsFromPEM(ca); !ok {
		return nil, fmt.Errorf("failed to append CA cert")
	}
	return certPool, nil
}

func createHTTPClient() (*http.Client, error) {
	rootCA, err := loadRootCA()
	if err != nil {
		return nil, err
	}

	client := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: &tls.Config{
				RootCAs: rootCA,
			},
		},
	}
	return client, nil
}

func createRuntime(path string, client *http.Client) (*httptransport.Runtime, error) {
	u, err := url.Parse(path)
	if err != nil {
		return nil, err
	}
	return httptransport.NewWithClient(
		u.Host,
		u.Path,
		[]string{u.Scheme},
		client,
	), nil
}

// NewHydraKratosClientFromConfig creates a new client from a config.
func NewHydraKratosClientFromConfig(cfg *HydraKratosConfig) (*HydraKratosClient, error) {
	var err error
	httpClient := cfg.HTTPClient
	if httpClient == nil {
		httpClient, err = createHTTPClient()
		if err != nil {
			return nil, err
		}
	}

	hydraAdminRuntime, err := createRuntime(cfg.HydraAdminHost, httpClient)
	if err != nil {
		return nil, err
	}
	// We specify the Admin client to avoid confusing bugs because the Public client is held behind a different endpoint.
	hydraAdminClient := hydra.New(hydraAdminRuntime, strfmt.NewFormats()).Admin

	kratosPublicRuntime, err := createRuntime(cfg.KratosPublicHost, httpClient)
	if err != nil {
		return nil, err
	}
	// We specify the Public client to avoid confusing bugs because the Admin client is held behind a different endpoint.
	kratosPublicClient := kratos.New(kratosPublicRuntime, strfmt.NewFormats()).Public

	kratosAdminRuntime, err := createRuntime(cfg.KratosAdminHost, httpClient)
	if err != nil {
		return nil, err
	}
	// We specify the Admin client to avoid confusing bugs because the Admin client is held behind a different endpoint.
	kratosAdminClient := kratos.New(kratosAdminRuntime, strfmt.NewFormats()).Admin

	return &HydraKratosClient{
		Config:             cfg,
		httpClient:         httpClient,
		hydraAdminClient:   hydraAdminClient,
		kratosPublicClient: kratosPublicClient,
		kratosAdminClient:  kratosAdminClient,
	}, nil
}

// NewHydraKratosClient creates a new client with the default config.
func NewHydraKratosClient() (*HydraKratosClient, error) {
	return NewHydraKratosClientFromConfig(
		&HydraKratosConfig{
			HydraPublicHost:  viper.GetString("hydra_public_host"),
			HydraAdminHost:   viper.GetString("hydra_admin_host"),
			HydraBrowserURL:  viper.GetString("hydra_browser_url"),
			KratosPublicHost: viper.GetString("kratos_public_host"),
			KratosAdminHost:  viper.GetString("kratos_admin_host"),
			KratosBrowserURL: viper.GetString("kratos_browser_url"),
			HydraConsentPath: viper.GetString("hydra_consent_path"),
			HydraClientID:    viper.GetString("hydra_client_id"),
		},
	)
}

func (c *HydraKratosClient) convertExternalHydraURLToInternal(externalHydraURL string) (string, error) {
	// We need to take the redirect URL which is at the URL for the Hydra Browser and instead
	// go to the internal Hydra URL so that we can call it internally.
	u, err := url.Parse(externalHydraURL)
	if err != nil {
		log.Debug("fail on redirect pars")
		return "", err
	}

	browserURL, err := url.Parse(c.Config.HydraBrowserURL)
	if err != nil {
		log.Debug("fail on public pars")
		return "", err
	}
	publicURL, err := url.Parse(c.Config.HydraPublicHost)
	if err != nil {
		log.Debug("fail on public pars")
		return "", err
	}
	// Set the scheme and host to the public URL.
	u.Scheme = publicURL.Scheme
	u.Host = publicURL.Host
	// Get rid of any prepended path.
	u.Path = strings.ReplaceAll(u.Path, browserURL.Path, "")
	return u.String(), nil
}

// deepCopyHeader creates a deep copy of the src into dst.
func deepCopyHeader(src, dst http.Header) {
	for k, vv := range src {
		for _, v := range vv {
			dst.Add(k, v)
		}
	}
}

func generateLoginChallenge(c int) (string, error) {
	b := make([]byte, c)
	_, err := rand.Read(b)
	if err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}

// interceptRedirect will GET request an endpoint but not follow any redirect requests and instead
// will return the response.
func (c *HydraKratosClient) interceptRedirect(u string, header http.Header) (*http.Response, error) {
	req, err := http.NewRequest("GET", u, nil)
	if err != nil {
		log.Debug("fail on req create")
		return nil, err
	}

	// Copy over the headr
	req.Header = make(http.Header)
	deepCopyHeader(header, req.Header)
	return c.httpClient.Transport.RoundTrip(req)
}

// kratosLoginURL returns the url for browser login.
func (c *HydraKratosClient) kratosLoginURL(returnToURL string) (string, error) {
	kratosURL, err := url.Parse(c.Config.KratosBrowserURL + "/self-service/login/browser")
	if err != nil {
		return "", err
	}

	kratosQ := url.Values{}

	kratosQ.Set("return_to", returnToURL)
	kratosQ.Set("refresh", "true")

	kratosURL.RawQuery = kratosQ.Encode()
	return kratosURL.String(), nil
}

// Whoami contains information about a user.
type Whoami struct {
	kratosSession *kratosModels.Session
}

// ID returns the ID of the whoami.
func (w *Whoami) ID() string {
	return strfmt.UUID4(w.kratosSession.Identity.ID).String()
}

// Whoami implements the Kratos whoami flow.
func (c *HydraKratosClient) Whoami(ctx context.Context, r *http.Request) (*Whoami, error) {
	cookie := r.Header.Get("Cookie")
	if cookie == "" {
		return nil, fmt.Errorf("Request cookie is empty")
	}
	params := &kratosPublic.WhoamiParams{
		Context: ctx,
		Cookie:  &cookie,
	}
	// Auth is nil because we pass auth through the cookie.
	resp, err := c.kratosPublicClient.Whoami(params, nil)
	if err != nil {
		return nil, err
	}
	return &Whoami{kratosSession: resp.GetPayload()}, nil
}

// PasswordRegistrationFlow describes what information is necessary to register using a password.
type PasswordRegistrationFlow struct {
	flow *kratosModels.RegistrationFlowMethod
}

// PasswordLoginFlow describes what informasetion is necessary to login with a password.
type PasswordLoginFlow struct {
	flow *kratosModels.LoginFlowMethod
}

// GetPasswordRegistrationFlow returns the registration flow for the oss auth. Used to render the form to register a user.
func (c *HydraKratosClient) GetPasswordRegistrationFlow(ctx context.Context, flowID string) (*PasswordRegistrationFlow, error) {
	params := &kratosPublic.GetSelfServiceRegistrationFlowParams{
		ID:      flowID,
		Context: ctx,
	}
	resp, err := c.kratosPublicClient.GetSelfServiceRegistrationFlow(params)
	if err != nil {
		return nil, err
	}

	if resp.GetPayload() == nil {
		return nil, errors.New("no message received from kratos self-service registration flow endpoint")
	}

	flow, ok := resp.GetPayload().Methods["password"]
	if !ok {
		return nil, errors.New("Does not have password method")
	}
	return &PasswordRegistrationFlow{&flow}, nil
}

// GetPasswordLoginFlow returns the login flow for the oss auth. Used to render the form to login a user.
func (c *HydraKratosClient) GetPasswordLoginFlow(ctx context.Context, flowID string) (*PasswordLoginFlow, error) {
	params := &kratosPublic.GetSelfServiceLoginFlowParams{
		ID:      flowID,
		Context: ctx,
	}
	// Auth is nil because we pass auth through the cookie.
	resp, err := c.kratosPublicClient.GetSelfServiceLoginFlow(params)
	if err != nil {
		return nil, err
	}

	if resp.GetPayload() == nil {
		return nil, errors.New("no message received from kratos self-service login flow endpoint")
	}

	flow, ok := resp.GetPayload().Methods["password"]
	if !ok {
		return nil, errors.New("Does not have password method")
	}

	return &PasswordLoginFlow{&flow}, nil
}

// Store the hydraLoginState as a cookie.
func setHydraLoginState(hydraLoginState string, session *sessions.Session, w http.ResponseWriter, r *http.Request) error {
	session.Values[HydraLoginStateKey] = hydraLoginState
	session.Options.HttpOnly = true
	session.Options.Secure = true
	session.Options.SameSite = http.SameSiteStrictMode
	session.Options.Domain = viper.GetString("domain_name")

	return session.Save(r, w)
}

// RedirectToLogin sets up the login flow and redirects the response writer to the Kratos URL login.
func (c *HydraKratosClient) RedirectToLogin(session *sessions.Session, w http.ResponseWriter, r *http.Request) error {
	hydraLoginState, err := generateLoginChallenge(48)
	if err != nil {
		return nil
	}

	err = setHydraLoginState(hydraLoginState, session, w, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	// Add the state to the URL. When we're back in the auth login flow, we verify that the `HydraLoginStateKey` query parameter matches the cookie
	// value to prevent unauthorized users from stealing this log in flow.
	returnToURL := r.URL
	returnQ := returnToURL.Query()
	returnQ.Set(HydraLoginStateKey, hydraLoginState)
	returnToURL.RawQuery = returnQ.Encode()

	// Create the Kratos login URL.
	kratosURL, err := c.kratosLoginURL(returnToURL.String())
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	http.Redirect(w, r, kratosURL, http.StatusFound)
	return nil
}

// RedirectResponse contains information about where a URL should redirect a user.
type RedirectResponse struct {
	RedirectTo *string
}

// AcceptHydraLogin sends a request to accept the login on the hydra endpoint.
func (c *HydraKratosClient) AcceptHydraLogin(ctx context.Context, challenge string, whoamiResp *Whoami) (*RedirectResponse, error) {
	subject := whoamiResp.ID()
	params := &hydraAdmin.AcceptLoginRequestParams{
		Body: &hydraModels.AcceptLoginRequest{
			Context: whoamiResp.kratosSession,
			Subject: &subject,
		},
		LoginChallenge: challenge,
		Context:        ctx,
	}
	resp, err := c.hydraAdminClient.AcceptLoginRequest(params)
	if err != nil {
		return nil, err
	}
	return &RedirectResponse{RedirectTo: resp.GetPayload().RedirectTo}, nil
}

// InterceptHydraUserConsent performs the user consent flow bypassing normal user interaction. Hydra uses
// consent to allow users to configure consent for third-party OAuth clients. Our auth system does not allow third-party
// OAuth clients and so we can skip the consent stage.
func (c *HydraKratosClient) InterceptHydraUserConsent(hydraConsentURL string, ogHeader http.Header) (http.Header, string, error) {
	u, err := c.convertExternalHydraURLToInternal(hydraConsentURL)
	if err != nil {
		return nil, "", err
	}
	// GET the consentURL and return the response, which we expect to be a redirect.
	resp, err := c.interceptRedirect(u, ogHeader)
	if err != nil {
		return nil, "", err
	}
	// Should be a 302 redirect, otherwise we fail.
	if resp.StatusCode != http.StatusFound {
		return nil, "", fmt.Errorf("Expected a redirect 302, got an unexpected %d", resp.StatusCode)
	}

	location := resp.Header.Get("location")
	if location == "" {
		return nil, "", fmt.Errorf("Expected a location in the redirect message, received empty string")
	}

	if err != nil {
		return nil, "", err
	}

	uu, err := url.Parse(location)
	if err != nil {
		return nil, "", err
	}

	if uu.Path != c.Config.HydraConsentPath {
		return nil, "", fmt.Errorf("Received an invalid redirect location '%s' with path: '%s'", location, uu.Path)
	}
	return resp.Header, uu.Query().Get("consent_challenge"), nil
}

// AcceptConsent acepts the consent request for the particular challenge.
func (c *HydraKratosClient) AcceptConsent(ctx context.Context, challenge string) (*RedirectResponse, error) {
	if challenge == "" {
		return nil, fmt.Errorf("challenge is empty")
	}
	resp, err := c.hydraAdminClient.GetConsentRequest(&hydraAdmin.GetConsentRequestParams{
		ConsentChallenge: challenge,
		Context:          ctx,
	})
	if err != nil {
		log.Debug("error on hydra.consentRequest:")
		return nil, err
	}

	if resp.GetPayload() == nil {
		log.Debug("consent request payload is empty")
		return nil, err
	}

	consentRequest := resp.GetPayload()

	// We only trust the client that's passed in as a config here. In the future we might want to support other clients
	// at which point we will want to actually ask for permission from the user.
	if consentRequest.Client.ClientID != c.Config.HydraClientID {
		return nil, fmt.Errorf("'%s' not an allowed client", consentRequest.Client.ClientID)
	}

	acceptResp, err := c.hydraAdminClient.AcceptConsentRequest(&hydraAdmin.AcceptConsentRequestParams{
		Body: &hydraModels.AcceptConsentRequest{
			GrantScope:               consentRequest.RequestedScope,
			GrantAccessTokenAudience: consentRequest.RequestedAccessTokenAudience,
		},
		ConsentChallenge: challenge,
		Context:          ctx,
	})

	if err != nil {
		log.Debug("error on hydra.AcceptConsentRequest:")
		return nil, err
	}
	return &RedirectResponse{RedirectTo: acceptResp.GetPayload().RedirectTo}, nil
}

// HandleLogin handles the login for Hydra and Kratos.
func (c *HydraKratosClient) HandleLogin(session *sessions.Session, w http.ResponseWriter, r *http.Request) error {
	query := r.URL.Query()
	// Hydra must set the login_challenge parameter as part of it's redirect scheme, otherwise
	// we won't be able to finish the login scheme.
	challenge := query.Get("login_challenge")
	if challenge == "" {
		return handler.NewStatusError(http.StatusInternalServerError, "no login challenge on call")
	}

	// URL should have HydraLoginStateKey that is mirrored in a cookie.
	// HydraLoginStateKey is necessary for csrf prevention. If it's not set in the query, we have to redirect
	// to the login flow where it'll be set.
	loginState := query.Get(HydraLoginStateKey)
	if loginState == "" {
		w.Header().Set("redirectReason", "login_state_not_found")
		return c.RedirectToLogin(session, w, r)
	}

	// The same login flow that sets the HydraLoginStateKey must set the state key. If that's not set then
	// we have an error.
	state, ok := session.Values[HydraLoginStateKey]
	if !ok {
		return handler.NewStatusError(http.StatusInternalServerError, fmt.Sprintf("Could not find hydra login state in cookie store: %v", session.Values))
	}

	// If the query parameter and the cookie value don't match, we set a new state and redirect the user to login.
	if state != loginState {
		log.Tracef("mismatching states %s %s", fmt.Sprintf("%s", state)[:5], loginState[:5])
		w.Header().Set("redirectReason", "mismatch_states")
		return c.RedirectToLogin(session, w, r)
	}

	ctx := context.Background()
	whoami, err := c.Whoami(ctx, r)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	redirectResp, err := c.AcceptHydraLogin(ctx, challenge, whoami)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	if redirectResp.RedirectTo == nil {
		return handler.NewStatusError(http.StatusInternalServerError, "No redirect URL set from OAuth Login accept")
	}

	// We expect the response to redirect to the consent endpoint. We will just intercept the consent endpoint
	respHeader, consentChallenge, err := c.InterceptHydraUserConsent(*redirectResp.RedirectTo, r.Header)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	consentResp, err := c.AcceptConsent(ctx, consentChallenge)
	if err != nil {
		return &handler.StatusError{Code: http.StatusInternalServerError, Err: err}
	}

	// Copy the header because the header contains a necessary Set-Cookie from the OAuth server.
	deepCopyHeader(respHeader, w.Header())
	http.Redirect(w, r, *consentResp.RedirectTo, http.StatusFound)
	return nil
}

// SessionKey returns the string key under which cookie the session info should be stored.
func (c *HydraKratosClient) SessionKey() string {
	return IDProviderSessionKey
}

// GetUserIDFromToken returns the userID from the subject portion of the access token.
func (c *HydraKratosClient) GetUserIDFromToken(ctx context.Context, token string) (string, error) {
	params := &hydraAdmin.IntrospectOAuth2TokenParams{
		Context: ctx,
		Token:   token,
	}
	res, err := c.hydraAdminClient.IntrospectOAuth2Token(params)
	if err != nil {
		return "", err
	}

	return res.GetPayload().Sub, nil
}

// KratosUserInfo contains the user information format as stored in Kratos.
type KratosUserInfo struct {
	Email    string `json:"email,omitempty"`
	PLOrgID  string `json:"plOrgID,omitempty"`
	PLUserID string `json:"plUserID,omitempty"`
	// KratosID is the ID assigned to the user by Kratos.
	KratosID string `json:"-"`
}

// GetUserInfo returns the UserInfo for the userID.
func (c *HydraKratosClient) GetUserInfo(ctx context.Context, userID string) (*KratosUserInfo, error) {
	params := &kratosAdmin.GetIdentityParams{
		ID:      userID,
		Context: ctx,
	}
	resp, err := c.kratosAdminClient.GetIdentity(params)
	if err != nil {
		return nil, err
	}

	return convertIdentityToKratosUserInfo(resp.Payload)
}

// Converts the Kratos internal representation to the KratosUserInfo representation.
func convertIdentityToKratosUserInfo(identity *kratosModels.Identity) (*KratosUserInfo, error) {
	// Trick to convert the traits map into a struct.
	s, err := json.Marshal(identity.Traits)
	if err != nil {
		return nil, err
	}

	k := &KratosUserInfo{}
	err = json.Unmarshal(s, k)
	if err != nil {
		return nil, err
	}
	k.KratosID = strfmt.UUID4(identity.ID).String()

	return k, nil
}

// UpdateUserInfo sets the userInfo for the user. Note that it doesn't patch, but fully updates so you likely need to GetUserInfo first.
func (c *HydraKratosClient) UpdateUserInfo(ctx context.Context, userID string, kratosInfo *KratosUserInfo) (*KratosUserInfo, error) {
	params := &kratosAdmin.UpdateIdentityParams{
		ID: userID,
		Body: &kratosModels.UpdateIdentity{
			Traits: kratosInfo,
		},
		Context: ctx,
	}
	resp, err := c.kratosAdminClient.UpdateIdentity(params)
	if err != nil {
		return nil, err
	}

	return convertIdentityToKratosUserInfo(resp.Payload)
}

// CreateInviteLink implements the idmanager.Manager interface function to create an account and return an InviteLink for the specified user in the specific org.
func (c *HydraKratosClient) CreateInviteLink(ctx context.Context, req *idmanager.CreateInviteLinkRequest) (*idmanager.CreateInviteLinkResponse, error) {
	ident, err := c.CreateIdentity(ctx, req.Email)
	if err != nil {
		return nil, err
	}

	_, err = c.UpdateUserInfo(ctx, ident.AuthProviderID, &KratosUserInfo{
		Email:    req.Email,
		PLOrgID:  req.PLOrgID,
		PLUserID: req.PLUserID,
	})

	if err != nil {
		return nil, err
	}

	invite, err := c.CreateInviteLinkForIdentity(ctx, &idmanager.CreateInviteLinkForIdentityRequest{
		AuthProviderID: ident.AuthProviderID,
	})

	if err != nil {
		return nil, err
	}
	return &idmanager.CreateInviteLinkResponse{
		Email:      req.Email,
		InviteLink: invite.InviteLink,
	}, nil
}

// CreateIdentity creates an identity for the comparable email.
func (c *HydraKratosClient) CreateIdentity(ctx context.Context, email string) (*idmanager.CreateIdentityResponse, error) {
	schemaID := viper.GetString("kratos_schema_id")
	idResp, err := c.kratosAdminClient.CreateIdentity(&kratosAdmin.CreateIdentityParams{
		Context: ctx,
		Body: &kratosModels.CreateIdentity{
			SchemaID: &schemaID,
			Traits: &KratosUserInfo{
				Email: email,
			},
		},
	})

	if err != nil {
		return nil, err
	}
	return &idmanager.CreateIdentityResponse{
		AuthProviderID:   strfmt.UUID4(idResp.Payload.ID).String(),
		IdentityProvider: "kratos",
	}, nil
}

// CreateInviteLinkForIdentity creates a Kratos recovery link for the identity, which can act like a one-time use invitelink.
func (c *HydraKratosClient) CreateInviteLinkForIdentity(ctx context.Context, req *idmanager.CreateInviteLinkForIdentityRequest) (*idmanager.CreateInviteLinkForIdentityResponse, error) {
	var identityID strfmt.UUID4
	if err := identityID.UnmarshalText([]byte(req.AuthProviderID)); err != nil {
		return nil, err
	}
	recovery, err := c.kratosAdminClient.CreateRecoveryLink(&kratosAdmin.CreateRecoveryLinkParams{
		Context: ctx,
		Body: &kratosModels.CreateRecoveryLink{
			ExpiresIn:  viper.GetString("kratos_recovery_link_lifetime"),
			IdentityID: kratosModels.UUID(identityID),
		},
	})

	if err != nil {
		return nil, err
	}
	return &idmanager.CreateInviteLinkForIdentityResponse{
		InviteLink: *recovery.Payload.RecoveryLink,
	}, nil
}

// SetPLMetadata will update the client with the related info.
func (c *HydraKratosClient) SetPLMetadata(userID, plOrgID, plUserID string) error {
	// TODO(philkuz,PC-1073) get rid of this in favor of not duplicating hydra_kratos_auth.go.
	kratosInfo, err := c.GetUserInfo(context.Background(), userID)
	if err != nil {
		return err
	}
	kratosInfo.PLOrgID = plOrgID
	kratosInfo.PLUserID = plUserID
	_, err = c.UpdateUserInfo(context.Background(), userID, kratosInfo)
	return err
}
