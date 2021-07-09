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

package controller_test

import (
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controller"
	"px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/utils"
)

func TestVizierDeploymentKeyServer_Create(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzreq := &vzmgrpb.CreateDeploymentKeyRequest{Desc: "test key"}
	vzresp := &vzmgrpb.DeploymentKey{
		ID:        utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		Key:       "foobar",
		CreatedAt: types.TimestampNow(),
	}
	mockClients.MockVzDeployKey.EXPECT().
		Create(gomock.Any(), vzreq).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}

	resp, err := vzDeployKeyServer.Create(ctx, &cloudpb.CreateDeploymentKeyRequest{Desc: "test key"})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp.ID, vzresp.ID)
	assert.Equal(t, resp.Key, vzresp.Key)
	assert.Equal(t, resp.CreatedAt, vzresp.CreatedAt)
}

func TestVizierDeploymentKeyServer_List(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzreq := &vzmgrpb.ListDeploymentKeyRequest{}
	vzresp := &vzmgrpb.ListDeploymentKeyResponse{
		Keys: []*vzmgrpb.DeploymentKey{
			{
				ID:        utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8"),
				Key:       "foobar",
				CreatedAt: types.TimestampNow(),
				Desc:      "this is a key",
			},
		},
	}
	mockClients.MockVzDeployKey.EXPECT().
		List(gomock.Any(), vzreq).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}

	resp, err := vzDeployKeyServer.List(ctx, &cloudpb.ListDeploymentKeyRequest{})
	require.NoError(t, err)
	assert.NotNil(t, resp)
	for i, key := range resp.Keys {
		assert.Equal(t, key.ID, vzresp.Keys[i].ID)
		assert.Equal(t, key.Key, vzresp.Keys[i].Key)
		assert.Equal(t, key.CreatedAt, vzresp.Keys[i].CreatedAt)
		assert.Equal(t, key.Desc, vzresp.Keys[i].Desc)
	}
}

func TestVizierDeploymentKeyServer_Get(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	vzreq := &vzmgrpb.GetDeploymentKeyRequest{
		ID: id,
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

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
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
}

func TestVizierDeploymentKeyServer_Delete(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	_, mockClients, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	id := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	vzresp := &types.Empty{}
	mockClients.MockVzDeployKey.EXPECT().
		Delete(gomock.Any(), id).Return(vzresp, nil)

	vzDeployKeyServer := &controller.VizierDeploymentKeyServer{
		VzDeploymentKey: mockClients.MockVzDeployKey,
	}
	resp, err := vzDeployKeyServer.Delete(ctx, id)
	require.NoError(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, resp, vzresp)
}
