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
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/metadata"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/auth/authpb"
)

func TestAuthServer_LoginAccessToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockAuth.EXPECT().Login(gomock.Any(), &authpb.LoginRequest{
		AccessToken: "test-token",
	}).
		Return(&authpb.LoginReply{
			Token:     "auth-token",
			ExpiresAt: 10,
		}, nil)

	authServer := &controllers.AuthServer{mockClients.MockAuth}

	resp, err := authServer.Login(ctx, &cloudpb.LoginRequest{
		AccessToken: "test-token",
	})

	require.NoError(t, err)
	assert.Equal(t, &cloudpb.LoginReply{
		Token:     "auth-token",
		ExpiresAt: 10,
	}, resp)
}

func TestAuthServer_LoginAPIKey(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	md := map[string][]string{
		"pixie-api-key": {"test-token"},
	}

	ctx := metadata.NewIncomingContext(context.Background(), md)
	mockClients.MockAuth.EXPECT().GetAugmentedTokenForAPIKey(gomock.Any(), &authpb.GetAugmentedTokenForAPIKeyRequest{
		APIKey: "test-token",
	}).
		Return(&authpb.GetAugmentedTokenForAPIKeyResponse{
			Token:     "auth-token",
			ExpiresAt: 10,
		}, nil)

	authServer := &controllers.AuthServer{mockClients.MockAuth}

	resp, err := authServer.Login(ctx, &cloudpb.LoginRequest{})

	require.NoError(t, err)
	assert.Equal(t, &cloudpb.LoginReply{
		Token:     "auth-token",
		ExpiresAt: 10,
	}, resp)
}

func TestAuthServer_UnAuthorized(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()

	authServer := &controllers.AuthServer{mockClients.MockAuth}

	_, err := authServer.Login(context.Background(), &cloudpb.LoginRequest{})

	require.Error(t, err)
}
