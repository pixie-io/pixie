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

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/utils"
)

func TestAPIKeyServer_Create(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			vzreq := &authpb.CreateAPIKeyRequest{Desc: "test key"}
			vzresp := &authpb.APIKey{
				ID:        utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Key:       "foobar",
				CreatedAt: types.TimestampNow(),
			}
			mockClients.MockAPIKey.EXPECT().
				Create(gomock.Any(), vzreq).Return(vzresp, nil)

			vzAPIKeyServer := &controllers.APIKeyServer{
				APIKeyClient: mockClients.MockAPIKey,
			}

			resp, err := vzAPIKeyServer.Create(ctx, &cloudpb.CreateAPIKeyRequest{Desc: "test key"})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, resp.ID, vzresp.ID)
			assert.Equal(t, resp.Key, vzresp.Key)
			assert.Equal(t, resp.CreatedAt, vzresp.CreatedAt)
		})
	}
}

func TestAPIKeyServer_List(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()
			ctx := test.ctx

			vzreq := &authpb.ListAPIKeyRequest{}
			vzresp := &authpb.ListAPIKeyResponse{
				Keys: []*authpb.APIKeyMetadata{
					{
						ID:        utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
						CreatedAt: types.TimestampNow(),
						Desc:      "this is a key",
					},
				},
			}
			mockClients.MockAPIKey.EXPECT().
				List(gomock.Any(), vzreq).Return(vzresp, nil)

			vzAPIKeyServer := &controllers.APIKeyServer{
				APIKeyClient: mockClients.MockAPIKey,
			}

			resp, err := vzAPIKeyServer.List(ctx, &cloudpb.ListAPIKeyRequest{})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			for i, key := range resp.Keys {
				assert.Equal(t, key.ID, vzresp.Keys[i].ID)
				assert.Equal(t, key.CreatedAt, vzresp.Keys[i].CreatedAt)
				assert.Equal(t, key.Desc, vzresp.Keys[i].Desc)
			}
		})
	}
}

func TestAPIKeyServer_Get(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()

			id := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			vzreq := &authpb.GetAPIKeyRequest{
				ID: id,
			}
			vzresp := &authpb.GetAPIKeyResponse{
				Key: &authpb.APIKey{
					ID:        id,
					Key:       "foobar",
					CreatedAt: types.TimestampNow(),
					Desc:      "this is a key",
				},
			}
			mockClients.MockAPIKey.EXPECT().
				Get(gomock.Any(), vzreq).Return(vzresp, nil)

			vzAPIKeyServer := &controllers.APIKeyServer{
				APIKeyClient: mockClients.MockAPIKey,
			}
			resp, err := vzAPIKeyServer.Get(test.ctx, &cloudpb.GetAPIKeyRequest{
				ID: id,
			})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, resp.Key.ID, vzresp.Key.ID)
			assert.Equal(t, resp.Key.Key, vzresp.Key.Key)
			assert.Equal(t, resp.Key.CreatedAt, vzresp.Key.CreatedAt)
			assert.Equal(t, resp.Key.Desc, vzresp.Key.Desc)
		})
	}
}

func TestAPIKeyServer_Delete(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
			defer cleanup()

			id := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			vzresp := &types.Empty{}
			mockClients.MockAPIKey.EXPECT().
				Delete(gomock.Any(), id).Return(vzresp, nil)

			vzAPIKeyServer := &controllers.APIKeyServer{
				APIKeyClient: mockClients.MockAPIKey,
			}
			resp, err := vzAPIKeyServer.Delete(test.ctx, id)
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, resp, vzresp)
		})
	}
}
