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
	"fmt"
	"os"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	bindata "github.com/golang-migrate/migrate/source/go_bindata"
	"github.com/golang/mock/gomock"
	"github.com/jmoiron/sqlx"
	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/cron_script/controllers"
	"px.dev/pixie/src/cloud/cron_script/cronscriptpb"
	"px.dev/pixie/src/cloud/cron_script/schema"
	"px.dev/pixie/src/cloud/shared/vzshard"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	mock_vzmgrpb "px.dev/pixie/src/cloud/vzmgr/vzmgrpb/mock"
	"px.dev/pixie/src/shared/cvmsgs"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/scripts"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/shared/services/pgtest"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

var db *sqlx.DB

func TestMain(m *testing.M) {
	err := testMain(m)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Got error: %v\n", err)
		os.Exit(1)
	}
	os.Exit(0)
}

func testMain(m *testing.M) error {
	viper.Set("jwt_signing_key", "key0")
	s := bindata.Resource(schema.AssetNames(), schema.Asset)
	testDB, teardown, err := pgtest.SetupTestDB(s)
	if err != nil {
		return fmt.Errorf("failed to start test database: %w", err)
	}

	defer teardown()
	db = testDB

	if c := m.Run(); c != 0 {
		return fmt.Errorf("some tests failed with code: %d", c)
	}
	return nil
}

func createTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = srvutils.GenerateJWTForUser("abcdef", "223e4567-e89b-12d3-a456-426655440000", "test@test.com", time.Now(), "pixie")
	return authcontext.NewContext(context.Background(), sCtx)
}

func mustLoadTestData(db *sqlx.DB) {
	db.MustExec(`DELETE FROM cron_scripts`)

	insertScript := `INSERT INTO cron_scripts(id, org_id, script, cluster_ids, configs, enabled, frequency_s) VALUES ($1, $2, $3, $4, PGP_SYM_ENCRYPT($5, $6), $7, $8)`

	clusterIDs1 := []uuid.UUID{
		uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440000"),
		uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440001"),
	}
	clusterIDs2 := []uuid.UUID{
		uuid.FromStringOrNil("423e4567-e89b-12d3-a456-426655440000"),
		uuid.FromStringOrNil("423e4567-e89b-12d3-a456-426655440001"),
	}
	clusterIDs3 := []uuid.UUID{
		uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440000"),
	}
	db.MustExec(insertScript, "123e4567-e89b-12d3-a456-426655440000", "223e4567-e89b-12d3-a456-426655440000", "px.display()", controllers.ClusterIDs(clusterIDs1), "testConfigYaml: abcd", "test", true, 5)
	db.MustExec(insertScript, "123e4567-e89b-12d3-a456-426655440002", "223e4567-e89b-12d3-a456-426655440000", "px()", controllers.ClusterIDs(clusterIDs3), "testConfigYaml: 1234", "test", false, 10)
	db.MustExec(insertScript, "123e4567-e89b-12d3-a456-426655440001", "223e4567-e89b-12d3-a456-426655440001", "px.stream()", controllers.ClusterIDs(clusterIDs2), "testConfigYaml2: efgh", "test", true, 10)
	db.MustExec(insertScript, "123e4567-e89b-12d3-a456-426655440003", "223e4567-e89b-12d3-a456-426655440001", "px.stream2()", controllers.ClusterIDs(clusterIDs2), "testConfigYaml2: efgh", "test", false, 10)
}

func TestServer_GetScript(t *testing.T) {
	mustLoadTestData(db)

	s := controllers.New(db, "test", nil, nil)
	resp, err := s.GetScript(createTestContext(), &cronscriptpb.GetScriptRequest{
		ID:    utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
		OrgID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cronscriptpb.GetScriptResponse{
		Script: &cronscriptpb.CronScript{
			ID:     utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
			OrgID:  utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
			Script: "px.display()",
			ClusterIDs: []*uuidpb.UUID{
				utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"),
				utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440001"),
			},
			Configs:    "testConfigYaml: abcd",
			Enabled:    true,
			FrequencyS: 5,
		},
	}, resp)
}

func TestServer_GetScripts(t *testing.T) {
	mustLoadTestData(db)

	s := controllers.New(db, "test", nil, nil)
	resp, err := s.GetScripts(createTestContext(), &cronscriptpb.GetScriptsRequest{
		IDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
			utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440002"),
		},
		OrgID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cronscriptpb.GetScriptsResponse{
		Scripts: []*cronscriptpb.CronScript{
			{
				ID:     utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440000"),
				OrgID:  utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
				Script: "px.display()",
				ClusterIDs: []*uuidpb.UUID{
					utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"),
					utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440001"),
				},
				Configs:    "testConfigYaml: abcd",
				Enabled:    true,
				FrequencyS: 5,
			},
			{
				ID:     utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440002"),
				OrgID:  utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
				Script: "px()",
				ClusterIDs: []*uuidpb.UUID{
					utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"),
				},
				Configs:    "testConfigYaml: 1234",
				Enabled:    false,
				FrequencyS: 10,
			},
		},
	}, resp)
}

func TestServer_CreateScript(t *testing.T) {
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	s := controllers.New(db, "test", nc, mockVZMgr)

	vz1ID := "323e4567-e89b-12d3-a456-426655440003"
	vz2ID := "323e4567-e89b-12d3-a456-426655440002"
	clusterIDs := []*uuidpb.UUID{
		utils.ProtoFromUUIDStrOrNil(vz1ID),
		utils.ProtoFromUUIDStrOrNil(vz2ID),
	}

	mockVZMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: clusterIDs,
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{
			{
				VizierID: utils.ProtoFromUUIDStrOrNil(vz1ID),
				Status:   cvmsgspb.VZ_ST_HEALTHY,
			},
			{
				VizierID: utils.ProtoFromUUIDStrOrNil(vz2ID),
				Status:   cvmsgspb.VZ_ST_HEALTHY,
			},
		},
	}, nil)

	var wg sync.WaitGroup
	wg.Add(2)

	expectedCronScript := &cvmsgspb.CronScript{
		Script:     "px.display()",
		Configs:    "testYAML",
		FrequencyS: 11,
	}

	mdSub1, err := nc.Subscribe(vzshard.C2VTopic(cvmsgs.CronScriptUpdatesChannel, uuid.FromStringOrNil(vz1ID)), func(msg *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(msg.Data, c2vMsg)
		require.NoError(t, err)
		req := &cvmsgspb.CronScriptUpdate{}
		err = types.UnmarshalAny(c2vMsg.Msg, req)
		require.NoError(t, err)
		assert.Equal(t, expectedCronScript.Script, req.GetUpsertReq().Script.Script)
		assert.Equal(t, expectedCronScript.Configs, req.GetUpsertReq().Script.Configs)
		assert.Equal(t, expectedCronScript.FrequencyS, req.GetUpsertReq().Script.FrequencyS)
		wg.Done()

		// Send response.
		resp := &cvmsgspb.RegisterOrUpdateCronScriptResponse{}
		v2cAnyMsg, err := types.MarshalAny(resp)
		require.NoError(t, err)
		v2cMsg := &cvmsgspb.V2CMessage{
			Msg: v2cAnyMsg,
		}
		b, err := v2cMsg.Marshal()
		require.NoError(t, err)

		err = nc.Publish(vzshard.V2CTopic(fmt.Sprintf("%s:%s", cvmsgs.CronScriptUpdatesResponseChannel, req.RequestID), uuid.FromStringOrNil(vz1ID)), b)
		require.NoError(t, err)
	})
	defer func() {
		err = mdSub1.Unsubscribe()
		require.NoError(t, err)
	}()

	mdSub2, err := nc.Subscribe(vzshard.C2VTopic(cvmsgs.CronScriptUpdatesChannel, uuid.FromStringOrNil(vz2ID)), func(msg *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(msg.Data, c2vMsg)
		require.NoError(t, err)
		req := &cvmsgspb.CronScriptUpdate{}
		err = types.UnmarshalAny(c2vMsg.Msg, req)
		require.NoError(t, err)
		wg.Done()

		// Send response.
		resp := &cvmsgspb.RegisterOrUpdateCronScriptResponse{}
		v2cAnyMsg, err := types.MarshalAny(resp)
		require.NoError(t, err)
		v2cMsg := &cvmsgspb.V2CMessage{
			Msg: v2cAnyMsg,
		}
		b, err := v2cMsg.Marshal()
		require.NoError(t, err)

		err = nc.Publish(vzshard.V2CTopic(fmt.Sprintf("%s:%s", cvmsgs.CronScriptUpdatesResponseChannel, req.RequestID), uuid.FromStringOrNil(vz2ID)), b)
		require.NoError(t, err)
	})
	defer func() {
		err = mdSub2.Unsubscribe()
		require.NoError(t, err)
	}()

	resp, err := s.CreateScript(createTestContext(), &cronscriptpb.CreateScriptRequest{
		Script:     "px.display()",
		Configs:    "testYAML",
		FrequencyS: 11,
		ClusterIDs: clusterIDs,
		OrgID:      utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	wg.Wait()
	require.NoError(t, err)
	require.NotNil(t, resp)

	id := resp.ID

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := db.Queryx(query, "test", "223e4567-e89b-12d3-a456-426655440000", utils.UUIDFromProtoOrNil(id))
	require.Nil(t, err)

	defer rows.Close()
	require.True(t, rows.Next())

	var script controllers.CronScript
	err = rows.StructScan(&script)
	require.Nil(t, err)

	assert.Equal(t, controllers.CronScript{
		ID:        utils.UUIDFromProtoOrNil(id),
		OrgID:     uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000"),
		Script:    "px.display()",
		ConfigStr: "testYAML",
		Enabled:   true,
		ClusterIDs: []uuid.UUID{
			uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440003"),
			uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440002"),
		},
		FrequencyS: 11,
	}, script)
}

func TestServer_CreateScriptDisabled(t *testing.T) {
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	s := controllers.New(db, "test", nc, mockVZMgr)

	vz1ID := "323e4567-e89b-12d3-a456-426655440003"
	vz2ID := "323e4567-e89b-12d3-a456-426655440002"
	clusterIDs := []*uuidpb.UUID{
		utils.ProtoFromUUIDStrOrNil(vz1ID),
		utils.ProtoFromUUIDStrOrNil(vz2ID),
	}

	resp, err := s.CreateScript(createTestContext(), &cronscriptpb.CreateScriptRequest{
		Script:     "px.display()",
		Configs:    "testYAML",
		FrequencyS: 11,
		ClusterIDs: clusterIDs,
		Disabled:   true,
		OrgID:      utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	require.NoError(t, err)
	require.NotNil(t, resp)

	id := resp.ID

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := db.Queryx(query, "test", "223e4567-e89b-12d3-a456-426655440000", utils.UUIDFromProtoOrNil(id))
	require.Nil(t, err)

	defer rows.Close()
	require.True(t, rows.Next())

	var script controllers.CronScript
	err = rows.StructScan(&script)
	require.Nil(t, err)

	assert.Equal(t, controllers.CronScript{
		ID:        utils.UUIDFromProtoOrNil(id),
		OrgID:     uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000"),
		Script:    "px.display()",
		ConfigStr: "testYAML",
		Enabled:   false,
		ClusterIDs: []uuid.UUID{
			uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440003"),
			uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440002"),
		},
		FrequencyS: 11,
	}, script)
}

func sendUpdateAndWaitForResponse(t *testing.T, nc *nats.Conn, vzID string, wg *sync.WaitGroup, isUpdate bool) func() {
	mdSub, err := nc.Subscribe(vzshard.C2VTopic(cvmsgs.CronScriptUpdatesChannel, uuid.FromStringOrNil(vzID)), func(msg *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(msg.Data, c2vMsg)
		require.NoError(t, err)
		req := &cvmsgspb.CronScriptUpdate{}
		err = types.UnmarshalAny(c2vMsg.Msg, req)
		require.NoError(t, err)
		wg.Done()
		// Send response.
		var v2cMsg *cvmsgspb.V2CMessage
		if isUpdate {
			resp := &cvmsgspb.RegisterOrUpdateCronScriptResponse{}
			v2cAnyMsg, err := types.MarshalAny(resp)
			require.NoError(t, err)
			v2cMsg = &cvmsgspb.V2CMessage{
				Msg: v2cAnyMsg,
			}
			assert.NotNil(t, req.GetUpsertReq())
		} else {
			resp2 := &cvmsgspb.DeleteCronScriptResponse{}
			v2cAnyMsg, err := types.MarshalAny(resp2)
			require.NoError(t, err)
			v2cMsg = &cvmsgspb.V2CMessage{
				Msg: v2cAnyMsg,
			}
			assert.NotNil(t, req.GetDeleteReq())
		}
		b, err := v2cMsg.Marshal()
		require.NoError(t, err)

		err = nc.Publish(vzshard.V2CTopic(fmt.Sprintf("%s:%s", cvmsgs.CronScriptUpdatesResponseChannel, req.RequestID), uuid.FromStringOrNil(vzID)), b)
		require.NoError(t, err)
	})
	require.NoError(t, err)
	return func() {
		err = mdSub.Unsubscribe()
		require.NoError(t, err)
	}
}

func TestServer_UpdateScript(t *testing.T) {
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	s := controllers.New(db, "test", nc, mockVZMgr)

	clusterIDs := []*uuidpb.UUID{
		utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440003"),
		utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440002"),
	}

	mockVZMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: clusterIDs,
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{
			{
				VizierID: utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440003"),
				Status:   cvmsgspb.VZ_ST_HEALTHY,
			},
			{
				VizierID: utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440002"),
				Status:   cvmsgspb.VZ_ST_HEALTHY,
			},
		},
	}, nil)

	mockVZMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: []*uuidpb.UUID{
			utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"),
		},
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{
			{
				VizierID: utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000"),
				Status:   cvmsgspb.VZ_ST_HEALTHY,
			},
		},
	}, nil)

	var wg sync.WaitGroup
	wg.Add(3)

	cleanup1 := sendUpdateAndWaitForResponse(t, nc, "323e4567-e89b-12d3-a456-426655440003", &wg, true)
	defer cleanup1()
	cleanup2 := sendUpdateAndWaitForResponse(t, nc, "323e4567-e89b-12d3-a456-426655440002", &wg, true)
	defer cleanup2()
	cleanup3 := sendUpdateAndWaitForResponse(t, nc, "323e4567-e89b-12d3-a456-426655440000", &wg, false)
	defer cleanup3()

	resp, err := s.UpdateScript(createTestContext(), &cronscriptpb.UpdateScriptRequest{
		Script:     &types.StringValue{Value: "px.updatedScript()"},
		Configs:    &types.StringValue{Value: "updatedYAML"},
		ClusterIDs: &cronscriptpb.ClusterIDs{Value: clusterIDs},
		ScriptId:   utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440002"),
		Enabled:    &types.BoolValue{Value: true},
		OrgID:      utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	wg.Wait()
	require.NoError(t, err)
	require.NotNil(t, resp)

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := db.Queryx(query, "test", "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440002")
	require.Nil(t, err)

	defer rows.Close()
	require.True(t, rows.Next())

	var script controllers.CronScript
	err = rows.StructScan(&script)
	require.Nil(t, err)

	assert.Equal(t, controllers.CronScript{
		ID:        uuid.FromStringOrNil("123e4567-e89b-12d3-a456-426655440002"),
		OrgID:     uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000"),
		Script:    "px.updatedScript()",
		ConfigStr: "updatedYAML",
		Enabled:   true,
		ClusterIDs: []uuid.UUID{
			uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440003"),
			uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440002"),
		},
		FrequencyS: 10,
	}, script)
}

func TestServer_DeleteScript(t *testing.T) {
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)

	vzIDpb := utils.ProtoFromUUIDStrOrNil("323e4567-e89b-12d3-a456-426655440000")
	scriptIDpb := utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440002")
	mockVZMgr.EXPECT().GetVizierInfos(gomock.Any(), &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: []*uuidpb.UUID{
			vzIDpb,
		},
	}).Return(&vzmgrpb.GetVizierInfosResponse{
		VizierInfos: []*cvmsgspb.VizierInfo{
			{
				VizierID: vzIDpb,
				Status:   cvmsgspb.VZ_ST_HEALTHY,
			},
		},
	}, nil)

	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	s := controllers.New(db, "test", nc, mockVZMgr)

	var wg sync.WaitGroup
	wg.Add(1)

	mdSub, err := nc.Subscribe(vzshard.C2VTopic(cvmsgs.CronScriptUpdatesChannel, uuid.FromStringOrNil("323e4567-e89b-12d3-a456-426655440000")), func(msg *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(msg.Data, c2vMsg)
		require.NoError(t, err)
		req := &cvmsgspb.CronScriptUpdate{}
		err = types.UnmarshalAny(c2vMsg.Msg, req)
		require.NoError(t, err)
		assert.Equal(t, req.GetDeleteReq().ScriptID, scriptIDpb)
		wg.Done()
	})
	defer func() {
		err = mdSub.Unsubscribe()
		require.NoError(t, err)
	}()

	resp, err := s.DeleteScript(createTestContext(), &cronscriptpb.DeleteScriptRequest{
		ID:    scriptIDpb,
		OrgID: utils.ProtoFromUUIDStrOrNil("223e4567-e89b-12d3-a456-426655440000"),
	})
	require.NoError(t, err)
	require.NotNil(t, resp)

	assert.Equal(t, &cronscriptpb.DeleteScriptResponse{}, resp)
	wg.Wait()

	query := `SELECT id, org_id, script, cluster_ids, PGP_SYM_DECRYPT(configs, $1::text) as configs, enabled, frequency_s FROM cron_scripts WHERE org_id=$2 AND id=$3`
	rows, err := db.Queryx(query, "test", "223e4567-e89b-12d3-a456-426655440000", "123e4567-e89b-12d3-a456-426655440002")
	require.Nil(t, err)

	defer rows.Close()
	require.False(t, rows.Next())
}

func TestServer_HandleChecksumRequest(t *testing.T) {
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)

	vzID := "423e4567-e89b-12d3-a456-426655440001"
	orgID := "223e4567-e89b-12d3-a456-426655440001"

	mockVZMgr.EXPECT().GetOrgFromVizier(gomock.Any(), utils.ProtoFromUUIDStrOrNil(vzID)).Return(&vzmgrpb.GetOrgFromVizierResponse{
		OrgID: utils.ProtoFromUUIDStrOrNil(orgID)}, nil)

	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	s := controllers.New(db, "test", nc, mockVZMgr)

	req := &cvmsgspb.GetCronScriptsChecksumRequest{
		Topic: "test",
	}
	anyMsg, err := types.MarshalAny(req)
	require.NoError(t, err)
	v2cMsg := &cvmsgspb.V2CMessage{
		Msg:      anyMsg,
		VizierID: vzID,
	}

	var wg sync.WaitGroup
	wg.Add(1)

	csMap := map[string]*cvmsgspb.CronScript{
		"123e4567-e89b-12d3-a456-426655440001": {
			ID:         utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
			Script:     "px.stream()",
			FrequencyS: 10,
			Configs:    "testConfigYaml2: efgh",
		},
	}
	mdSub, err := nc.Subscribe(vzshard.C2VTopic(fmt.Sprintf("%s:%s", cvmsgs.CronScriptChecksumResponseChannel, "test"), uuid.FromStringOrNil(vzID)), func(msg *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(msg.Data, c2vMsg)
		require.NoError(t, err)
		req := &cvmsgspb.GetCronScriptsChecksumResponse{}
		err = types.UnmarshalAny(c2vMsg.Msg, req)
		require.NoError(t, err)
		expectedChecksum, err := scripts.ChecksumFromScriptMap(csMap)
		require.NoError(t, err)
		assert.Equal(t, expectedChecksum, req.Checksum)
		wg.Done()
	})
	defer func() {
		err = mdSub.Unsubscribe()
		require.NoError(t, err)
	}()

	s.HandleChecksumRequest(v2cMsg)
	wg.Wait()
}

func TestServer_HandleGetScriptsRequest(t *testing.T) {
	mustLoadTestData(db)

	ctrl := gomock.NewController(t)
	mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)

	vzID := "423e4567-e89b-12d3-a456-426655440001"
	orgID := "223e4567-e89b-12d3-a456-426655440001"

	mockVZMgr.EXPECT().GetOrgFromVizier(gomock.Any(), utils.ProtoFromUUIDStrOrNil(vzID)).Return(&vzmgrpb.GetOrgFromVizierResponse{
		OrgID: utils.ProtoFromUUIDStrOrNil(orgID)}, nil)

	nc, natsCleanup := testingutils.MustStartTestNATS(t)
	defer natsCleanup()

	s := controllers.New(db, "test", nc, mockVZMgr)

	req := &cvmsgspb.GetCronScriptsRequest{
		Topic: "test",
	}
	anyMsg, err := types.MarshalAny(req)
	require.NoError(t, err)
	v2cMsg := &cvmsgspb.V2CMessage{
		Msg:      anyMsg,
		VizierID: vzID,
	}

	var wg sync.WaitGroup
	wg.Add(1)

	csMap := map[string]*cvmsgspb.CronScript{
		"123e4567-e89b-12d3-a456-426655440001": {
			ID:         utils.ProtoFromUUIDStrOrNil("123e4567-e89b-12d3-a456-426655440001"),
			Script:     "px.stream()",
			FrequencyS: 10,
			Configs:    "testConfigYaml2: efgh",
		},
	}
	mdSub, err := nc.Subscribe(vzshard.C2VTopic(fmt.Sprintf("%s:%s", cvmsgs.GetCronScriptsResponseChannel, "test"), uuid.FromStringOrNil(vzID)), func(msg *nats.Msg) {
		c2vMsg := &cvmsgspb.C2VMessage{}
		err := proto.Unmarshal(msg.Data, c2vMsg)
		require.NoError(t, err)
		req := &cvmsgspb.GetCronScriptsResponse{}
		err = types.UnmarshalAny(c2vMsg.Msg, req)
		require.NoError(t, err)
		require.NotNil(t, req.Scripts)
		assert.Equal(t, csMap, req.Scripts)
		wg.Done()
	})
	defer func() {
		err = mdSub.Unsubscribe()
		require.NoError(t, err)
	}()

	s.HandleScriptsRequest(v2cMsg)
	wg.Wait()
}
