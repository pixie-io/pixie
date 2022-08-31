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

package controllers

import (
	"context"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/cloud/auth/authenv"
)

// APIKeyMgr is the internal interface for managing API keys.
type APIKeyMgr interface {
	FetchOrgUserIDUsingAPIKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, error)
}

// UserInfo contains all the info about a user. It's not tied to any specific AuthProvider.
type UserInfo struct {
	// The following fields are from the AuthProvider.
	Email         string
	EmailVerified bool
	FirstName     string
	LastName      string
	Name          string
	Picture       string

	// IdentityProvider is the name of the provider that the User used to Login. This is distinct
	// from AuthProviders - there might be many IdentityProviders that a single AuthProvider implements. Ie
	// google-oauth and github might both be IdentityProviders for Auth0.
	IdentityProvider string
	// AuthProviderID is the ID assigned to the user internal to the AuthProvider.
	AuthProviderID string
	// HostedDomain is the name of an org that a user belongs to according to the IdentityProvider.
	// If empty, the IdentityProvider does not consider the user as part of an org.
	HostedDomain string
}

// CreateInviteLinkResponse contaions the InviteLink and any accompanying information.
type CreateInviteLinkResponse struct {
	InviteLink string
}

// CreateIdentityResponse contains relevant information about the Identity that was created.
type CreateIdentityResponse struct {
	IdentityProvider string
	AuthProviderID   string
}

// AuthProvider interfaces the service we use for auth.
type AuthProvider interface {
	// GetUserInfoFromAccessToken fetches and returns the UserInfo for the given access token.
	GetUserInfoFromAccessToken(accessToken string) (*UserInfo, error)
	// CreateInviteLinkForIdentity creates an invite link for the specific user, identified by the AuthProviderID.
	CreateInviteLink(authProviderID string) (*CreateInviteLinkResponse, error)
	// CreateIdentity will create an identity for the corresponding email.
	CreateIdentity(email string) (*CreateIdentityResponse, error)
}

// Server defines an gRPC server type.
type Server struct {
	env       authenv.AuthEnv
	a         AuthProvider
	apiKeyMgr APIKeyMgr
}

// NewServer creates GRPC handlers.
func NewServer(env authenv.AuthEnv, a AuthProvider, apiKeyMgr APIKeyMgr) (*Server, error) {
	return &Server{
		env:       env,
		a:         a,
		apiKeyMgr: apiKeyMgr,
	}, nil
}
