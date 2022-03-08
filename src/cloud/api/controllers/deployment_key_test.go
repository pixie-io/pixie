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
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controllers"
	"px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/utils"
)

func TestVizierDeploymentKeyServer_Create(t *testing.T) {
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

			vzreq := &vzmgrpb.CreateDeploymentKeyRequest{
				OrgID:  utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				UserID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
				Desc:   "test key",
			}
			vzresp := &vzmgrpb.DeploymentKey{
				ID:        utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				OrgID:     utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
				Key:       "foobar",
				CreatedAt: types.TimestampNow(),
			}
			mockClients.MockVzDeployKey.EXPECT().
				Create(gomock.Any(), vzreq).Return(vzresp, nil)

			vzDeployKeyServer := &controllers.VizierDeploymentKeyServer{
				VzDeploymentKey: mockClients.MockVzDeployKey,
			}

			resp, err := vzDeployKeyServer.Create(ctx, &cloudpb.CreateDeploymentKeyRequest{Desc: "test key"})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, resp.ID, vzresp.ID)
			assert.Equal(t, resp.Key, vzresp.Key)
			assert.Equal(t, resp.CreatedAt, vzresp.CreatedAt)
		})
	}
}

func TestVizierDeploymentKeyServer_List(t *testing.T) {
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

			vzreq := &vzmgrpb.ListDeploymentKeyRequest{
				OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			}
			vzresp := &vzmgrpb.ListDeploymentKeyResponse{
				Keys: []*vzmgrpb.DeploymentKeyMetadata{
					{
						ID:        utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430d3"),
						OrgID:     utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
						CreatedAt: types.TimestampNow(),
						Desc:      "this is a key",
					},
				},
			}
			mockClients.MockVzDeployKey.EXPECT().
				List(gomock.Any(), vzreq).Return(vzresp, nil)

			vzDeployKeyServer := &controllers.VizierDeploymentKeyServer{
				VzDeploymentKey: mockClients.MockVzDeployKey,
			}

			resp, err := vzDeployKeyServer.List(ctx, &cloudpb.ListDeploymentKeyRequest{})
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

func TestVizierDeploymentKeyServer_Get(t *testing.T) {
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

			id := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")
			vzreq := &vzmgrpb.GetDeploymentKeyRequest{
				ID:    id,
				OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
			}
			vzresp := &vzmgrpb.GetDeploymentKeyResponse{
				Key: &vzmgrpb.DeploymentKey{
					ID:        id,
					Key:       "foobar",
					CreatedAt: types.TimestampNow(),
					Desc:      "this is a key",
				},
			}
			mockClients.MockVzDeployKey.EXPECT().
				Get(gomock.Any(), vzreq).Return(vzresp, nil)

			vzDeployKeyServer := &controllers.VizierDeploymentKeyServer{
				VzDeploymentKey: mockClients.MockVzDeployKey,
			}
			resp, err := vzDeployKeyServer.Get(ctx, &cloudpb.GetDeploymentKeyRequest{
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

func TestVizierDeploymentKeyServer_Delete(t *testing.T) {
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

			id := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")
			vzresp := &types.Empty{}
			mockClients.MockVzDeployKey.EXPECT().
				Delete(gomock.Any(), &vzmgrpb.DeleteDeploymentKeyRequest{
					ID:    id,
					OrgID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				}).Return(vzresp, nil)

			vzDeployKeyServer := &controllers.VizierDeploymentKeyServer{
				VzDeploymentKey: mockClients.MockVzDeployKey,
			}
			resp, err := vzDeployKeyServer.Delete(ctx, id)
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, resp, vzresp)
		})
	}
}

func TestVizierDeploymentKeyServer_LookupDeploymentKeyAuthorized(t *testing.T) {
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

			vzresp := &vzmgrpb.LookupDeploymentKeyResponse{
				Key: &vzmgrpb.DeploymentKey{
					Key:    "abc",
					UserID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
					OrgID:  utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				},
			}
			mockClients.MockVzDeployKey.EXPECT().
				LookupDeploymentKey(gomock.Any(), &vzmgrpb.LookupDeploymentKeyRequest{
					Key: "abc",
				}).Return(vzresp, nil)

			vzDeployKeyServer := &controllers.VizierDeploymentKeyServer{
				VzDeploymentKey: mockClients.MockVzDeployKey,
			}
			resp, err := vzDeployKeyServer.LookupDeploymentKey(ctx, &cloudpb.LookupDeploymentKeyRequest{
				Key: "abc",
			})
			require.NoError(t, err)
			assert.NotNil(t, resp)
			assert.Equal(t, resp.Key.Key, vzresp.Key.Key)
			assert.Equal(t, resp.Key.UserID, vzresp.Key.UserID)
			assert.Equal(t, resp.Key.OrgID, vzresp.Key.OrgID)
		})
	}
}

func TestVizierDeploymentKeyServer_LookupDeploymentKeyUnauthorized(t *testing.T) {
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

			vzresp := &vzmgrpb.LookupDeploymentKeyResponse{
				Key: &vzmgrpb.DeploymentKey{
					Key:    "abc",
					UserID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
					OrgID:  utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd43000"),
				},
			}
			mockClients.MockVzDeployKey.EXPECT().
				LookupDeploymentKey(gomock.Any(), &vzmgrpb.LookupDeploymentKeyRequest{
					Key: "abc",
				}).Return(vzresp, nil)

			vzDeployKeyServer := &controllers.VizierDeploymentKeyServer{
				VzDeploymentKey: mockClients.MockVzDeployKey,
			}
			resp, err := vzDeployKeyServer.LookupDeploymentKey(ctx, &cloudpb.LookupDeploymentKeyRequest{
				Key: "abc",
			})
			assert.Nil(t, resp)
			assert.NotNil(t, err)
			assert.Equal(t, codes.NotFound, status.Code(err))
		})
	}
}
