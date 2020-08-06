package controllers_test

import (
	"context"
	"errors"
	"fmt"
	"net"
	"sync"
	"testing"
	"time"

	"google.golang.org/grpc"
	grpc_metadata "google.golang.org/grpc/metadata"
	"google.golang.org/grpc/test/bufconn"

	"github.com/gogo/protobuf/proto"
	types "github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	distributedpb "pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	bloomfilterpb "pixielabs.ai/pixielabs/src/shared/bloomfilterpb"
	sharedmetadatapb "pixielabs.ai/pixielabs/src/shared/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/services"
	env2 "pixielabs.ai/pixielabs/src/shared/services/env"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir/logicalpb"
	utils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
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
	}
	return tableInfos
}

func TestGetAgentInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	agent1IDStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u1, err := uuid.FromString(agent1IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u1pb := utils.ProtoFromUUID(&u1)

	agent2IDStr := "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u2, err := uuid.FromString(agent2IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2pb := utils.ProtoFromUUID(&u2)

	agents := []*agentpb.Agent{
		&agentpb.Agent{
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
		&agentpb.Agent{
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
	}

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return(agents, nil)

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(30, 11))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, nil, mockMds, clock)

	req := metadatapb.AgentInfoRequest{}

	resp, err := s.GetAgentInfo(context.Background(), &req)

	assert.Equal(t, 2, len(resp.Info))

	agentResp := new(metadatapb.AgentMetadata)
	if err := proto.UnmarshalText(testutils.Agent1StatusPB, agentResp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentResp.Status.State = agentpb.AGENT_STATE_UNRESPONSIVE
	agentResp.Status.NSSinceLastHeartbeat = 30*1e9 + 1 // (30s [UnhealthyAgentThreshold] + 11ns [time clock advanced] - 10ns [agent1 LastHeartBeatNS])
	assert.Equal(t, agentResp, resp.Info[0])

	agentResp = new(metadatapb.AgentMetadata)
	if err = proto.UnmarshalText(testutils.Agent2StatusPB, agentResp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentResp.Status.NSSinceLastHeartbeat = 30*1e9 - 9 // (30s [UnhealthyAgentThreshold] + 11ns  [time clock advanced] - 20ns [agent2 LastHeartBeatNS])
	assert.Equal(t, agentResp, resp.Info[1])
}

func TestGetAgentInfoGetActiveAgentsFailed(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return(nil, errors.New("could not get active agents"))

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, nil, mockMds, clock)

	req := metadatapb.AgentInfoRequest{}

	resp, err := s.GetAgentInfo(context.Background(), &req)

	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestGetSchemas(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	tableInfos := testTableInfos()

	mockMds.
		EXPECT().
		GetComputedSchema().
		Return(&storepb.ComputedSchema{Tables: tableInfos}, nil)

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, nil, mockMds, clock)

	req := metadatapb.SchemaRequest{}

	resp, err := s.GetSchemas(context.Background(), &req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	assert.Equal(t, 2, len(resp.Schema.RelationMap))
	assert.Equal(t, 3, len(resp.Schema.RelationMap["table1"].Columns))
	assert.Equal(t, "t1Col1", resp.Schema.RelationMap["table1"].Columns[0].ColumnName)
	assert.Equal(t, typespb.INT64, resp.Schema.RelationMap["table1"].Columns[0].ColumnType)
	assert.Equal(t, "t1Col2", resp.Schema.RelationMap["table1"].Columns[1].ColumnName)
	assert.Equal(t, typespb.BOOLEAN, resp.Schema.RelationMap["table1"].Columns[1].ColumnType)
	assert.Equal(t, "t1Col3", resp.Schema.RelationMap["table1"].Columns[2].ColumnName)
	assert.Equal(t, typespb.UINT128, resp.Schema.RelationMap["table1"].Columns[2].ColumnType)

	assert.Equal(t, 2, len(resp.Schema.RelationMap["table2"].Columns))
	assert.Equal(t, "t2Col1", resp.Schema.RelationMap["table2"].Columns[0].ColumnName)
	assert.Equal(t, typespb.BOOLEAN, resp.Schema.RelationMap["table2"].Columns[0].ColumnType)
	assert.Equal(t, "t2Col2", resp.Schema.RelationMap["table2"].Columns[1].ColumnName)
	assert.Equal(t, typespb.UINT128, resp.Schema.RelationMap["table2"].Columns[1].ColumnType)
}

func TestGetSchemaByAgent(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, nil, mockMds, clock)

	req := metadatapb.SchemaByAgentRequest{}

	resp, err := s.GetSchemaByAgent(context.Background(), &req)

	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestGetAgentTableMetadata(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	agent1ID, err := uuid.FromString("11285cdd-1de9-4ab1-ae6a-0ba08c8c676c")
	require.Nil(t, err)
	agent2ID, err := uuid.FromString("21285cdd-1de9-4ab1-ae6a-0ba08c8c676c")
	require.Nil(t, err)

	schemaInfos := []*storepb.TableInfo{
		&storepb.TableInfo{
			Name: "table1",
			Columns: []*storepb.TableInfo_ColumnInfo{
				&storepb.TableInfo_ColumnInfo{
					Name:     "t1Col1",
					DataType: 1,
				},
			},
		},
	}

	schemaMap := make(map[string]*storepb.ComputedSchema_AgentIDs)
	agentIDList := []*uuidpb.UUID{
		utils.ProtoFromUUID(&agent1ID),
		utils.ProtoFromUUID(&agent2ID),
	}
	schemaMap["table1"] = &storepb.ComputedSchema_AgentIDs{
		AgentID: agentIDList,
	}
	mockMds.
		EXPECT().
		GetComputedSchema().
		Return(&storepb.ComputedSchema{
			Tables:              schemaInfos,
			TableNameToAgentIDs: schemaMap,
		}, nil)

	expectedDataInfos := map[uuid.UUID]*messagespb.AgentDataInfo{}
	expectedDataInfos[agent1ID] = &messagespb.AgentDataInfo{
		MetadataInfo: &distributedpb.MetadataInfo{
			MetadataFields: []sharedmetadatapb.MetadataType{
				sharedmetadatapb.CONTAINER_ID,
				sharedmetadatapb.POD_NAME,
			},
			Filter: &distributedpb.MetadataInfo_XXHash64BloomFilter{
				XXHash64BloomFilter: &bloomfilterpb.XXHash64BloomFilter{
					Data:      []byte("1234"),
					NumHashes: 4,
				},
			},
		},
	}
	expectedDataInfos[agent2ID] = &messagespb.AgentDataInfo{
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
	}

	mockMds.
		EXPECT().
		GetAgentsDataInfo().
		Return(expectedDataInfos, nil)

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, nil, mockMds, clock)

	req := metadatapb.AgentTableMetadataRequest{}

	resp, err := s.GetAgentTableMetadata(context.Background(), &req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, len(resp.MetadataByAgent), 2)
	dataInfoMap := map[uuid.UUID]*messagespb.AgentDataInfo{}

	for _, agentMetadata := range resp.MetadataByAgent {
		id := utils.UUIDFromProtoOrNil(agentMetadata.AgentID)
		dataInfoMap[id] = agentMetadata.DataInfo
	}

	assert.Equal(t, len(dataInfoMap), 2)
	assert.Equal(t, dataInfoMap[agent1ID], expectedDataInfos[agent1ID])
	assert.Equal(t, dataInfoMap[agent2ID], expectedDataInfos[agent2ID])

	assert.Equal(t, 1, len(resp.SchemaInfo))
	assert.Equal(t, "table1", resp.SchemaInfo[0].Name)
	assert.Equal(t, "t1Col1", resp.SchemaInfo[0].Relation.Columns[0].ColumnName)
	assert.Equal(t, typespb.BOOLEAN, resp.SchemaInfo[0].Relation.Columns[0].ColumnType)

	assert.ElementsMatch(t, resp.SchemaInfo[0].AgentList, agentIDList)
}

func Test_Server_RegisterTracepoint(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	program := &logicalpb.TracepointDeployment{
		Probes: []*logicalpb.Probe{
			&logicalpb.Probe{
				Name: "test",
			},
			&logicalpb.Probe{
				Name: "anotherTracepoint",
			},
		},
	}

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
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, tracepointMgr, mockMds, clock)

	reqs := []*metadatapb.RegisterTracepointRequest_TracepointRequest{
		&metadatapb.RegisterTracepointRequest_TracepointRequest{
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
	assert.Nil(t, err)

	assert.Equal(t, 1, len(resp.Tracepoints))
	assert.Equal(t, tpID, utils.UUIDFromProtoOrNil(resp.Tracepoints[0].ID))
	assert.Equal(t, statuspb.OK, resp.Tracepoints[0].Status.ErrCode)
	assert.Equal(t, statuspb.OK, resp.Status.ErrCode)
}

func Test_Server_RegisterTracepoint_Exists(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	program := &logicalpb.TracepointDeployment{
		Outputs: []*logicalpb.Output{
			&logicalpb.Output{
				Name:   "table1",
				Fields: []string{"abc", "def"},
			},
		},
	}

	oldTPID := uuid.NewV4()

	mockTracepointStore.
		EXPECT().
		GetTracepointsWithNames([]string{"test_tracepoint"}).
		Return([]*uuid.UUID{&oldTPID}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(oldTPID).
		Return(&storepb.TracepointInfo{
			Tracepoint: &logicalpb.TracepointDeployment{
				Outputs: []*logicalpb.Output{
					&logicalpb.Output{
						Name:   "table1",
						Fields: []string{"def"},
					},
				},
			},
		}, nil)

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, tracepointMgr, mockMds, clock)

	reqs := []*metadatapb.RegisterTracepointRequest_TracepointRequest{
		&metadatapb.RegisterTracepointRequest_TracepointRequest{
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
	assert.Nil(t, err)

	assert.Equal(t, 1, len(resp.Tracepoints))
	assert.Equal(t, utils.ProtoFromUUID(&oldTPID), resp.Tracepoints[0].ID)
	assert.Equal(t, statuspb.ALREADY_EXISTS, resp.Tracepoints[0].Status.ErrCode)
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
				&storepb.AgentTracepointStatus{
					State: statuspb.FAILED_STATE,
				},
				&storepb.AgentTracepointStatus{
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
				&storepb.AgentTracepointStatus{
					State: statuspb.RUNNING_STATE,
				},
				&storepb.AgentTracepointStatus{
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
				&storepb.AgentTracepointStatus{
					State: statuspb.FAILED_STATE,
				},
				&storepb.AgentTracepointStatus{
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
				&storepb.AgentTracepointStatus{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.RESOURCE_UNAVAILABLE,
					},
				},
				&storepb.AgentTracepointStatus{
					State: statuspb.FAILED_STATE,
				},
			},
			tracepointExists: true,
		},
		{
			name:           "all tracepoints",
			expectedState:  statuspb.PENDING_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentTracepointStatus{
				&storepb.AgentTracepointStatus{
					State: statuspb.FAILED_STATE,
				},
				&storepb.AgentTracepointStatus{
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
			mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
			mockMds := mock_controllers.NewMockMetadataStore(ctrl)
			mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

			tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

			program := &logicalpb.TracepointDeployment{
				Outputs: []*logicalpb.Output{
					&logicalpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
					&logicalpb.Output{
						Name:   "test",
						Fields: []string{"test1", "test2"},
					},
				},
			}

			tID := uuid.NewV4()
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
						Return([]*storepb.TracepointInfo{&storepb.TracepointInfo{ID: utils.ProtoFromUUID(&tID), Tracepoint: program, ExpectedState: statuspb.RUNNING_STATE}}, nil)
				} else {
					mockTracepointStore.
						EXPECT().
						GetTracepointsForIDs([]uuid.UUID{tID}).
						Return([]*storepb.TracepointInfo{&storepb.TracepointInfo{ID: utils.ProtoFromUUID(&tID), Tracepoint: program, ExpectedState: statuspb.RUNNING_STATE}}, nil)
				}

				mockTracepointStore.
					EXPECT().
					GetTracepointStates(tID).
					Return(test.agentStates, nil)
			}

			// Set up server.
			env, err := metadataenv.New()
			if err != nil {
				t.Fatal("Failed to create api environment.")
			}

			clock := testingutils.NewTestClock(time.Unix(0, 70))

			s, err := controllers.NewServerWithClock(env, mockAgtMgr, tracepointMgr, mockMds, clock)
			req := metadatapb.GetTracepointInfoRequest{
				IDs: []*uuidpb.UUID{utils.ProtoFromUUID(&tID)},
			}
			if test.expectAll {
				req = metadatapb.GetTracepointInfoRequest{
					IDs: []*uuidpb.UUID{},
				}
			}

			resp, err := s.GetTracepointInfo(context.Background(), &req)
			assert.Nil(t, err)
			assert.Equal(t, 1, len(resp.Tracepoints))
			assert.Equal(t, utils.ProtoFromUUID(&tID), resp.Tracepoints[0].ID)
			assert.Equal(t, test.expectedState, resp.Tracepoints[0].State)
			assert.Equal(t, test.expectedStatus, resp.Tracepoints[0].Status)
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
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	tpID1 := uuid.NewV4()
	tpID2 := uuid.NewV4()

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
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, tracepointMgr, mockMds, clock)

	req := metadatapb.RemoveTracepointRequest{
		Names: []string{"test1", "test2"},
	}

	resp, err := s.RemoveTracepoint(context.Background(), &req)

	assert.NotNil(t, resp)
	assert.Nil(t, err)

	assert.Equal(t, statuspb.OK, resp.Status.ErrCode)
}

func createDialer(lis *bufconn.Listener) func(string, time.Duration) (net.Conn, error) {
	return func(str string, duration time.Duration) (conn net.Conn, e error) {
		return lis.Dial()
	}
}

func TestGetAgentUpdates(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	agent1IDStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u1, err := uuid.FromString(agent1IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u1pb := utils.ProtoFromUUID(&u1)

	agent2IDStr := "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u2, err := uuid.FromString(agent2IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2pb := utils.ProtoFromUUID(&u2)

	agent3IDStr := "61123ced-1de9-4ab1-ae6a-0ba08c8c676c"
	u3, err := uuid.FromString(agent3IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u3pb := utils.ProtoFromUUID(&u3)

	updates1 := []*metadatapb.AgentUpdate{
		&metadatapb.AgentUpdate{
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
		&metadatapb.AgentUpdate{
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
		&metadatapb.AgentUpdate{
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
			"table1": &storepb.ComputedSchema_AgentIDs{
				AgentID: []*uuidpb.UUID{u1pb, u2pb},
			},
			"table2": &storepb.ComputedSchema_AgentIDs{
				AgentID: []*uuidpb.UUID{u1pb},
			},
		},
	}

	// Initial state (2 messages)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(true).
		Return(updates1, computedSchema1, nil)

	// Empty state (0 messages)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(false).
		Return(nil, nil, nil)

	computedSchema2 := &storepb.ComputedSchema{
		Tables: []*storepb.TableInfo{computedSchema1.Tables[0]},
		TableNameToAgentIDs: map[string]*storepb.ComputedSchema_AgentIDs{
			"table1": &storepb.ComputedSchema_AgentIDs{
				AgentID: []*uuidpb.UUID{u2pb},
			},
		},
	}

	// Schema update (1 message)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(false).
		Return(nil, computedSchema2, nil)

	updates2 := []*metadatapb.AgentUpdate{
		&metadatapb.AgentUpdate{
			AgentID: u3pb,
			Update: &metadatapb.AgentUpdate_Deleted{
				Deleted: true,
			},
		},
	}

	// Agent updates (1 message)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(false).
		Return(updates2, nil, nil)

	// Empty state (0 messages)
	mockAgtMgr.
		EXPECT().
		GetAgentUpdates(false).
		Return(nil, nil, nil).
		AnyTimes()

	// Set up server.
	mdEnv, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))
	server, err := controllers.NewServerWithClock(mdEnv, mockAgtMgr, nil, mockMds, clock)

	env := env2.New()
	s := services.CreateGRPCServer(env, &services.GRPCServerOptions{})
	metadatapb.RegisterMetadataServiceServer(s, server)
	lis := bufconn.Listen(1024 * 1024)

	go func() {
		if err := s.Serve(lis); err != nil {
			t.Fatalf("Server exited with error: %v\n", err)
		}
	}()

	ctx := context.Background()
	conn, err := grpc.DialContext(ctx, "bufnet", grpc.WithDialer(createDialer(lis)), grpc.WithInsecure())
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
		assert.Nil(t, err)

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
				t.Fatal("timeout")
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
	// Check schemas
	assert.Equal(t, 2, len(r0.AgentSchemas))
	assert.Equal(t, "table1", r0.AgentSchemas[0].Name)
	assert.Equal(t, 3, len(r0.AgentSchemas[0].Relation.Columns))
	assert.Equal(t, 2, len(r0.AgentSchemas[0].AgentList))
	assert.Equal(t, u1pb, r0.AgentSchemas[0].AgentList[0])
	assert.Equal(t, u2pb, r0.AgentSchemas[0].AgentList[1])
	assert.Equal(t, "table2", r0.AgentSchemas[1].Name)
	assert.Equal(t, 2, len(r0.AgentSchemas[1].Relation.Columns))
	assert.Equal(t, 1, len(r0.AgentSchemas[1].AgentList))
	assert.Equal(t, u1pb, r0.AgentSchemas[1].AgentList[0])
	// Check updates
	assert.Equal(t, 2, len(r0.AgentUpdates))
	assert.Equal(t, updates1[0], r0.AgentUpdates[0])
	assert.Equal(t, updates1[1], r0.AgentUpdates[1])

	// Check second message
	r1 := resps[1]
	assert.Nil(t, r1.AgentSchemas)
	assert.Equal(t, 1, len(r1.AgentUpdates))
	assert.Equal(t, updates1[2], r1.AgentUpdates[0])

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
