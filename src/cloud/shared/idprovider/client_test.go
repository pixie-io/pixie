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
	"errors"
	"net/http"
	"net/http/httptest"
	"net/url"
	"testing"

	"github.com/go-openapi/runtime"
	"github.com/go-openapi/strfmt"
	"github.com/golang/mock/gomock"
	"github.com/gorilla/sessions"
	hydraAdmin "github.com/ory/hydra-client-go/client/admin"
	hydraModels "github.com/ory/hydra-client-go/models"
	kratosAdmin "github.com/ory/kratos-client-go/client/admin"
	kratosPublic "github.com/ory/kratos-client-go/client/public"
	kratosModels "github.com/ory/kratos-client-go/models"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	mock_idprovider "px.dev/pixie/src/cloud/shared/idprovider/mock"
)

// Implements the kratosAdminClient interface.
type fakeKratosAdminClient struct {
	updateIdentityFn *func(params *kratosAdmin.UpdateIdentityParams) (*kratosAdmin.UpdateIdentityOK, error)
	getIdentityFn    *func(params *kratosAdmin.GetIdentityParams) (*kratosAdmin.GetIdentityOK, error)
}

func (ka *fakeKratosAdminClient) GetIdentity(params *kratosAdmin.GetIdentityParams) (*kratosAdmin.GetIdentityOK, error) {
	if ka.getIdentityFn == nil {
		return nil, errors.New("not implemented")
	}

	return (*ka.getIdentityFn)(params)
}

func (ka *fakeKratosAdminClient) UpdateIdentity(params *kratosAdmin.UpdateIdentityParams) (*kratosAdmin.UpdateIdentityOK, error) {
	if ka.updateIdentityFn == nil {
		return nil, errors.New("not implemented")
	}

	return (*ka.updateIdentityFn)(params)
}

func (ka *fakeKratosAdminClient) CreateIdentity(params *kratosAdmin.CreateIdentityParams) (*kratosAdmin.CreateIdentityCreated, error) {
	return nil, errors.New("not implemented")
}

func (ka *fakeKratosAdminClient) CreateRecoveryLink(params *kratosAdmin.CreateRecoveryLinkParams) (*kratosAdmin.CreateRecoveryLinkOK, error) {
	return nil, errors.New("not implemented")
}

func convertKratosUserInfoToIdentity(t *testing.T, ui *KratosUserInfo) *kratosModels.Identity {
	return &kratosModels.Identity{
		Traits: ui,
	}
}

// Implements the hydraAdminClientService interface.
type fakeHydraAdminClient struct {
	redirect         string
	consentChallenge string

	oauthClientID           string
	getConsentRequestFn     *func(params *hydraAdmin.GetConsentRequestParams) (*hydraAdmin.GetConsentRequestOK, error)
	acceptConsentRequestFn  *func(params *hydraAdmin.AcceptConsentRequestParams) (*hydraAdmin.AcceptConsentRequestOK, error)
	acceptLoginRequestFn    *func(params *hydraAdmin.AcceptLoginRequestParams) (*hydraAdmin.AcceptLoginRequestOK, error)
	introspectOAuth2TokenFn *func(params *hydraAdmin.IntrospectOAuth2TokenParams) (*hydraAdmin.IntrospectOAuth2TokenOK, error)
}

func (ha *fakeHydraAdminClient) AcceptConsentRequest(params *hydraAdmin.AcceptConsentRequestParams) (*hydraAdmin.AcceptConsentRequestOK, error) {
	if ha.acceptConsentRequestFn != nil {
		return (*ha.acceptConsentRequestFn)(params)
	}

	return &hydraAdmin.AcceptConsentRequestOK{
		Payload: &hydraModels.CompletedRequest{
			RedirectTo: &ha.redirect,
		},
	}, nil
}

func (ha *fakeHydraAdminClient) AcceptLoginRequest(params *hydraAdmin.AcceptLoginRequestParams) (*hydraAdmin.AcceptLoginRequestOK, error) {
	if ha.acceptLoginRequestFn != nil {
		return (*ha.acceptLoginRequestFn)(params)
	}
	return &hydraAdmin.AcceptLoginRequestOK{
		Payload: &hydraModels.CompletedRequest{
			RedirectTo: &ha.redirect,
		},
	}, nil
}

func (ha *fakeHydraAdminClient) IntrospectOAuth2Token(params *hydraAdmin.IntrospectOAuth2TokenParams) (*hydraAdmin.IntrospectOAuth2TokenOK, error) {
	if ha.introspectOAuth2TokenFn == nil {
		return nil, errors.New("not implemented")
	}

	return (*ha.introspectOAuth2TokenFn)(params)
}

func (ha *fakeHydraAdminClient) AcceptLogoutRequest(params *hydraAdmin.AcceptLogoutRequestParams) (*hydraAdmin.AcceptLogoutRequestOK, error) {
	panic("not implemented")
}

func (ha *fakeHydraAdminClient) GetConsentRequest(params *hydraAdmin.GetConsentRequestParams) (*hydraAdmin.GetConsentRequestOK, error) {
	if ha.getConsentRequestFn != nil {
		return (*ha.getConsentRequestFn)(params)
	}
	return &hydraAdmin.GetConsentRequestOK{
		Payload: &hydraModels.ConsentRequest{
			Client: &hydraModels.OAuth2Client{
				ClientID: ha.oauthClientID,
			},
			RequestedScope:               []string{},
			RequestedAccessTokenAudience: []string{},
			Challenge:                    &ha.consentChallenge,
		},
	}, nil
}

func (ha *fakeHydraAdminClient) GetLoginRequest(params *hydraAdmin.GetLoginRequestParams) (*hydraAdmin.GetLoginRequestOK, error) {
	panic("not implemented")
}

func (ha *fakeHydraAdminClient) GetLogoutRequest(params *hydraAdmin.GetLogoutRequestParams) (*hydraAdmin.GetLogoutRequestOK, error) {
	panic("not implemented")
}

// Implements the kratosPublicClientService interface.
type fakeKratosPublicClient struct {
	userID string
}

func (kp *fakeKratosPublicClient) CompleteSelfServiceBrowserSettingsOIDCSettingsFlow(params *kratosPublic.CompleteSelfServiceBrowserSettingsOIDCSettingsFlowParams) error {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) CompleteSelfServiceLoginFlowWithPasswordMethod(params *kratosPublic.CompleteSelfServiceLoginFlowWithPasswordMethodParams) (*kratosPublic.CompleteSelfServiceLoginFlowWithPasswordMethodOK, error) {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) CompleteSelfServiceRegistrationFlowWithPasswordMethod(params *kratosPublic.CompleteSelfServiceRegistrationFlowWithPasswordMethodParams) (*kratosPublic.CompleteSelfServiceRegistrationFlowWithPasswordMethodOK, error) {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) GetSelfServiceLoginFlow(params *kratosPublic.GetSelfServiceLoginFlowParams) (*kratosPublic.GetSelfServiceLoginFlowOK, error) {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) GetSelfServiceRecoveryFlow(params *kratosPublic.GetSelfServiceRecoveryFlowParams) (*kratosPublic.GetSelfServiceRecoveryFlowOK, error) {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) GetSelfServiceRegistrationFlow(params *kratosPublic.GetSelfServiceRegistrationFlowParams) (*kratosPublic.GetSelfServiceRegistrationFlowOK, error) {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) GetSelfServiceSettingsFlow(params *kratosPublic.GetSelfServiceSettingsFlowParams, authInfo runtime.ClientAuthInfoWriter) (*kratosPublic.GetSelfServiceSettingsFlowOK, error) {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) InitializeSelfServiceBrowserLogoutFlow(params *kratosPublic.InitializeSelfServiceBrowserLogoutFlowParams) error {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) InitializeSelfServiceLoginViaBrowserFlow(params *kratosPublic.InitializeSelfServiceLoginViaBrowserFlowParams) error {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) InitializeSelfServiceRecoveryViaBrowserFlow(params *kratosPublic.InitializeSelfServiceRecoveryViaBrowserFlowParams) error {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) InitializeSelfServiceRegistrationViaBrowserFlow(params *kratosPublic.InitializeSelfServiceRegistrationViaBrowserFlowParams) error {
	panic("not implemented")
}

func (kp *fakeKratosPublicClient) Whoami(params *kratosPublic.WhoamiParams, authInfo runtime.ClientAuthInfoWriter) (*kratosPublic.WhoamiOK, error) {
	return &kratosPublic.WhoamiOK{
		Payload: &kratosModels.Session{
			Identity: &kratosModels.Identity{
				ID: kratosModels.UUID(kp.userID),
			},
		},
	}, nil
}
func makeClient(t *testing.T) (*HydraKratosClient, func()) {
	return makeClientFromConfig(t, &testClientConfig{})
}

type testClientConfig struct {
	postLoginRedirect       string
	hydraPublicHostCookie   string
	consentChallenge        string
	browserURL              string
	idpConsentPath          string
	hydraBrowserURL         string
	hydraConsentPath        string
	introspectOAuth2TokenFn *func(params *hydraAdmin.IntrospectOAuth2TokenParams) (*hydraAdmin.IntrospectOAuth2TokenOK, error)
	updateIdentityFn        *func(params *kratosAdmin.UpdateIdentityParams) (*kratosAdmin.UpdateIdentityOK, error)
	getIdentityFn           *func(params *kratosAdmin.GetIdentityParams) (*kratosAdmin.GetIdentityOK, error)
}

func fillDefaults(p *testClientConfig) *testClientConfig {
	if p.postLoginRedirect == "" {
		p.postLoginRedirect = "/auth/callback"
	}
	if p.hydraPublicHostCookie == "" {
		p.hydraPublicHostCookie = "hydraPublicHostCookie"
	}
	if p.consentChallenge == "" {
		p.consentChallenge = "123456789"
	}

	if p.browserURL == "" {
		p.browserURL = "https://work.withpixie.ai"
	}
	if p.idpConsentPath == "" {
		// This is the path that Hydra Consent will redirect to. Set in the Hydra config.
		p.idpConsentPath = "/api/auth/consent"
	}
	if p.hydraBrowserURL == "" {
		p.hydraBrowserURL = p.browserURL + "/hydra"
	}
	if p.hydraConsentPath == "" {
		// This is the path that AcceptHydraLogin returns.
		p.hydraConsentPath = "/oauth2/auth?audience=&client_id=auth-code-client"
	}

	return p
}
func makeClientFromConfig(t *testing.T, p *testClientConfig) (*HydraKratosClient, func()) {
	p = fillDefaults(p)

	// Set up the redirect URL from the fake endpoint.
	consentURL, err := url.Parse(p.browserURL + p.idpConsentPath)
	require.NoError(t, err)

	q := make(url.Values)
	q.Set("consent_challenge", p.consentChallenge)
	consentURL.RawQuery = q.Encode()

	// Setup the test server.
	hydraPublicHostFake := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		assert.Equal(t, p.hydraConsentPath, r.URL.String())
		assert.Equal(t, "abcd", r.Header.Get("ory_hydra_session"))
		// The actual endpoint sets a cookie, so we want do forward that as well.
		w.Header().Set("Set-Cookie", p.hydraPublicHostCookie)
		http.Redirect(w, r, consentURL.String(), 302)
	}))

	acceptConsentRequestFn := func(params *hydraAdmin.AcceptConsentRequestParams) (*hydraAdmin.AcceptConsentRequestOK, error) {
		return &hydraAdmin.AcceptConsentRequestOK{
			Payload: &hydraModels.CompletedRequest{
				RedirectTo: &p.postLoginRedirect,
			},
		}, nil
	}

	return &HydraKratosClient{
		httpClient: hydraPublicHostFake.Client(),
		Config: &HydraKratosConfig{
			KratosBrowserURL: "https://work.withpixie.ai/kratos",
			HydraBrowserURL:  p.hydraBrowserURL,
			HydraPublicHost:  hydraPublicHostFake.URL,
			HydraConsentPath: p.idpConsentPath,
		},
		hydraAdminClient: &fakeHydraAdminClient{
			introspectOAuth2TokenFn: p.introspectOAuth2TokenFn,
			redirect:                p.hydraBrowserURL + p.hydraConsentPath,
			acceptConsentRequestFn:  &acceptConsentRequestFn,
		},
		kratosPublicClient: &fakeKratosPublicClient{},
		kratosAdminClient: &fakeKratosAdminClient{
			getIdentityFn:    p.getIdentityFn,
			updateIdentityFn: p.updateIdentityFn,
		},
	}, hydraPublicHostFake.Close
}

func TestWhoami(t *testing.T) {
	client := HydraKratosClient{}

	kratosPublicClient := &fakeKratosPublicClient{userID: "1234"}
	client.kratosPublicClient = kratosPublicClient

	r := &http.Request{}
	r.Header = make(http.Header)
	r.Header.Set("Cookie", "notempty")

	whoami, err := client.Whoami(context.Background(), r)
	require.NoError(t, err)

	assert.Equal(t, kratosPublicClient.userID, whoami.ID())

	// Whoami should error out when we don't have a cookie in the header.
	r.Header.Del("Cookie")
	_, err = client.Whoami(context.Background(), r)
	assert.EqualError(t, err, "Request cookie is empty")
}

// getSessionFromResponse extracts a cookie session created in a response into a structure that can be
// inspected.
func getSessionFromResponse(t *testing.T, cookieStore sessions.Store, resp *http.Response, sessionKey string) *sessions.Session {
	cookies, ok := resp.Header["Set-Cookie"]
	require.True(t, ok)
	require.Len(t, cookies, 1)
	// Extract the cookie value by creating a new request, then feeding it to the cookieStore.
	testReq, err := http.NewRequest("", "/", nil)
	require.NoError(t, err)

	testReq.Header = make(http.Header)
	testReq.Header.Set("Cookie", cookies[0])
	session, err := cookieStore.Get(testReq, IDProviderSessionKey)
	require.NoError(t, err)

	return session
}

// Returns the URL without the query string.
func stripQuery(t *testing.T, urlStr string) string {
	u, err := url.Parse(urlStr)
	require.NoError(t, err)

	u.RawQuery = ""
	return u.String()
}

func createLoginRequest(t *testing.T, hydraLoginState, loginChallenge string) *http.Request {
	reqURL, err := url.Parse("/api/auth/oauth/login")
	require.NoError(t, err)

	q := url.Values{}
	if hydraLoginState != "" {
		q.Set(HydraLoginStateKey, hydraLoginState)
	}
	q.Set("login_challenge", loginChallenge)
	reqURL.RawQuery = q.Encode()

	req, err := http.NewRequest("GET", reqURL.String(), nil)
	require.NoError(t, err)

	// Set the host, matching what we expect internally.
	req.Host = "withpixie.ai"
	return req
}

func TestRedirectToLogin(t *testing.T) {
	c, cleanup := makeClient(t)
	defer cleanup()

	req := createLoginRequest(t, "", "abcd")

	cookieStore := sessions.NewCookieStore([]byte("pair"))
	session, err := cookieStore.New(req, IDProviderSessionKey)
	require.NoError(t, err)
	w := httptest.NewRecorder()

	err = c.RedirectToLogin(session, w, req)
	require.NoError(t, err)

	resp := w.Result()
	// Verify the call redirected the writer.
	assert.Equal(t, http.StatusFound, resp.StatusCode)

	loginRedirectURL := resp.Header.Get("Location")

	require.NotEmpty(t, loginRedirectURL)

	u, err := url.Parse(loginRedirectURL)
	require.NoError(t, err)

	returnTo := u.Query().Get("return_to")
	require.NotEmpty(t, returnTo)

	returnToU, err := url.Parse(returnTo)
	require.NoError(t, err)

	redirectHydraState := returnToU.Query().Get(HydraLoginStateKey)
	require.NotEmpty(t, returnTo)

	respSession := getSessionFromResponse(t, cookieStore, resp, IDProviderSessionKey)
	// Verify the login state matches in the return URL and cookies.
	assert.Equal(t, respSession.Values[HydraLoginStateKey], redirectHydraState)
}

func TestAcceptHydraLogin(t *testing.T) {
	loginChallenge := "abcdefgh"
	acceptLoginRequestFn := func(params *hydraAdmin.AcceptLoginRequestParams) (*hydraAdmin.AcceptLoginRequestOK, error) {
		// Make sure the loginChallenge is forwarded.
		assert.Equal(t, params.LoginChallenge, loginChallenge)
		// Call the original login request to handle the rest.
		return (&fakeHydraAdminClient{}).AcceptLoginRequest(params)
	}
	c, cleanup := makeClient(t)
	c.hydraAdminClient = &fakeHydraAdminClient{
		acceptLoginRequestFn: &acceptLoginRequestFn,
	}
	defer cleanup()

	// Fake whoami response.
	whoami := &Whoami{
		kratosSession: &kratosModels.Session{
			Identity: &kratosModels.Identity{
				ID: kratosModels.UUID("user"),
			},
		},
	}

	// Just make sure the error is not nil.
	_, err := c.AcceptHydraLogin(context.Background(), loginChallenge, whoami)
	require.NoError(t, err)
}

func TestConvertHydraURL(t *testing.T) {
	browserURL := "https://work.withpixie.ai/hydra"
	internalHost := "https://hydra.plc-dev.svc.cluster.local:4445"
	c := &HydraKratosClient{
		Config: &HydraKratosConfig{
			HydraBrowserURL: browserURL,
			HydraPublicHost: internalHost,
		},
	}

	hydraPath := "/oauth/auth/coolendpoint?query_param=1234"
	externalURL := browserURL + hydraPath

	// Should strip the full browserURL prefix and add to the internalHost.
	internalURL, err := c.convertExternalHydraURLToInternal(externalURL)
	require.NoError(t, err)

	// Set the expected internal.
	assert.Equal(t, internalHost+hydraPath, internalURL)
}

func TestInterceptHydraConsent(t *testing.T) {
	consentChallenge := "a1b2c3d4e5"
	hydraPublicHostCookie := "coolcookie"
	p := fillDefaults(&testClientConfig{consentChallenge: consentChallenge, hydraPublicHostCookie: hydraPublicHostCookie})

	c, cleanup := makeClientFromConfig(t, p)
	defer cleanup()

	consentURL := p.hydraBrowserURL + p.hydraConsentPath

	r := &http.Request{}
	r.Header = make(http.Header)
	r.Header.Set("ory_hydra_session", "abcd")

	header, challenge, err := c.InterceptHydraUserConsent(consentURL, r.Header)
	require.NoError(t, err)

	assert.Equal(t, consentChallenge, challenge)
	assert.Contains(t, header.Get("Set-Cookie"), hydraPublicHostCookie)
}

func TestAcceptConsent(t *testing.T) {
	consentChallenge := "123456789"
	getConsentRequestFn := func(params *hydraAdmin.GetConsentRequestParams) (*hydraAdmin.GetConsentRequestOK, error) {
		assert.Equal(t, consentChallenge, params.ConsentChallenge)
		return &hydraAdmin.GetConsentRequestOK{
			Payload: &hydraModels.ConsentRequest{
				Client: &hydraModels.OAuth2Client{
					ClientID: "hydra_client_id",
				},
				RequestedScope:               []string{"openid", "offline"},
				RequestedAccessTokenAudience: []string{"api"},
			},
		}, nil
	}
	redirectURL := "/oauth2/auth"
	acceptConsentRequestFn := func(params *hydraAdmin.AcceptConsentRequestParams) (*hydraAdmin.AcceptConsentRequestOK, error) {
		assert.ElementsMatch(t, []string{"openid", "offline"}, params.Body.GrantScope)
		assert.ElementsMatch(t, []string{"api"}, params.Body.GrantAccessTokenAudience)
		assert.Equal(t, consentChallenge, params.ConsentChallenge)
		return &hydraAdmin.AcceptConsentRequestOK{
			Payload: &hydraModels.CompletedRequest{
				RedirectTo: &redirectURL,
			},
		}, nil
	}
	hydraAdminClient := &fakeHydraAdminClient{
		redirect:               "/",
		consentChallenge:       consentChallenge,
		oauthClientID:          "hydra_client_id",
		getConsentRequestFn:    &getConsentRequestFn,
		acceptConsentRequestFn: &acceptConsentRequestFn,
	}
	c := HydraKratosClient{
		Config: &HydraKratosConfig{
			HydraClientID: "hydra_client_id",
		},
		hydraAdminClient: hydraAdminClient,
	}

	consentResp, err := c.AcceptConsent(context.Background(), hydraAdminClient.consentChallenge)
	require.NoError(t, err)
	assert.Equal(t, redirectURL, *consentResp.RedirectTo)
}

func TestAcceptConsentWithWrongClientID(t *testing.T) {
	// We test to make sure that mismatching client ids throws an error.
	hydraAdminClient := &fakeHydraAdminClient{
		oauthClientID:    "not_hydra_client_id",
		consentChallenge: "123456",
	}
	c := HydraKratosClient{
		Config: &HydraKratosConfig{
			HydraClientID: "hydra_client_id",
		},
		hydraAdminClient: hydraAdminClient,
	}

	_, err := c.AcceptConsent(context.Background(), hydraAdminClient.consentChallenge)
	assert.EqualError(t, err, "'not_hydra_client_id' not an allowed client")
}

func TestHandleLogin(t *testing.T) {
	postLoginRedirect := "/auth/callback"
	consentRedirectCookie := "consentRedirectCookie"
	c, cleanup := makeClientFromConfig(t, &testClientConfig{
		postLoginRedirect:     postLoginRedirect,
		hydraPublicHostCookie: consentRedirectCookie,
	})
	defer cleanup()

	hydraLoginState := "abcdef"
	loginChallenge := "ghijkl"

	req := createLoginRequest(t, hydraLoginState, loginChallenge)
	req.Header.Set("Cookie", "whoamicookie")
	req.Header.Set("ory_hydra_session", "abcd")

	cookieStore := sessions.NewCookieStore([]byte("pair"))
	session, err := cookieStore.New(req, IDProviderSessionKey)
	require.NoError(t, err)

	session.Values[HydraLoginStateKey] = hydraLoginState

	w := httptest.NewRecorder()

	err = c.HandleLogin(session, w, req)
	require.NoError(t, err)

	resp := w.Result()
	// Verify the call redirected the writer.
	assert.Equal(t, http.StatusFound, resp.StatusCode)

	redirectToURL := resp.Header.Get("Location")
	require.NotEmpty(t, redirectToURL)
	// Make sure the redirection is the same as the consentRedirect.
	assert.Equal(t, postLoginRedirect, redirectToURL)

	// Make sure header is set from InterceptHydraUserConsent().
	interceptHeader := resp.Header.Get("Set-Cookie")
	assert.Equal(t, consentRedirectCookie, interceptHeader)
}

func getRedirectURL(t *testing.T, resp *http.Response) string {
	loginRedirectURL := resp.Header.Get("Location")

	require.NotEmpty(t, loginRedirectURL)

	return loginRedirectURL
}

func TestHandleLoginPerformsRedirects(t *testing.T) {
	// Performs redirect on state mismatch.
	c, cleanup := makeClient(t)
	defer cleanup()

	req := createLoginRequest(t, "state1", "abcd")

	cookieStore := sessions.NewCookieStore([]byte("pair"))
	session, err := cookieStore.New(req, IDProviderSessionKey)
	require.NoError(t, err)

	session.Values[HydraLoginStateKey] = "state2"
	w := httptest.NewRecorder()
	err = c.HandleLogin(session, w, req)
	require.NoError(t, err)

	resp := w.Result()
	assert.Equal(t, http.StatusFound, w.Code)
	loginURL, err := c.kratosLoginURL("")
	require.NoError(t, err)

	assert.Equal(t, stripQuery(t, loginURL), stripQuery(t, getRedirectURL(t, resp)))

	// Performs login when no state in query.
	// empty string for hydra_login_state prevents it from being added.
	req = createLoginRequest(t, "", "abcd")

	w = httptest.NewRecorder()
	err = c.HandleLogin(session, w, req)
	require.NoError(t, err)

	resp = w.Result()
	assert.Equal(t, http.StatusFound, w.Code)
	assert.Equal(t, stripQuery(t, loginURL), stripQuery(t, getRedirectURL(t, resp)))
}

func TestManageUserInfo(t *testing.T) {
	email := "a@b.com"
	plUserID := "123456789"
	plOrgID := "b.com"
	kratosID := "6f41e7a4-12ef-44bd-8bfe-c307a1cf325f"
	token := "usertoken"

	updateIdentityFn := func(params *kratosAdmin.UpdateIdentityParams) (*kratosAdmin.UpdateIdentityOK, error) {
		assert.Equal(t, params.ID, plUserID)
		ui := &KratosUserInfo{
			Email:    email,
			PLOrgID:  plOrgID,
			PLUserID: plUserID,
		}
		assert.Equal(t, params.Body.Traits, ui)
		return &kratosAdmin.UpdateIdentityOK{
			Payload: convertKratosUserInfoToIdentity(t, ui),
		}, nil
	}
	introspectOAuth2TokenFn := func(params *hydraAdmin.IntrospectOAuth2TokenParams) (*hydraAdmin.IntrospectOAuth2TokenOK, error) {
		assert.Equal(t, params.Token, token)
		return &hydraAdmin.IntrospectOAuth2TokenOK{
			Payload: &hydraModels.OAuth2TokenIntrospection{
				Sub: plUserID,
			},
		}, nil
	}

	getIdentityFn := func(params *kratosAdmin.GetIdentityParams) (*kratosAdmin.GetIdentityOK, error) {
		var idStruct strfmt.UUID4
		require.NoError(t, idStruct.UnmarshalText([]byte(kratosID)))
		identy := convertKratosUserInfoToIdentity(t, &KratosUserInfo{
			Email:    email,
			PLOrgID:  plOrgID,
			PLUserID: plUserID,
		})
		identy.ID = kratosModels.UUID(idStruct)
		assert.Equal(t, params.ID, plUserID)
		return &kratosAdmin.GetIdentityOK{
			Payload: identy,
		}, nil
	}

	c, cleanup := makeClientFromConfig(t, &testClientConfig{
		updateIdentityFn:        &updateIdentityFn,
		introspectOAuth2TokenFn: &introspectOAuth2TokenFn,
		getIdentityFn:           &getIdentityFn,
	})

	defer cleanup()
	_, err := c.UpdateUserInfo(context.Background(), plUserID, &KratosUserInfo{
		Email:    email,
		PLOrgID:  plOrgID,
		PLUserID: plUserID,
	})
	require.NoError(t, err)

	userID, err := c.GetUserIDFromToken(context.Background(), token)
	require.NoError(t, err)
	assert.Equal(t, userID, plUserID)

	userInfo, err := c.GetUserInfo(context.Background(), userID)
	require.NoError(t, err)

	assert.Equal(t, userInfo.KratosID, kratosID)
	assert.Equal(t, userInfo.Email, email)
	assert.Equal(t, userInfo.PLUserID, plUserID)
	assert.Equal(t, userInfo.PLOrgID, plOrgID)
}

func Test_CreateIdentity(t *testing.T) {
	c, cleanup := makeClient(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_idprovider.NewMockkratosAdminClientService(ctrl)
	c.kratosAdminClient = d

	var idStruct strfmt.UUID4
	require.NoError(t, idStruct.UnmarshalText([]byte("08c254cb-741b-4088-9fa4-19806efe497a")))

	schemaID := ""

	d.EXPECT().CreateIdentity(&kratosAdmin.CreateIdentityParams{
		Context: context.Background(),
		Body: &kratosModels.CreateIdentity{
			SchemaID: &schemaID,
			Traits:   &KratosUserInfo{Email: "blahblah@gmail.com"},
		},
	}).
		Return(&kratosAdmin.CreateIdentityCreated{
			Payload: &kratosModels.Identity{
				ID: kratosModels.UUID(idStruct),
			},
		}, nil)

	ident, err := c.CreateIdentity(context.Background(), "blahblah@gmail.com")
	require.NoError(t, err)

	assert.Equal(t, ident.AuthProviderID, "08c254cb-741b-4088-9fa4-19806efe497a")
}

func Test_CreateInviteLinkForIdentity(t *testing.T) {
	c, cleanup := makeClient(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_idprovider.NewMockkratosAdminClientService(ctrl)
	c.kratosAdminClient = d

	var idStruct strfmt.UUID4
	require.NoError(t, idStruct.UnmarshalText([]byte("08c254cb-741b-4088-9fa4-19806efe497a")))

	link := "https://work.withpixie.dev/recovery"

	d.EXPECT().CreateRecoveryLink(&kratosAdmin.CreateRecoveryLinkParams{
		Context: context.Background(),
		Body: &kratosModels.CreateRecoveryLink{
			IdentityID: kratosModels.UUID(idStruct),
		},
	}).
		Return(&kratosAdmin.CreateRecoveryLinkOK{
			Payload: &kratosModels.RecoveryLink{
				RecoveryLink: &link,
			},
		}, nil)

	linkResp, err := c.CreateInviteLinkForIdentity(context.Background(), &idmanager.CreateInviteLinkForIdentityRequest{
		AuthProviderID: "08c254cb-741b-4088-9fa4-19806efe497a",
	})
	require.NoError(t, err)

	assert.Equal(t, linkResp.InviteLink, link)
}
