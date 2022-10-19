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
	kratos.V0alpha2Api

	userID       string
	recoveryLink string
}

func (k kratosFakeAPI) ToSession(ctx context.Context) kratos.V0alpha2ApiApiToSessionRequest {
	return kratos.V0alpha2ApiApiToSessionRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) ToSessionExecute(r kratos.V0alpha2ApiApiToSessionRequest) (*kratos.Session, *http.Response, error) {
	return &kratos.Session{
		Identity: kratos.Identity{
			Id: k.userID,
		},
	}, nil, nil
}

func (k kratosFakeAPI) AdminCreateIdentity(ctx context.Context) kratos.V0alpha2ApiApiAdminCreateIdentityRequest {
	return kratos.V0alpha2ApiApiAdminCreateIdentityRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) AdminCreateIdentityExecute(r kratos.V0alpha2ApiApiAdminCreateIdentityRequest) (*kratos.Identity, *http.Response, error) {
	return &kratos.Identity{
		Id: k.userID,
	}, nil, nil
}

func (k kratosFakeAPI) AdminCreateSelfServiceRecoveryLink(ctx context.Context) kratos.V0alpha2ApiApiAdminCreateSelfServiceRecoveryLinkRequest {
	return kratos.V0alpha2ApiApiAdminCreateSelfServiceRecoveryLinkRequest{
		ApiService: k,
	}
}

func (k kratosFakeAPI) AdminCreateSelfServiceRecoveryLinkExecute(r kratos.V0alpha2ApiApiAdminCreateSelfServiceRecoveryLinkRequest) (*kratos.SelfServiceRecoveryLink, *http.Response, error) {
	return &kratos.SelfServiceRecoveryLink{
		RecoveryLink: k.recoveryLink,
	}, nil, nil
}
