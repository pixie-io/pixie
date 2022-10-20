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

	"px.dev/pixie/src/cloud/shared/idprovider"
)

const kratosIdentityProvider = "kratos"

func transformKratosUserInfoToUserInfo(kratosUser *idprovider.KratosUserInfo) (*UserInfo, error) {
	u := &UserInfo{
		Email: kratosUser.Email,
		// Stop gap while email server has not been added to the deploy scheme.
		EmailVerified:    true,
		IdentityProvider: kratosIdentityProvider,
		AuthProviderID:   kratosUser.KratosID,
	}
	return u, nil
}

// HydraKratosUserClient exposes user management for hydra and kratos.
type HydraKratosUserClient interface {
	GetUserIDFromToken(ctx context.Context, token string) (string, error)
	GetUserInfo(ctx context.Context, userID string) (*idprovider.KratosUserInfo, error)
	CreateInviteLinkForIdentity(ctx context.Context, req *idprovider.CreateInviteLinkForIdentityRequest) (*idprovider.CreateInviteLinkForIdentityResponse, error)
	CreateIdentity(ctx context.Context, email string) (*idprovider.CreateIdentityResponse, error)
}

// HydraKratosConnector implements the AuthProvider interface for Hydra + Kratos.
type HydraKratosConnector struct {
	Client HydraKratosUserClient
}

// NewHydraKratosConnector provides an implementation of an HydraKratosConnector.
func NewHydraKratosConnector() (*HydraKratosConnector, error) {
	client, err := idprovider.NewHydraKratosClient()
	if err != nil {
		return nil, err
	}
	return &HydraKratosConnector{client}, nil
}

// GetUserIDFromToken returns the UserID for the particular token.
func (a *HydraKratosConnector) GetUserIDFromToken(token string) (string, error) {
	return a.Client.GetUserIDFromToken(context.Background(), token)
}

// GetUserInfo returns the UserInfo for this userID.
func (a *HydraKratosConnector) GetUserInfo(userID string) (*UserInfo, error) {
	kratosInfo, err := a.Client.GetUserInfo(context.Background(), userID)
	if err != nil {
		return nil, err
	}

	return transformKratosUserInfoToUserInfo(kratosInfo)
}

// GetUserInfoFromAccessToken fetches and returns the UserInfo for the given access token.
func (a *HydraKratosConnector) GetUserInfoFromAccessToken(accessToken string) (*UserInfo, error) {
	userID, err := a.GetUserIDFromToken(accessToken)
	if err != nil {
		return nil, err
	}
	return a.GetUserInfo(userID)
}

// CreateIdentity creates an identity for the passed in email.
func (a *HydraKratosConnector) CreateIdentity(email string) (*CreateIdentityResponse, error) {
	resp, err := a.Client.CreateIdentity(context.Background(), email)
	if err != nil {
		return nil, err
	}
	return &CreateIdentityResponse{
		IdentityProvider: resp.IdentityProvider,
		AuthProviderID:   resp.AuthProviderID,
	}, nil
}

// CreateInviteLink takes the auth provider ID for a user and creates an Invite Link for that user.
func (a *HydraKratosConnector) CreateInviteLink(authProviderID string) (*CreateInviteLinkResponse, error) {
	ident, err := a.Client.CreateInviteLinkForIdentity(context.Background(), &idprovider.CreateInviteLinkForIdentityRequest{
		AuthProviderID: authProviderID,
	})
	if err != nil {
		return nil, err
	}
	return &CreateInviteLinkResponse{InviteLink: ident.InviteLink}, nil
}
