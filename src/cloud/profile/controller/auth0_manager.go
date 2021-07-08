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

package controller

import (
	"context"
	"errors"

	"px.dev/pixie/src/cloud/profile/controller/idmanager"
)

// Auth0Manager implements the idmanager.Manager interface for Auth0.
type Auth0Manager struct{}

// NewAuth0Manager creates a new Auth0Manager.
func NewAuth0Manager() (*Auth0Manager, error) {
	return &Auth0Manager{}, nil
}

// CreateInviteLink implements the idmanager.Manager interface. We have yet to implement this flow so it fails in the mean time.
func (c *Auth0Manager) CreateInviteLink(context.Context, *idmanager.CreateInviteLinkRequest) (*idmanager.CreateInviteLinkResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support generating InviteLinks")
}

// CreateInviteLinkForIdentity sits here just to warn users that we don't implement this for Auth0.
func (c *Auth0Manager) CreateInviteLinkForIdentity(context.Context, *idmanager.CreateInviteLinkForIdentityRequest) (*idmanager.CreateInviteLinkForIdentityResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support generating InviteLinks")
}

// CreateIdentity sits here just to warn users that we don't implement this for Auth0.
func (c *Auth0Manager) CreateIdentity(context.Context, string) (*idmanager.CreateIdentityResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support creating identities")
}

// SetPLMetadata is not implemented for auth0.
func (c *Auth0Manager) SetPLMetadata(userID, plOrgID, plUserID string) error {
	return errors.New("auth0 not supported")
}
