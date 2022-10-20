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

package controllers_test

import (
	"context"
	"errors"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/auth/controllers"
	"px.dev/pixie/src/cloud/shared/idprovider"
)

// Simple testing fake just to check one piece.
type testHydraKratosUserClient struct{}

func (c *testHydraKratosUserClient) GetUserIDFromToken(_ context.Context, token string) (string, error) {
	return "", errors.New("not implemented")
}

func (c *testHydraKratosUserClient) GetUserInfo(ctx context.Context, userID string) (*idprovider.KratosUserInfo, error) {
	return &idprovider.KratosUserInfo{}, nil
}

func (c *testHydraKratosUserClient) CreateInviteLinkForIdentity(ctx context.Context, req *idprovider.CreateInviteLinkForIdentityRequest) (*idprovider.CreateInviteLinkForIdentityResponse, error) {
	return nil, errors.New("not implemented")
}

func (c *testHydraKratosUserClient) CreateIdentity(ctx context.Context, email string) (*idprovider.CreateIdentityResponse, error) {
	return nil, errors.New("not implemented")
}

func TestGetUserInfoReturnsKratosAsIdProvider(t *testing.T) {
	connector := &controllers.HydraKratosConnector{Client: &testHydraKratosUserClient{}}
	userInfo, err := connector.GetUserInfo("")
	require.NoError(t, err)
	assert.Equal(t, "kratos", userInfo.IdentityProvider)
	assert.True(t, userInfo.EmailVerified)
}
