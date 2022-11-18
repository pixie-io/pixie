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
	"fmt"
	"net"
	"sort"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sync/errgroup"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	grpc_metadata "google.golang.org/grpc/metadata"
	"google.golang.org/grpc/test/bufconn"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/dynamic_tracing/ir/logicalpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/shared/bloomfilterpb"

	sharedmetadatapb "px.dev/pixie/src/shared/metadatapb"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/shared/types/typespb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/controllers"
	mock_agent "px.dev/pixie/src/vizier/services/metadata/controllers/agent/mock"
	"px.dev/pixie/src/vizier/services/metadata/controllers/testutils"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
	mock_tracepoint "px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint/mock"
	"px.dev/pixie/src/vizier/services/metadata/metadataenv"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
)

func testTableInfos() []*storepb.TableInfo {
	tableInfos := make([]*storepb.TableInfo, 2)

	schema1Cols := make([]*storepb.TableInfo_ColumnInfo, 3)
	schema1Cols[0] = &storepb.TableInfo_ColumnInfo{
		Name:     "t1Col1",
		DataType: 2,
	}
	schema1Cols[1] = &storepb.TableInfo_ColumnInfo{
		Name:     "t1Col2",
		DataType: 1,
	}
	schema1Cols[2] = &storepb.TableInfo_ColumnInfo{
		Name:     "t1Col3",
		DataType: 3,
	}
	tableInfos[0] = &storepb.TableInfo{
		Name:    "table1",
		Columns: schema1Cols,
		Desc:    "table 1 desc",
	}

	schema2Cols := make([]*storepb.TableInfo_ColumnInfo, 2)
	schema2Cols[0] = &storepb.TableInfo_ColumnInfo{
		Name:     "t2Col1",
		DataType: 1,
	}
	schema2Cols[1] = &storepb.TableInfo_ColumnInfo{
		Name:     "t2Col2",
		DataType: 3,
	}
	tableInfos[1] = &storepb.TableInfo{
		Name:    "table2",
		Columns: schema2Cols,
		Desc:    "table 2 desc",
	}
	return tableInfos
}

func TestGetAgentInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)

	agent1IDStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u1, err := uuid.FromString(agent1IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u1pb := utils.ProtoFromUUID(u1)

	agent2IDStr := "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u2, err := uuid.FromString(agent2IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2pb := utils.ProtoFromUUID(u2)

	now := time.Now()

	agents := []*agentpb.Agent{
		{
			// Push the heartbeat into the past (past the UnhealthyAgentThreshold) to make this look unhealthy!
			LastHeartbeatNS: now.Add(-controllers.UnhealthyAgentThreshold).UnixNano(),
			CreateTimeNS:    5,
			Info: &agentpb.AgentInfo{
				AgentID: u1pb,
				HostInfo: &agentpb.HostInfo{
					Hostname: "test_host",
					HostIP:   "127.0.0.1",
				},
			},
			ASID: 123,
		},
		{
			// Push the heartbeat into the future to make this look healthy!
			LastHeartbeatNS: now.Add(10 * controllers.UnhealthyAgentThreshold).UnixNano(),
			CreateTimeNS:    0,
			Info: &agentpb.AgentInfo{
				AgentID: u2pb,
				HostInfo: &agentpb.HostInfo{
					Hostname: "another_host",
					HostIP:   "127.0.0.1",
				},
			},
			ASID: 456,
		},
	}

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return(agents, nil)

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, nil)

	req := metadatapb.AgentInfoRequest{}

	resp, err := s.GetAgentInfo(context.Background(), &req)
	require.NoError(t, err)

	assert.Equal(t, 2, len(resp.Info))

	agentResp := new(metadatapb.AgentMetadata)
	if err := proto.UnmarshalText(testutils.Agent1StatusPB, agentResp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentResp.Status.State = agentpb.AGENT_STATE_UNRESPONSIVE
	assert.Greater(t, resp.Info[0].Status.NSSinceLastHeartbeat, controllers.UnhealthyAgentThreshold.Nanoseconds())
	resp.Info[0].Status.NSSinceLastHeartbeat = 0
	resp.Info[0].Agent.LastHeartbeatNS = 0
	assert.Equal(t, agentResp, resp.Info[0])

	agentResp = new(metadatapb.AgentMetadata)
	if err = proto.UnmarshalText(testutils.Agent2StatusPB, agentResp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Less(t, resp.Info[0].Status.NSSinceLastHeartbeat, controllers.UnhealthyAgentThreshold.Nanoseconds())
	resp.Info[1].Status.NSSinceLastHeartbeat = 0
	resp.Info[1].Agent.LastHeartbeatNS = 0
	assert.Equal(t, agentResp, resp.Info[1])
}

func TestGetAgentInfoGetActiveAgentsFailed(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return(nil, errors.New("could not get active agents"))

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, nil)

	req := metadatapb.AgentInfoRequest{}

	resp, err := s.GetAgentInfo(context.Background(), &req)

	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestGetSchemas(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)

	tableInfos := testTableInfos()

	mockAgtMgr.
		EXPECT().
		GetComputedSchema().
		Return(&storepb.ComputedSchema{Tables: tableInfos}, nil)

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, nil)

	req := metadatapb.SchemaRequest{}

	resp, err := s.GetSchemas(context.Background(), &req)

	require.NoError(t, err)
	assert.NotNil(t, resp)

	assert.Equal(t, 2, len(resp.Schema.RelationMap))
	assert.Equal(t, "table 1 desc", resp.Schema.RelationMap["table1"].Desc)
	assert.Equal(t, 3, len(resp.Schema.RelationMap["table1"].Columns))
	assert.Equal(t, "t1Col1", resp.Schema.RelationMap["table1"].Columns[0].ColumnName)
	assert.Equal(t, typespb.INT64, resp.Schema.RelationMap["table1"].Columns[0].ColumnType)
	assert.Equal(t, "t1Col2", resp.Schema.RelationMap["table1"].Columns[1].ColumnName)
	assert.Equal(t, typespb.BOOLEAN, resp.Schema.RelationMap["table1"].Columns[1].ColumnType)
	assert.Equal(t, "t1Col3", resp.Schema.RelationMap["table1"].Columns[2].ColumnName)
	assert.Equal(t, typespb.UINT128, resp.Schema.RelationMap["table1"].Columns[2].ColumnType)

	assert.Equal(t, "table 2 desc", resp.Schema.RelationMap["table2"].Desc)
	assert.Equal(t, 2, len(resp.Schema.RelationMap["table2"].Columns))
	assert.Equal(t, "t2Col1", resp.Schema.RelationMap["table2"].Columns[0].ColumnName)
	assert.Equal(t, typespb.BOOLEAN, resp.Schema.RelationMap["table2"].Columns[0].ColumnType)
	assert.Equal(t, "t2Col2", resp.Schema.RelationMap["table2"].Columns[1].ColumnName)
	assert.Equal(t, typespb.UINT128, resp.Schema.RelationMap["table2"].Columns[1].ColumnType)
}

func Test_Server_RegisterTracepoint(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)

	program := &logicalpb.TracepointDeployment{
		Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
			{
				TableName: "test",
				Spec: &logicalpb.TracepointSpec{
					Outputs: []*logicalpb.Output{
						{
							Name:   "test",
							Fields: []string{"def"},
						},
					},
				},
			},
			{
				TableName: "anotherTracepoint",
				Spec: &logicalpb.TracepointSpec{
					Outputs: []*logicalpb.Output{
						{
							Name:   "anotherTracepoint",
							Fields: []string{"def"},
						},
					},
				},
			},
		},
	}

	mockAgtMgr.
		EXPECT().
		MessageAgents([]uuid.UUID{}, gomock.Any()).
		Return(nil)

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return([]*agentpb.Agent{}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepointsWithNames([]string{"test_tracepoint"}).
		Return([]*uuid.UUID{nil}, nil)

	var tpID uuid.UUID
	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(gomock.Any(), gomock.Any()).
		DoAndReturn(func(tracepointID uuid.UUID, tracepointInfo *storepb.TracepointInfo) error {
			assert.Equal(t, program, tracepointInfo.Tracepoint)
			tpID = tracepointID
			assert.Equal(t, "test_tracepoint", tracepointInfo.Name)
			return nil
		})
	mockTracepointStore.
		EXPECT().
		SetTracepointWithName("test_tracepoint", gomock.Any()).
		DoAndReturn(func(tpName string, id uuid.UUID) error {
			assert.Equal(t, tpID, id)
			return nil
		})
	mockTracepointStore.
		EXPECT().
		SetTracepointTTL(gomock.Any(), time.Second*5).
		DoAndReturn(func(id uuid.UUID, ttl time.Duration) error {
			assert.Equal(t, tpID, id)
			return nil
		})
	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, tracepointMgr)

	reqs := []*metadatapb.RegisterTracepointRequest_TracepointRequest{
		{
			TracepointDeployment: program,
			Name:                 "test_tracepoint",
			TTL: &types.Duration{
				Seconds: 5,
			},
		},
	}
	req := metadatapb.RegisterTracepointRequest{
		Requests: reqs,
	}

	resp, err := s.RegisterTracepoint(context.Background(), &req)

	assert.NotNil(t, resp)
	require.NoError(t, err)

	assert.Equal(t, 1, len(resp.Tracepoints))
	assert.Equal(t, tpID, utils.UUIDFromProtoOrNil(resp.Tracepoints[0].ID))
	assert.Equal(t, statuspb.OK, resp.Tracepoints[0].Status.ErrCode)
	assert.Equal(t, statuspb.OK, resp.Status.ErrCode)
}

func Test_Server_RegisterTracepoint_Exists(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)

	program := &logicalpb.TracepointDeployment{
		Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
			{
				TableName: "table1",
				Spec: &logicalpb.TracepointSpec{
					Outputs: []*logicalpb.Output{
						{
							Name:   "table1",
							Fields: []string{"def", "abcd"},
						},
					},
				},
			},
		},
	}

	oldTPID := uuid.Must(uuid.NewV4())

	mockAgtMgr.
		EXPECT().
		MessageAgents([]uuid.UUID{}, gomock.Any()).
		Return(nil)

	mockTracepointStore.
		EXPECT().
		GetTracepointsWithNames([]string{"test_tracepoint"}).
		Return([]*uuid.UUID{&oldTPID}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(oldTPID).
		Return(&storepb.TracepointInfo{
			Tracepoint: &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
						Spec: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"def"},
								},
							},
						},
					},
				},
			},
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepointTTLs([]uuid.UUID{oldTPID}).
		Return(nil)

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return([]*agentpb.Agent{}, nil)

	var tpID uuid.UUID
	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(gomock.Any(), gomock.Any()).
		DoAndReturn(func(tracepointID uuid.UUID, tracepointInfo *storepb.TracepointInfo) error {
			assert.Equal(t, program, tracepointInfo.Tracepoint)
			tpID = tracepointID
			assert.Equal(t, "test_tracepoint", tracepointInfo.Name)
			return nil
		})
	mockTracepointStore.
		EXPECT().
		SetTracepointWithName("test_tracepoint", gomock.Any()).
		DoAndReturn(func(tpName string, id uuid.UUID) error {
			assert.Equal(t, tpID, id)
			return nil
		})
	mockTracepointStore.
		EXPECT().
		SetTracepointTTL(gomock.Any(), time.Second*5).
		DoAndReturn(func(id uuid.UUID, ttl time.Duration) error {
			assert.Equal(t, tpID, id)
			return nil
		})

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, tracepointMgr)

	reqs := []*metadatapb.RegisterTracepointRequest_TracepointRequest{
		{
			TracepointDeployment: program,
			Name:                 "test_tracepoint",
			TTL: &types.Duration{
				Seconds: 5,
			},
		},
	}
	req := metadatapb.RegisterTracepointRequest{
		Requests: reqs,
	}

	resp, err := s.RegisterTracepoint(context.Background(), &req)

	assert.NotNil(t, resp)
	require.NoError(t, err)

	assert.Equal(t, 1, len(resp.Tracepoints))
	assert.Equal(t, utils.ProtoFromUUID(tpID), resp.Tracepoints[0].ID)
	assert.Equal(t, statuspb.OK, resp.Tracepoints[0].Status.ErrCode)
}

func Test_Server_GetTracepointInfo(t *testing.T) {
	tests := []struct {
		name             string
		expectedState    statuspb.LifeCycleState
		expectedStatus   *statuspb.Status
		agentStates      []*storepb.AgentTracepointStatus
		tracepointExists bool
		expectAll        bool
	}{
		{
			name:           "healthy tracepoint",
			expectedState:  statuspb.RUNNING_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentTracepointStatus{
				{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.NOT_FOUND,
					},
				},
				{
					State: statuspb.RUNNING_STATE,
				},
			},
			tracepointExists: true,
		},
		{
			name:           "terminated tracepoint",
			expectedState:  statuspb.TERMINATED_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentTracepointStatus{
				{
					State: statuspb.RUNNING_STATE,
				},
				{
					State: statuspb.TERMINATED_STATE,
				},
			},
			tracepointExists: true,
		},
		{
			name:          "nonexistent tracepoint",
			expectedState: statuspb.UNKNOWN_STATE,
			expectedStatus: &statuspb.Status{
				ErrCode: statuspb.NOT_FOUND,
			},
			agentStates:      nil,
			tracepointExists: false,
		},
		{
			name:           "pending tracepoint",
			expectedState:  statuspb.PENDING_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentTracepointStatus{
				{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.NOT_FOUND,
					},
				},
				{
					State: statuspb.PENDING_STATE,
				},
			},
			tracepointExists: true,
		},
		{
			name:          "failed tracepoint",
			expectedState: statuspb.FAILED_STATE,
			expectedStatus: &statuspb.Status{
				ErrCode: statuspb.RESOURCE_UNAVAILABLE,
			},
			agentStates: []*storepb.AgentTracepointStatus{
				{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.RESOURCE_UNAVAILABLE,
					},
				},
				{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.RESOURCE_UNAVAILABLE,
					},
				},
			},
			tracepointExists: true,
		},
		{
			name:           "all tracepoints",
			expectedState:  statuspb.PENDING_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentTracepointStatus{
				{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.RESOURCE_UNAVAILABLE,
					},
				},
				{
					State: statuspb.PENDING_STATE,
				},
			},
			tracepointExists: true,
			expectAll:        true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			// Set up mock.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			mockAgtMgr := mock_agent.NewMockManager(ctrl)
			mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

			tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)

			program := &logicalpb.TracepointDeployment{
				Programs: []*logicalpb.TracepointDeployment_TracepointProgram{
					{
						TableName: "table1",
					},
					{
						TableName: "test",
					},
				},
			}

			tID := uuid.Must(uuid.NewV4())
			if !test.tracepointExists {
				mockTracepointStore.
					EXPECT().
					GetTracepointsForIDs([]uuid.UUID{tID}).
					Return([]*storepb.TracepointInfo{nil}, nil)
			} else {
				if test.expectAll {
					mockTracepointStore.
						EXPECT().
						GetTracepoints().
						Return([]*storepb.TracepointInfo{{ID: utils.ProtoFromUUID(tID), Tracepoint: program, ExpectedState: statuspb.RUNNING_STATE}}, nil)
				} else {
					mockTracepointStore.
						EXPECT().
						GetTracepointsForIDs([]uuid.UUID{tID}).
						Return([]*storepb.TracepointInfo{{ID: utils.ProtoFromUUID(tID), Tracepoint: program, ExpectedState: statuspb.RUNNING_STATE}}, nil)
				}

				mockTracepointStore.
					EXPECT().
					GetTracepointStates(tID).
					Return(test.agentStates, nil)
			}

			// Set up server.
			env, err := metadataenv.New("vizier")
			if err != nil {
				t.Fatal("Failed to create api environment.")
			}

			s := controllers.NewServer(env, nil, nil, mockAgtMgr, tracepointMgr)
			req := metadatapb.GetTracepointInfoRequest{
				IDs: []*uuidpb.UUID{utils.ProtoFromUUID(tID)},
			}
			if test.expectAll {
				req = metadatapb.GetTracepointInfoRequest{
					IDs: []*uuidpb.UUID{},
				}
			}

			resp, err := s.GetTracepointInfo(context.Background(), &req)
			require.NoError(t, err)
			assert.Equal(t, 1, len(resp.Tracepoints))
			assert.Equal(t, utils.ProtoFromUUID(tID), resp.Tracepoints[0].ID)
			assert.Equal(t, test.expectedState, resp.Tracepoints[0].State)
			var status *statuspb.Status
			if len(resp.Tracepoints[0].Statuses) > 0 {
				status = resp.Tracepoints[0].Statuses[0]
			}
			assert.Equal(t, test.expectedStatus, status)
			if test.tracepointExists {
				assert.Equal(t, statuspb.RUNNING_STATE, resp.Tracepoints[0].ExpectedState)
				assert.Equal(t, []string{"table1", "test"}, resp.Tracepoints[0].SchemaNames)
			}
		})
	}
}

func Test_Server_RemoveTracepoint(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)

	tpID1 := uuid.Must(uuid.NewV4())
	tpID2 := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		GetTracepointsWithNames([]string{"test1", "test2"}).
		Return([]*uuid.UUID{
			&tpID1, &tpID2,
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepointTTLs([]uuid.UUID{tpID1, tpID2}).
		Return(nil)

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, tracepointMgr)

	req := metadatapb.RemoveTracepointRequest{
		Names: []string{"test1", "test2"},
	}

	resp, err := s.RemoveTracepoint(context.Background(), &req)

	assert.NotNil(t, resp)
	require.NoError(t, err)

	assert.Equal(t, statuspb.OK, resp.Status.ErrCode)
}

func createDialer(lis *bufconn.Listener) func(ctx context.Context, url string) (net.Conn, error) {
	return func(ctx context.Context, url string) (net.Conn, error) {
		return lis.Dial()
	}
}

func TestGetAgentUpdates(t *testing.T) {
	viper.Set("jwt_signing_key", "jwtkey")
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")

	agent1IDStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u1, err := uuid.FromString(agent1IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u1pb := utils.ProtoFromUUID(u1)

	agent2IDStr := "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u2, err := uuid.FromString(agent2IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2pb := utils.ProtoFromUUID(u2)

	agent3IDStr := "61123ced-1de9-4ab1-ae6a-0ba08c8c676c"
	u3, err := uuid.FromString(agent3IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u3pb := utils.ProtoFromUUID(u3)

	updates1 := []*metadatapb.AgentUpdate{
		{
			AgentID: u1pb,
			Update: &metadatapb.AgentUpdate_Agent{
				Agent: &agentpb.Agent{
					LastHeartbeatNS: 10,
					CreateTimeNS:    5,
					Info: &agentpb.AgentInfo{
						AgentID: u1pb,
						HostInfo: &agentpb.HostInfo{
							Hostname: "test_host",
							HostIP:   "127.0.0.1",
						},
					},
					ASID: 123,
				},
			},
		},
		{
			AgentID: u2pb,
			Update: &metadatapb.AgentUpdate_Agent{
				Agent: &agentpb.Agent{
					LastHeartbeatNS: 20,
					CreateTimeNS:    0,
					Info: &agentpb.AgentInfo{
						AgentID: u2pb,
						HostInfo: &agentpb.HostInfo{
							Hostname: "another_host",
							HostIP:   "127.0.0.1",
						},
					},
					ASID: 456,
				},
			},
		},
		{
			AgentID: u1pb,
			Update: &metadatapb.AgentUpdate_DataInfo{
				DataInfo: &messagespb.AgentDataInfo{
					MetadataInfo: &distributedpb.MetadataInfo{
						MetadataFields: []sharedmetadatapb.MetadataType{
							sharedmetadatapb.CONTAINER_ID,
							sharedmetadatapb.POD_NAME,
						},
						Filter: &distributedpb.MetadataInfo_XXHash64BloomFilter{
							XXHash64BloomFilter: &bloomfilterpb.XXHash64BloomFilter{
								Data:      []byte("5678"),
								NumHashes: 3,
							},
						},
					},
				},
			},
		},
	}

	computedSchema1 := &storepb.ComputedSchema{
		Tables: testTableInfos(),
		TableNameToAgentIDs: map[string]*storepb.ComputedSchema_AgentIDs{
			"table1": {
				AgentID: []*uuidpb.UUID{u1pb, u2pb},
			},
			"table2": {
				AgentID: []*uuidpb.UUID{u1pb},
			},
		},
	}

	cursorID := uuid.Must(uuid.NewV4())
	mockAgtMgr.
		EXPECT().
		NewAgentUpdateCursor().
		Return(cursorID)

	// Initial state (2 messages)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(cursorID).
		Return(updates1, computedSchema1, nil)

	// Empty state (0 messages)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(cursorID).
		Return(nil, nil, nil)

	computedSchema2 := &storepb.ComputedSchema{
		Tables: []*storepb.TableInfo{computedSchema1.Tables[0]},
		TableNameToAgentIDs: map[string]*storepb.ComputedSchema_AgentIDs{
			"table1": {
				AgentID: []*uuidpb.UUID{u2pb},
			},
		},
	}

	// Schema update (1 message)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(cursorID).
		Return(nil, computedSchema2, nil)

	updates2 := []*metadatapb.AgentUpdate{
		{
			AgentID: u3pb,
			Update: &metadatapb.AgentUpdate_Deleted{
				Deleted: true,
			},
		},
	}

	// Agent updates (1 message)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(cursorID).
		Return(updates2, nil, nil)

	// Empty state (0 messages)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(cursorID).
		Return(nil, nil, nil).
		AnyTimes()

	// Remove cursor
	mockAgtMgr.
		EXPECT().
		DeleteAgentUpdateCursor(cursorID).
		Return().
		AnyTimes()

	// Set up server.
	mdEnv, err := metadataenv.New("test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	srv := controllers.NewServer(mdEnv, nil, nil, mockAgtMgr, nil)

	env := env.New("withpixie.ai")
	s := server.CreateGRPCServer(env, &server.GRPCServerOptions{})
	metadatapb.RegisterMetadataServiceServer(s, srv)
	lis := bufconn.Listen(1024 * 1024)

	eg := errgroup.Group{}
	eg.Go(func() error { return s.Serve(lis) })

	defer func() {
		s.GracefulStop()

		err := eg.Wait()
		if err != nil {
			t.Fatalf("failed to start server: %v", err)
		}
	}()

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithContextDialer(createDialer(lis)), grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		t.Fatalf("Failed to dial bufnet: %v", err)
	}
	defer conn.Close()

	client := metadatapb.NewMetadataServiceClient(conn)

	var wg sync.WaitGroup
	wg.Add(1)

	var resps []*metadatapb.AgentUpdatesResponse
	var readErr error

	errCh := make(chan error)
	msgCh := make(chan *metadatapb.AgentUpdatesResponse)
	msgCount := 0
	expectedMsgs := 5

	go func() {
		validTestToken := testingutils.GenerateTestJWTToken(t, viper.GetString("jwt_signing_key"))
		ctx = grpc_metadata.AppendToOutgoingContext(ctx, "authorization",
			fmt.Sprintf("bearer %s", validTestToken))
		ctx, cancel := context.WithCancel(ctx)
		defer cancel()

		resp, err := client.GetAgentUpdates(ctx, &metadatapb.AgentUpdatesRequest{
			MaxUpdateInterval: &types.Duration{
				Seconds: 0,
				Nanos:   10 * 1000 * 1000, // 10 ms
			},
			MaxUpdatesPerResponse: 2,
		})
		assert.NotNil(t, resp)
		require.NoError(t, err)

		defer func() {
			close(errCh)
			close(msgCh)
		}()

		for {
			msg, err := resp.Recv()
			if err != nil {
				errCh <- err
				return
			}
			msgCh <- msg
			msgCount++
			if msgCount >= expectedMsgs {
				return
			}
		}
	}()

	go func() {
		defer wg.Done()
		timeout := time.NewTimer(5 * time.Second)
		for {
			select {
			case <-timeout.C:
				readErr = errors.New("timeout")
			case err := <-errCh:
				readErr = err
				return
			case msg := <-msgCh:
				resps = append(resps, msg)
				if len(resps) >= expectedMsgs {
					return
				}
			}
		}
	}()
	wg.Wait()
	assert.Nil(t, readErr)
	assert.Equal(t, len(resps), expectedMsgs)

	// Check first message
	r0 := resps[0]
	// Check updates
	assert.Equal(t, 2, len(r0.AgentUpdates))
	assert.Equal(t, updates1[0], r0.AgentUpdates[0])
	assert.Equal(t, updates1[1], r0.AgentUpdates[1])
	assert.Nil(t, r0.AgentSchemas)

	// Check second message
	r1 := resps[1]
	assert.Equal(t, 1, len(r1.AgentUpdates))
	assert.Equal(t, updates1[2], r1.AgentUpdates[0])
	// Check schemas
	assert.Equal(t, 2, len(r1.AgentSchemas))
	assert.Equal(t, "table1", r1.AgentSchemas[0].Name)
	assert.Equal(t, 3, len(r1.AgentSchemas[0].Relation.Columns))
	assert.Equal(t, 2, len(r1.AgentSchemas[0].AgentList))
	assert.Equal(t, u1pb, r1.AgentSchemas[0].AgentList[0])
	assert.Equal(t, u2pb, r1.AgentSchemas[0].AgentList[1])
	assert.Equal(t, "table2", r1.AgentSchemas[1].Name)
	assert.Equal(t, 2, len(r1.AgentSchemas[1].Relation.Columns))
	assert.Equal(t, 1, len(r1.AgentSchemas[1].AgentList))
	assert.Equal(t, u1pb, r1.AgentSchemas[1].AgentList[0])

	// Check empty message
	r2 := resps[2]
	assert.Nil(t, r2.AgentUpdates)
	assert.Nil(t, r2.AgentSchemas)

	// Check third message
	r3 := resps[3]
	assert.Nil(t, r3.AgentUpdates)
	assert.Equal(t, 1, len(r3.AgentSchemas))
	assert.Equal(t, "table1", r3.AgentSchemas[0].Name)
	assert.Equal(t, 3, len(r3.AgentSchemas[0].Relation.Columns))
	assert.Equal(t, 1, len(r3.AgentSchemas[0].AgentList))
	assert.Equal(t, u2pb, r3.AgentSchemas[0].AgentList[0])

	// Check fourth message
	r4 := resps[4]
	assert.Nil(t, r4.AgentSchemas)
	assert.Equal(t, 1, len(r4.AgentUpdates))
	assert.Equal(t, updates2[0], r4.AgentUpdates[0])
}

func Test_Server_UpdateConfig(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)
	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)

	mockAgtMgr.
		EXPECT().
		UpdateConfig("pl", "pem-1234", "gprof", "true").
		Return(nil)

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, nil, mockAgtMgr, tracepointMgr)

	req := metadatapb.UpdateConfigRequest{
		AgentPodName: "pl/pem-1234",
		Key:          "gprof",
		Value:        "true",
	}

	resp, err := s.UpdateConfig(context.Background(), &req)

	assert.NotNil(t, resp)
	require.NoError(t, err)
	assert.Equal(t, statuspb.OK, resp.Status.ErrCode)

	// This is an invalid request because AgentPodName must contain the namespace.
	invalidReq := metadatapb.UpdateConfigRequest{
		AgentPodName: "pem-1234",
		Key:          "gprof",
		Value:        "true",
	}
	resp, err = s.UpdateConfig(context.Background(), &invalidReq)
	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func Test_Server_ConvertLabelsToPods(t *testing.T) {
	// Set up pod label store with associated pods and labels.
	pls := &testutils.InMemoryPodLabelStore{
		Store: make(map[string]string),
	}
	err := pls.SetPodLabels("namespace1", "pod1", map[string]string{"app": "my_app", "version": "v1"})
	require.NoError(t, err)
	err = pls.SetPodLabels("namespace1", "pod2", map[string]string{"app": "my_app", "version": "v2"})
	require.NoError(t, err)

	// Set up server.
	env, err := metadataenv.New("vizier")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s := controllers.NewServer(env, nil, pls, nil, nil)

	program := &logicalpb.TracepointDeployment{}
	err = proto.UnmarshalText(testutils.TDLabelSelectorPb, program)
	require.NoError(t, err)

	err = s.ConvertLabelsToPods(program)
	require.NoError(t, err)
	sort.Strings(program.GetDeploymentSpec().GetPodProcess().GetPods())

	expected := &logicalpb.TracepointDeployment{}
	err = proto.UnmarshalText(testutils.TDPodProcessPb, expected)
	require.NoError(t, err)

	assert.True(t, proto.Equal(program, expected), fmt.Sprintf("expect: %s\nactual: %s", expected, program))
}
