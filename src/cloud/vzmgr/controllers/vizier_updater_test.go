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
	"sync"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	mock_artifacttrackerpb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb/mock"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzmgr/controllers"
	"px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/shared/cvmsgspb"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func setUpUpdater(t *testing.T) (*controllers.Updater, *nats.Conn, *sqlx.DB, *mock_artifacttrackerpb.MockArtifactTrackerClient, func()) {
	viper.Set("jwt_signing_key", "jwtkey")

	mustLoadTestData(db)

	ctrl := gomock.NewController(t)

	nc, natsCleanup := testingutils.MustStartTestNATS(t)

	mockArtifactTrackerClient := mock_artifacttrackerpb.NewMockArtifactTrackerClient(ctrl)
	atReq := &artifacttrackerpb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}
	mockArtifactTrackerClient.EXPECT().GetArtifactList(
		gomock.Any(), atReq).Return(&versionspb.ArtifactSet{
		Name: "vizier",
		Artifact: []*versionspb.Artifact{{
			VersionStr: "0.4.1",
		}},
	}, nil).AnyTimes()

	updater, _ := controllers.NewUpdater(db, mockArtifactTrackerClient, nc)

	cleanup := func() {
		ctrl.Finish()
		natsCleanup()
	}

	return updater, nc, db, mockArtifactTrackerClient, cleanup
}

func TestUpdater_UpdateOrInstallVizier(t *testing.T) {
	updater, nc, _, mockArtifactTrackerClient, cleanup := setUpUpdater(t)
	defer cleanup()
	viper.Set("domain_name", "withpixie.ai")

	vizierID, _ := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")

	go func() {
		_, err := nc.Subscribe("c2v.123e4567-e89b-12d3-a456-426655440001.VizierUpdate", func(m *nats.Msg) {
			c2vMsg := &cvmsgspb.C2VMessage{}
			err := proto.Unmarshal(m.Data, c2vMsg)
			require.NoError(t, err)
			resp := &cvmsgspb.UpdateOrInstallVizierRequest{}
			err = types.UnmarshalAny(c2vMsg.Msg, resp)
			require.NoError(t, err)
			assert.Equal(t, "123", resp.Version)
			assert.NotNil(t, resp.Token)
			token, err := srvutils.ParseToken(resp.Token, "jwtkey", "withpixie.ai")
			require.NoError(t, err)
			assert.Equal(t, []string{"cluster"}, srvutils.GetScopes(token))
			// Send response.
			updateResp := &cvmsgspb.UpdateOrInstallVizierResponse{
				UpdateStarted: true,
			}
			respAnyMsg, err := types.MarshalAny(updateResp)
			require.NoError(t, err)
			wrappedMsg := &cvmsgspb.V2CMessage{
				VizierID: vizierID.String(),
				Msg:      respAnyMsg,
			}

			b, err := wrappedMsg.Marshal()
			require.NoError(t, err)
			topic := vzshard.V2CTopic("VizierUpdateResponse", vizierID)
			err = nc.Publish(topic, b)
			require.NoError(t, err)
		})
		require.NoError(t, err)
	}()

	mockArtifactTrackerClient.EXPECT().GetDownloadLink(
		gomock.Any(), &artifacttrackerpb.GetDownloadLinkRequest{
			ArtifactName: "vizier",
			VersionStr:   "123",
			ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
		}).Return(&artifacttrackerpb.GetDownloadLinkResponse{}, nil)

	_, err := updater.UpdateOrInstallVizier(vizierID, "123", false)
	require.NoError(t, err)
}

func TestUpdater_VersionUpToDate(t *testing.T) {
	updater, _, _, _, cleanup := setUpUpdater(t)
	defer cleanup()

	assert.True(t, updater.VersionUpToDate("0.4.1"))
	assert.True(t, updater.VersionUpToDate("0.4.2-pre-rc1"))
	assert.True(t, updater.VersionUpToDate("0.4.1+meta.is.good"))
	assert.True(t, updater.VersionUpToDate("0.4.1+meta.is...bad"))
	assert.False(t, updater.VersionUpToDate("0.3.1"))
	assert.False(t, updater.VersionUpToDate("0.3.1+meta.is.good"))
	assert.False(t, updater.VersionUpToDate("0.3.1+meta.is...bad"))
	assert.True(t, updater.VersionUpToDate("0.0.0-dev+Modified.0000000.19700101000000.0"))
}

func TestUpdater_AddToUpdateQueue(t *testing.T) {
	updater, _, _, _, cleanup := setUpUpdater(t)
	defer cleanup()

	id1 := uuid.Must(uuid.NewV4())
	id2 := uuid.Must(uuid.NewV4())

	assert.True(t, updater.AddToUpdateQueue(id1))
	assert.True(t, updater.AddToUpdateQueue(id2))
	assert.False(t, updater.AddToUpdateQueue(id1))
	assert.False(t, updater.AddToUpdateQueue(id2))
}

func TestUpdater_AddToUpdateQueueNoDeadlock(t *testing.T) {
	updater, nc, _, _, cleanup := setUpUpdater(t)
	defer cleanup()

	// Fill the update queue.
	for i := 0; i < 32; i++ {
		id := uuid.Must(uuid.NewV4())
		assert.True(t, updater.AddToUpdateQueue(id))
	}
	// Update queue no longer accepts updates.s
	for i := 0; i < 32; i++ {
		id := uuid.Must(uuid.NewV4())
		assert.False(t, updater.AddToUpdateQueue(id))
	}

	// Drain the update queue.
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		_, err := nc.Subscribe("c2v.*.VizierUpdate", func(m *nats.Msg) {
			wg.Done()
		})
		require.NoError(t, err)
	}()

	go updater.ProcessUpdateQueue()

	// Wait for one item to be processed.
	wg.Wait()

	// We should be able to add another update to the queue.
	id := uuid.Must(uuid.NewV4())
	assert.True(t, updater.AddToUpdateQueue(id))

	updater.Stop()
}

func TestUpdater_ProcessUpdateQueue(t *testing.T) {
	updater, nc, _, _, cleanup := setUpUpdater(t)
	defer cleanup()
	vizierID, _ := uuid.FromString("123e4567-e89b-12d3-a456-426655440001")
	viper.Set("domain_name", "withpixie.ai")

	assert.True(t, updater.AddToUpdateQueue(vizierID))

	var wg sync.WaitGroup
	wg.Add(1)
	defer func() {
		wg.Wait()
		updater.Stop()
	}()

	go func() {
		_, err := nc.Subscribe("c2v.123e4567-e89b-12d3-a456-426655440001.VizierUpdate", func(m *nats.Msg) {
			c2vMsg := &cvmsgspb.C2VMessage{}
			err := proto.Unmarshal(m.Data, c2vMsg)
			require.NoError(t, err)
			resp := &cvmsgspb.UpdateOrInstallVizierRequest{}
			err = types.UnmarshalAny(c2vMsg.Msg, resp)
			require.NoError(t, err)
			assert.Equal(t, "0.4.1", resp.Version)
			assert.NotNil(t, resp.Token)
			token, err := srvutils.ParseToken(resp.Token, "jwtkey", "withpixie.ai")
			require.NoError(t, err)
			assert.Equal(t, []string{"cluster"}, srvutils.GetScopes(token))
			// Send response.
			updateResp := &cvmsgspb.UpdateOrInstallVizierResponse{
				UpdateStarted: true,
			}
			respAnyMsg, err := types.MarshalAny(updateResp)
			require.NoError(t, err)
			wrappedMsg := &cvmsgspb.V2CMessage{
				VizierID: vizierID.String(),
				Msg:      respAnyMsg,
			}

			b, err := wrappedMsg.Marshal()
			require.NoError(t, err)
			topic := vzshard.V2CTopic("VizierUpdateResponse", vizierID)
			err = nc.Publish(topic, b)
			require.NoError(t, err)
			wg.Done()
		})
		require.NoError(t, err)
	}()

	go updater.ProcessUpdateQueue()
}
