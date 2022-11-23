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
	"fmt"
	"net/http"
	"net/url"
	"os"
	"strings"

	httptransport "github.com/go-openapi/runtime/client"
	"github.com/go-openapi/strfmt"
	"github.com/gorilla/sessions"
	hydra "github.com/ory/hydra-client-go/client"
	hydraAdmin "github.com/ory/hydra-client-go/client/admin"
	hydraModels "github.com/ory/hydra-client-go/models"
	kratos "github.com/ory/kratos-client-go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/services/handler"
)

func init() {
	pflag.String("hydra_public_host", "https://hydra.plc-dev.svc.cluster.local:4444", "The URL to access hydra public endpoint internally.")
	pflag.String("hydra_admin_host", "https://hydra.plc-dev.svc.cluster.local:4445", "The URL to access hydra admin endpoint internally.")
	pflag.String("hydra_browser_url", "https://work.dev.withpixie.dev/oauth/hydra", "The Hydra URL as available from the browser.")

	pflag.String("hydra_consent_path", "/oauth/auth/hydra/consent", "The path that hydra will send consent to.")
	pflag.String("hydra_client_id", "auth-code-client", "Hydra OAuth2 client ID that is the only allowed client to use this flow.")

	pflag.String("kratos_public_host", "https://kratos:4433", "The URL to access kratos internally.")
	pflag.String("kratos_admin_host", "https://kratos:4434", "The URL to access kratos admin internally.")
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
	GetConsentRequest(params *hydraAdmin.GetConsentRequestParams) (*hydraAdmin.GetConsentRequestOK, error)
	IntrospectOAuth2Token(params *hydraAdmin.IntrospectOAuth2TokenParams) (*hydraAdmin.IntrospectOAuth2TokenOK, error)
}

type kratosPublicClientService interface {
	ToSession(context.Context) kratos.V0alpha2ApiApiToSessionRequest
}

type kratosAdminClientService interface {
	AdminGetIdentity(context.Context, string) kratos.V0alpha2ApiApiAdminGetIdentityRequest
	AdminCreateIdentity(context.Context) kratos.V0alpha2ApiApiAdminCreateIdentityRequest
	AdminCreateSelfServiceRecoveryLink(context.Context) kratos.V0alpha2ApiApiAdminCreateSelfServiceRecoveryLinkRequest
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
	ca, err := os.ReadFile(tlsCACert)
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

	tr := http.DefaultTransport.(*http.Transport).Clone()
	tr.TLSClientConfig = &tls.Config{RootCAs: rootCA}

	client := &http.Client{Transport: tr}
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

func createKratosClient(host string, client *http.Client) (*kratos.APIClient, error) {
	u, err := url.Parse(host)
	if err != nil {
		return nil, err
	}

	conf := kratos.NewConfiguration()
	conf.Host = u.Host
	conf.Scheme = u.Scheme
	conf.Servers = kratos.ServerConfigurations{{URL: host}}
	conf.HTTPClient = client
	return kratos.NewAPIClient(conf), nil
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

	// One can theoretically send public requests to the Admin Host but then kratos will
	// 302 the requests to the public host/port.
	// For OSS cloud, the kratos public host might not be accessible from within the cluster
	// so we use an explicit public client that points to the cluster internal kratos public
	// addr.
	kratosPublicClient, err := createKratosClient(cfg.KratosPublicHost, httpClient)
	if err != nil {
		return nil, err
	}

	kratosAdminClient, err := createKratosClient(cfg.KratosAdminHost, httpClient)
	if err != nil {
		return nil, err
	}

	return &HydraKratosClient{
		Config:             cfg,
		httpClient:         httpClient,
		hydraAdminClient:   hydraAdminClient,
		kratosAdminClient:  kratosAdminClient.V0alpha2Api,
		kratosPublicClient: kratosPublicClient.V0alpha2Api,
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
	kratosSession *kratos.Session
}

// ID returns the ID of the whoami.
func (w *Whoami) ID() string {
	return w.kratosSession.Identity.GetId()
}

// Whoami implements the Kratos whoami flow.
func (c *HydraKratosClient) Whoami(ctx context.Context, r *http.Request) (*Whoami, error) {
	cookie := r.Header.Get("Cookie")
	if cookie == "" {
		return nil, fmt.Errorf("Request cookie is empty")
	}
	session, _, err := c.kratosPublicClient.ToSession(ctx).Cookie(cookie).Execute()
	if err != nil {
		return nil, err
	}
	return &Whoami{kratosSession: session}, nil
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
	for k, vv := range respHeader {
		// We only want to cookie and Location headers, otherwise Firefox and Safari complain.
		if !(k == "Set-Cookie" || k == "Location") {
			continue
		}
		for _, v := range vv {
			w.Header().Add(k, v)
		}
	}
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
	Email string `json:"email,omitempty"`
	// KratosID is the ID assigned to the user by Kratos.
	KratosID string `json:"-"`
}

// GetUserInfo returns the UserInfo for the userID.
func (c *HydraKratosClient) GetUserInfo(ctx context.Context, userID string) (*KratosUserInfo, error) {
	id, _, err := c.kratosAdminClient.AdminGetIdentity(ctx, userID).Execute()
	if err != nil {
		return nil, err
	}

	return convertIdentityToKratosUserInfo(id)
}

// Converts the Kratos internal representation to the KratosUserInfo representation.
func convertIdentityToKratosUserInfo(identity *kratos.Identity) (*KratosUserInfo, error) {
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
	k.KratosID = identity.GetId()

	return k, nil
}

// CreateIdentityResponse contains relevant information about the Identity that was created.
type CreateIdentityResponse struct {
	IdentityProvider string
	AuthProviderID   string
}

// CreateIdentity creates an identity for the comparable email.
func (c *HydraKratosClient) CreateIdentity(ctx context.Context, email string) (*CreateIdentityResponse, error) {
	schemaID := viper.GetString("kratos_schema_id")

	body := kratos.NewAdminCreateIdentityBody(schemaID, map[string]interface{}{"email": email})
	idResp, _, err := c.kratosAdminClient.AdminCreateIdentity(ctx).AdminCreateIdentityBody(*body).Execute()
	if err != nil {
		return nil, err
	}

	return &CreateIdentityResponse{
		AuthProviderID:   idResp.GetId(),
		IdentityProvider: "kratos",
	}, nil
}

// CreateInviteLinkForIdentityRequest is the request value for the invite link method.
type CreateInviteLinkForIdentityRequest struct {
	AuthProviderID string
}

// CreateInviteLinkForIdentityResponse contains the response for the invite link method.
type CreateInviteLinkForIdentityResponse struct {
	InviteLink string
}

// CreateInviteLinkForIdentity creates a Kratos recovery link for the identity, which can act like a one-time use invitelink.
func (c *HydraKratosClient) CreateInviteLinkForIdentity(ctx context.Context, req *CreateInviteLinkForIdentityRequest) (*CreateInviteLinkForIdentityResponse, error) {
	body := kratos.NewAdminCreateSelfServiceRecoveryLinkBody(req.AuthProviderID)
	body.SetExpiresIn(viper.GetString("kratos_recovery_link_lifetime"))
	recovery, _, err := c.kratosAdminClient.AdminCreateSelfServiceRecoveryLink(ctx).AdminCreateSelfServiceRecoveryLinkBody(*body).Execute()
	if err != nil {
		return nil, err
	}
	return &CreateInviteLinkForIdentityResponse{
		InviteLink: recovery.GetRecoveryLink(),
	}, nil
}
