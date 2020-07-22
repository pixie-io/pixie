package controllers_test

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	distributedpb "pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	bloomfilterpb "pixielabs.ai/pixielabs/src/shared/bloomfilterpb"
	k8smetadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	sharedmetadatapb "pixielabs.ai/pixielabs/src/shared/metadatapb"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir"
	irpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir/logical/shared"
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

	schemaInfos := make([]*k8smetadatapb.SchemaInfo, 2)

	schema1Cols := make([]*k8smetadatapb.SchemaInfo_ColumnInfo, 3)
	schema1Cols[0] = &k8smetadatapb.SchemaInfo_ColumnInfo{
		Name:     "t1Col1",
		DataType: 2,
	}
	schema1Cols[1] = &k8smetadatapb.SchemaInfo_ColumnInfo{
		Name:     "t1Col2",
		DataType: 1,
	}
	schema1Cols[2] = &k8smetadatapb.SchemaInfo_ColumnInfo{
		Name:     "t1Col3",
		DataType: 3,
	}
	schemaInfos[0] = &k8smetadatapb.SchemaInfo{
		Name:    "table1",
		Columns: schema1Cols,
	}

	schema2Cols := make([]*k8smetadatapb.SchemaInfo_ColumnInfo, 2)
	schema2Cols[0] = &k8smetadatapb.SchemaInfo_ColumnInfo{
		Name:     "t2Col1",
		DataType: 1,
	}
	schema2Cols[1] = &k8smetadatapb.SchemaInfo_ColumnInfo{
		Name:     "t2Col2",
		DataType: 3,
	}
	schemaInfos[1] = &k8smetadatapb.SchemaInfo{
		Name:    "table2",
		Columns: schema2Cols,
	}

	mockMds.
		EXPECT().
		GetComputedSchemas().
		Return(schemaInfos, nil)

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

	schemaInfos := []*k8smetadatapb.SchemaInfo{
		&k8smetadatapb.SchemaInfo{
			Name: "table1",
			Columns: []*k8smetadatapb.SchemaInfo_ColumnInfo{
				&k8smetadatapb.SchemaInfo_ColumnInfo{
					Name:     "t1Col1",
					DataType: 1,
				},
			},
		},
	}
	mockMds.
		EXPECT().
		GetComputedSchemas().
		Return(schemaInfos, nil)

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

		// check the schema
		assert.Equal(t, 1, len(agentMetadata.Schema.RelationMap))
		assert.Equal(t, 1, len(agentMetadata.Schema.RelationMap["table1"].Columns))
		assert.Equal(t, "t1Col1", agentMetadata.Schema.RelationMap["table1"].Columns[0].ColumnName)
		assert.Equal(t, typespb.BOOLEAN, agentMetadata.Schema.RelationMap["table1"].Columns[0].ColumnType)
	}

	assert.Equal(t, len(dataInfoMap), 2)
	assert.Equal(t, dataInfoMap[agent1ID], expectedDataInfos[agent1ID])
	assert.Equal(t, dataInfoMap[agent2ID], expectedDataInfos[agent2ID])
}

func Test_Server_RegisterProbe(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

	program := &logicalpb.Program{
		Probes: []*logicalpb.Probe{
			&logicalpb.Probe{
				Name: "test",
			},
			&logicalpb.Probe{
				Name: "anotherProbe",
			},
		},
	}

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return([]*agentpb.Agent{}, nil)

	mockProbeStore.
		EXPECT().
		GetProbe("test_probe").
		Return(nil, nil)

	mockProbeStore.
		EXPECT().
		UpsertProbe(gomock.Any(), gomock.Any()).
		DoAndReturn(func(probeID string, probeInfo *storepb.ProbeInfo) error {
			assert.Equal(t, program, probeInfo.Program)
			assert.Equal(t, "test_probe", probeInfo.ProbeID)
			return nil
		})

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	clock := testingutils.NewTestClock(time.Unix(0, 70))

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, probeMgr, mockMds, clock)

	req := metadatapb.RegisterProbeRequest{
		Program:   program,
		ProbeName: "test_probe",
	}

	resp, err := s.RegisterProbe(context.Background(), &req)

	assert.NotNil(t, resp)
	assert.Nil(t, err)

	assert.Equal(t, "test_probe", resp.ProbeID)
	assert.Equal(t, statuspb.OK, resp.Status.ErrCode)
}

func Test_Server_RegisterProbe_Exists(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

	program := &logicalpb.Program{
		Outputs: []*irpb.Output{
			&irpb.Output{
				Name:   "table1",
				Fields: []string{"abc", "def"},
			},
		},
	}

	mockProbeStore.
		EXPECT().
		GetProbe("test_probe").
		Return(&storepb.ProbeInfo{
			Program: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
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

	s, err := controllers.NewServerWithClock(env, mockAgtMgr, probeMgr, mockMds, clock)

	req := metadatapb.RegisterProbeRequest{
		Program:   program,
		ProbeName: "test_probe",
	}

	resp, err := s.RegisterProbe(context.Background(), &req)

	assert.NotNil(t, resp)
	assert.Nil(t, err)

	assert.Equal(t, "test_probe", resp.ProbeID)
	assert.Equal(t, statuspb.ALREADY_EXISTS, resp.Status.ErrCode)
}

func Test_Server_GetProbeInfo(t *testing.T) {
	tests := []struct {
		name           string
		expectedState  statuspb.LifeCycleState
		expectedStatus *statuspb.Status
		agentStates    []*storepb.AgentProbeStatus
		probeExists    bool
	}{
		{
			name:           "healthy probe",
			expectedState:  statuspb.RUNNING_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentProbeStatus{
				&storepb.AgentProbeStatus{
					State: statuspb.FAILED_STATE,
				},
				&storepb.AgentProbeStatus{
					State: statuspb.RUNNING_STATE,
				},
			},
			probeExists: true,
		},
		{
			name:           "evicted probe",
			expectedState:  statuspb.EVICTED_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentProbeStatus{
				&storepb.AgentProbeStatus{
					State: statuspb.RUNNING_STATE,
				},
				&storepb.AgentProbeStatus{
					State: statuspb.EVICTED_STATE,
				},
			},
			probeExists: true,
		},
		{
			name:          "nonexistent probe",
			expectedState: statuspb.UNKNOWN_STATE,
			expectedStatus: &statuspb.Status{
				ErrCode: statuspb.NOT_FOUND,
			},
			agentStates: nil,
			probeExists: false,
		},
		{
			name:           "pending probe",
			expectedState:  statuspb.PENDING_STATE,
			expectedStatus: nil,
			agentStates: []*storepb.AgentProbeStatus{
				&storepb.AgentProbeStatus{
					State: statuspb.FAILED_STATE,
				},
				&storepb.AgentProbeStatus{
					State: statuspb.PENDING_STATE,
				},
			},
			probeExists: true,
		},
		{
			name:          "failed probe",
			expectedState: statuspb.FAILED_STATE,
			expectedStatus: &statuspb.Status{
				ErrCode: statuspb.RESOURCE_UNAVAILABLE,
			},
			agentStates: []*storepb.AgentProbeStatus{
				&storepb.AgentProbeStatus{
					State: statuspb.FAILED_STATE,
					Status: &statuspb.Status{
						ErrCode: statuspb.RESOURCE_UNAVAILABLE,
					},
				},
				&storepb.AgentProbeStatus{
					State: statuspb.FAILED_STATE,
				},
			},
			probeExists: true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			// Set up mock.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
			mockMds := mock_controllers.NewMockMetadataStore(ctrl)
			mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

			probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

			if !test.probeExists {
				mockProbeStore.
					EXPECT().
					GetProbe("test_probe").
					Return(nil, nil)
			} else {
				mockProbeStore.
					EXPECT().
					GetProbe("test_probe").
					Return(&storepb.ProbeInfo{ProbeID: "test_probe"}, nil)

				mockProbeStore.
					EXPECT().
					GetProbeStates("test_probe").
					Return(test.agentStates, nil)
			}

			// Set up server.
			env, err := metadataenv.New()
			if err != nil {
				t.Fatal("Failed to create api environment.")
			}

			clock := testingutils.NewTestClock(time.Unix(0, 70))

			s, err := controllers.NewServerWithClock(env, mockAgtMgr, probeMgr, mockMds, clock)
			req := metadatapb.GetProbeInfoRequest{
				ProbeIDs: []string{"test_probe"},
			}

			resp, err := s.GetProbeInfo(context.Background(), &req)
			assert.Nil(t, err)
			assert.Equal(t, 1, len(resp.Probes))
			assert.Equal(t, "test_probe", resp.Probes[0].ProbeID)
			assert.Equal(t, test.expectedState, resp.Probes[0].State)
			assert.Equal(t, test.expectedStatus, resp.Probes[0].Status)
			assert.Equal(t, test.expectedStatus, resp.Probes[0].Status)
		})
	}
}
