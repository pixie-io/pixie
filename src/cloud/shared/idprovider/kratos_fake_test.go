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
	userID       string
	recoveryLink string
}

// Create a custom request type that implements the method chaining
type fakeToSessionRequest struct {
	kratosFakeAPI *kratosFakeAPI
	cookie        string
}

func (r *fakeToSessionRequest) Cookie(cookie string) kratos.FrontendAPIToSessionRequest {
	r.cookie = cookie
	return kratos.FrontendAPIToSessionRequest{}
}

func (r *fakeToSessionRequest) Execute() (*kratos.Session, *http.Response, error) {
	return &kratos.Session{
		Identity: &kratos.Identity{
			Id: r.kratosFakeAPI.userID,
		},
	}, nil, nil
}

func (k kratosFakeAPI) ToSession(ctx context.Context) kratos.FrontendAPIToSessionRequest {
	return kratos.FrontendAPIToSessionRequest{}
}

// func (k kratosFakeAPI) ToSessionExecute(r kratos.FrontendAPIToSessionRequest) (*kratos.Session, *http.Response, error) {
// 	return &kratos.Session{
// 		Identity: &kratos.Identity{
// 			Id: k.userID,
// 		},
// 	}, nil, nil
// }

func (k kratosFakeAPI) GetIdentity(ctx context.Context, id string) kratos.IdentityAPIGetIdentityRequest {
	return kratos.IdentityAPIGetIdentityRequest{}
}

func (k kratosFakeAPI) GetIdentityExecute(r kratos.IdentityAPIGetIdentityRequest) (*kratos.Identity, *http.Response, error) {
	return &kratos.Identity{
		Id: k.userID,
	}, nil, nil
}

func (k kratosFakeAPI) CreateIdentity(ctx context.Context) kratos.IdentityAPICreateIdentityRequest {
	return kratos.IdentityAPICreateIdentityRequest{}
}

func (k kratosFakeAPI) CreateIdentityExecute(r kratos.IdentityAPICreateIdentityRequest) (*kratos.Identity, *http.Response, error) {
	return &kratos.Identity{
		Id: k.userID,
	}, nil, nil
}

func (k kratosFakeAPI) CreateRecoveryLinkForIdentity(ctx context.Context) kratos.IdentityAPICreateRecoveryLinkForIdentityRequest {
	return kratos.IdentityAPICreateRecoveryLinkForIdentityRequest{}
}

func (k kratosFakeAPI) CreateRecoveryLinkForIdentityExecute(r kratos.IdentityAPICreateRecoveryLinkForIdentityRequest) (*kratos.CreateRecoveryLinkForIdentityBody, *http.Response, error) {
	return &kratos.CreateRecoveryLinkForIdentityBody{}, nil, nil
}
