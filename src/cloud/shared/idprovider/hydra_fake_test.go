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
	"errors"

	hydraAdmin "github.com/ory/hydra-client-go/client/admin"
	hydraModels "github.com/ory/hydra-client-go/models"
)

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
