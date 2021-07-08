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

package idmanager

import "context"

// CreateInviteLinkRequest is the request value for the invite link method.
type CreateInviteLinkRequest struct {
	Email    string
	PLOrgID  string
	PLUserID string
}

// CreateInviteLinkResponse is the response value for the invite link method.
type CreateInviteLinkResponse struct {
	Email      string
	InviteLink string
}

// CreateInviteLinkForIdentityRequest is the request value for the invite link method.
type CreateInviteLinkForIdentityRequest struct {
	AuthProviderID string
}

// CreateInviteLinkForIdentityResponse contains the response for the invite link method.
type CreateInviteLinkForIdentityResponse struct {
	InviteLink string
}

// CreateIdentityResponse contains relevant information about the Identity that was created.
type CreateIdentityResponse struct {
	IdentityProvider string
	AuthProviderID   string
}

// Manager is the interface for an identity provider's user management API.
type Manager interface {
	// CreateIdentity will create an identity for the corresponding email.
	CreateIdentity(ctx context.Context, email string) (*CreateIdentityResponse, error)
	CreateInviteLink(ctx context.Context, req *CreateInviteLinkRequest) (*CreateInviteLinkResponse, error)
	CreateInviteLinkForIdentity(ctx context.Context, req *CreateInviteLinkForIdentityRequest) (*CreateInviteLinkForIdentityResponse, error)
	// SetPLMetadata sets the pixielabs related metadata in the auth provider.
	SetPLMetadata(userID, plOrgID, plUserID string) error
}
