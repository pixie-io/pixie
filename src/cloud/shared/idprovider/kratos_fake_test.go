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

	kratos "github.com/ory/kratos-client-go"
)

type kratosFakeAPI struct {
	kratos.FrontendAPI
	kratos.IdentityAPI
	userID       string
	recoveryLink string
}

func (k kratosFakeAPI) ToSession(ctx context.Context) kratos.FrontendAPIToSessionRequest {
	return kratos.FrontendAPIToSessionRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) ToSessionExecute(r kratos.FrontendAPIToSessionRequest) (*kratos.Session, *http.Response, error) {
	return &kratos.Session{
		Identity: &kratos.Identity{
			Id: k.userID,
		},
	}, nil, nil
}

func (k kratosFakeAPI) CreateIdentity(context.Context) kratos.IdentityAPICreateIdentityRequest {
	return kratos.IdentityAPICreateIdentityRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) CreateIdentityExecute(r kratos.IdentityAPICreateIdentityRequest) (*kratos.Identity, *http.Response, error) {
	return &kratos.Identity{
		Id: k.userID,
	}, nil, nil
}

func (k kratosFakeAPI) GetIdentity(context.Context, string) kratos.IdentityAPIGetIdentityRequest {
	return kratos.IdentityAPIGetIdentityRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) CreateRecoveryLinkForIdentity(context.Context) kratos.IdentityAPICreateRecoveryLinkForIdentityRequest {
	return kratos.IdentityAPICreateRecoveryLinkForIdentityRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) CreateRecoveryLinkForIdentityExecute(r kratos.IdentityAPICreateRecoveryLinkForIdentityRequest) (*kratos.RecoveryLinkForIdentity, *http.Response, error) {
	return &kratos.RecoveryLinkForIdentity{
		RecoveryLink: k.recoveryLink,
	}, nil, nil
}
