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
	"net/http"

	hydra "github.com/ory/hydra-client-go/v2"
)

// Implements the hydraAdminClientService interface.
type fakeHydraAdminClient struct {
	hydra.OAuth2API
	redirect         string
	consentChallenge string

	oauthClientID           string
	getConsentRequestFn     *func(params *hydra.OAuth2APIGetOAuth2ConsentRequestRequest) (*hydra.OAuth2ConsentRequest, *http.Response, error)
	acceptConsentRequestFn  *func(params *hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest) (*hydra.OAuth2RedirectTo, *http.Response, error)
	acceptLoginRequestFn    *func(params *hydra.OAuth2APIAcceptOAuth2LoginRequestRequest) (*hydra.OAuth2RedirectTo, *http.Response, error)
	introspectOAuth2TokenFn *func(req *hydra.OAuth2APIIntrospectOAuth2TokenRequest) (*hydra.IntrospectedOAuth2Token, *http.Response, error)
}

func (ha *fakeHydraAdminClient) AcceptOAuth2LoginRequest(ctx context.Context) hydra.OAuth2APIAcceptOAuth2LoginRequestRequest {
	return hydra.OAuth2APIAcceptOAuth2LoginRequestRequest{
		ApiService: ha,
	}
}

func (ha *fakeHydraAdminClient) AcceptOAuth2LoginRequestExecute(r hydra.OAuth2APIAcceptOAuth2LoginRequestRequest) (*hydra.OAuth2RedirectTo, *http.Response, error) {
	if ha.acceptLoginRequestFn != nil {
		return (*ha.acceptLoginRequestFn)(&r)
	}

	return &hydra.OAuth2RedirectTo{
		RedirectTo: ha.redirect,
	}, nil, nil
}

func (ha *fakeHydraAdminClient) AcceptOAuth2ConsentRequest(ctx context.Context) hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest {
	return hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest{
		ApiService: ha,
	}
}

func (ha *fakeHydraAdminClient) AcceptOAuth2ConsentRequestExecute(params hydra.OAuth2APIAcceptOAuth2ConsentRequestRequest) (*hydra.OAuth2RedirectTo, *http.Response, error) {
	if ha.acceptConsentRequestFn != nil {
		return (*ha.acceptConsentRequestFn)(&params)
	}

	return &hydra.OAuth2RedirectTo{
		RedirectTo: ha.redirect,
	}, nil, nil
}

func (ha *fakeHydraAdminClient) IntrospectOAuth2Token(context.Context) hydra.OAuth2APIIntrospectOAuth2TokenRequest {
	return hydra.OAuth2APIIntrospectOAuth2TokenRequest{
		ApiService: ha,
	}
}

func (ha *fakeHydraAdminClient) GetOAuth2ConsentRequest(context.Context) hydra.OAuth2APIGetOAuth2ConsentRequestRequest {
	return hydra.OAuth2APIGetOAuth2ConsentRequestRequest{
		ApiService: ha,
	}
}

func (ha *fakeHydraAdminClient) GetOAuth2ConsentRequestExecute(params hydra.OAuth2APIGetOAuth2ConsentRequestRequest) (*hydra.OAuth2ConsentRequest, *http.Response, error) {
	if ha.getConsentRequestFn != nil {
		return (*ha.getConsentRequestFn)(&params)
	}
	return &hydra.OAuth2ConsentRequest{
		LoginChallenge:               &ha.consentChallenge,
		RequestedScope:               []string{},
		RequestedAccessTokenAudience: []string{},
		Client: &hydra.OAuth2Client{
			ClientId: &ha.oauthClientID,
		},
	}, nil, nil
}
