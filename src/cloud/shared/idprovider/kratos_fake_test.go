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

	"github.com/go-openapi/runtime"
	kratosAdmin "github.com/ory/kratos-client-go/client/admin"
	kratosPublic "github.com/ory/kratos-client-go/client/public"
	kratosModels "github.com/ory/kratos-client-go/models"
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

func (ka *fakeKratosAdminClient) CreateIdentity(params *kratosAdmin.CreateIdentityParams) (*kratosAdmin.CreateIdentityCreated, error) {
	return nil, errors.New("not implemented")
}

func (ka *fakeKratosAdminClient) CreateRecoveryLink(params *kratosAdmin.CreateRecoveryLinkParams) (*kratosAdmin.CreateRecoveryLinkOK, error) {
	return nil, errors.New("not implemented")
}

// Implements the kratosPublicClientService interface.
type fakeKratosPublicClient struct {
	userID string
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
