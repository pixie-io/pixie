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

	hydra "github.com/ory/hydra-client-go/v2"
)

// Implements the hydraAdminClientService interface.
type fakeHydraAdminClient struct {
	redirect         string
	consentChallenge string
	oauthClientID    string

	// Function hooks for testing
	getConsentRequestFn     func(string) (*hydra.OAuth2ConsentRequest, error)
	acceptConsentRequestFn  func(string, *hydra.AcceptOAuth2ConsentRequest) (*hydra.OAuth2RedirectTo, error)
	acceptLoginRequestFn    func(string, *hydra.AcceptOAuth2LoginRequest) (*hydra.OAuth2RedirectTo, error)
	introspectOAuth2TokenFn func(string) (*hydra.IntrospectOAuth2TokenResponse, error)
}

func (ha *fakeHydraAdminClient) AcceptOAuth2ConsentRequest(ctx context.Context) hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest {
	return fakeAcceptConsentRequest{ha: ha}
}

type fakeAcceptConsentRequest struct {
	ha               *fakeHydraAdminClient
	consentChallenge string
	body             *hydra.AcceptOAuth2ConsentRequest
}

func (r fakeAcceptConsentRequest) ConsentChallenge(challenge string) hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest {
	r.consentChallenge = challenge
	return r
}

func (r fakeAcceptConsentRequest) AcceptOAuth2ConsentRequest(body hydra.AcceptOAuth2ConsentRequest) hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest {
	r.body = &body
	return r
}

func (r fakeAcceptConsentRequest) Execute() (*hydra.OAuth2RedirectTo, *http.Response, error) {
	if r.ha.acceptConsentRequestFn != nil {
		resp, err := r.ha.acceptConsentRequestFn(r.consentChallenge, r.body)
		return resp, nil, err
	}
	return &hydra.OAuth2RedirectTo{
		RedirectTo: &r.ha.redirect,
	}, nil, nil
}

func (ha *fakeHydraAdminClient) AcceptOAuth2LoginRequest(ctx context.Context) hydra.OAuth2APIAcceptOAuth2LoginRequestRequest {
	return fakeAcceptLoginRequest{ha: ha}
}

type fakeAcceptLoginRequest struct {
	ha             *fakeHydraAdminClient
	loginChallenge string
	body           *hydra.AcceptOAuth2LoginRequest
}

func (r fakeAcceptLoginRequest) LoginChallenge(challenge string) hydra.OAuth2APIAcceptOAuth2LoginRequestRequest {
	r.loginChallenge = challenge
	return r
}

func (r fakeAcceptLoginRequest) AcceptOAuth2LoginRequest(body hydra.AcceptOAuth2LoginRequest) hydra.OAuth2APIAcceptOAuth2LoginRequestRequest {
	r.body = &body
	return r
}

func (r fakeAcceptLoginRequest) Execute() (*hydra.OAuth2RedirectTo, *http.Response, error) {
	if r.ha.acceptLoginRequestFn != nil {
		resp, err := r.ha.acceptLoginRequestFn(r.loginChallenge, r.body)
		return resp, nil, err
	}
	return &hydra.OAuth2RedirectTo{
		RedirectTo: &r.ha.redirect,
	}, nil, nil
}

func (ha *fakeHydraAdminClient) IntrospectOAuth2Token(ctx context.Context) hydra.OAuth2APIIntrospectOAuth2TokenRequest {
	return fakeIntrospectRequest{ha: ha}
}

type fakeIntrospectRequest struct {
	ha    *fakeHydraAdminClient
	token string
}

func (r fakeIntrospectRequest) Token(token string) hydra.OAuth2APIIntrospectOAuth2TokenRequest {
	r.token = token
	return r
}

func (r fakeIntrospectRequest) Execute() (*hydra.IntrospectOAuth2TokenResponse, *http.Response, error) {
	if r.ha.introspectOAuth2TokenFn == nil {
		return nil, nil, errors.New("not implemented")
	}
	resp, err := r.ha.introspectOAuth2TokenFn(r.token)
	return resp, nil, err
}

func (ha *fakeHydraAdminClient) GetOAuth2ConsentRequest(ctx context.Context) hydra.OAuth2APIGetOAuth2ConsentRequestRequest {
	return fakeGetConsentRequest{ha: ha}
}

type fakeGetConsentRequest struct {
	ha               *fakeHydraAdminClient
	consentChallenge string
}

func (r fakeGetConsentRequest) ConsentChallenge(challenge string) hydra.OAuth2APIGetOAuth2ConsentRequestRequest {
	r.consentChallenge = challenge
	return r
}

func (r fakeGetConsentRequest) Execute() (*hydra.OAuth2ConsentRequest, *http.Response, error) {
	if r.ha.getConsentRequestFn != nil {
		resp, err := r.ha.getConsentRequestFn(r.consentChallenge)
		return resp, nil, err
	}
	return &hydra.OAuth2ConsentRequest{
		Client: &hydra.OAuth2Client{
			ClientId: &r.ha.oauthClientID,
		},
		RequestedScope:               []string{},
		RequestedAccessTokenAudience: []string{},
		Challenge:                    &r.ha.consentChallenge,
	}, nil, nil
}
