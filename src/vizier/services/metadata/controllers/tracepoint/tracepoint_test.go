package tracepoint_test

import (
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	statuspb "pixielabs.ai/pixielabs/src/common/base/statuspb"
	"pixielabs.ai/pixielabs/src/stirling/source_connectors/dynamic_tracer/dynamic_tracing/ir/logicalpb"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	mock_agent "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/agent/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/tracepoint"
	mock_tracepoint "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/tracepoint/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

func TestCreateTracepoint(t *testing.T) {
	tests := []struct {
		name                    string
		originalTracepoint      *logicalpb.TracepointDeployment
		originalTracepointState statuspb.LifeCycleState
		newTracepoint           *logicalpb.TracepointDeployment
		expectError             bool
		expectOldUpdated        bool
		expectTTLUpdateOnly     bool
	}{
		{
			name:               "new_tracepoint",
			originalTracepoint: nil,
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectError: false,
		},
		{
			name: "existing tracepoint, match",
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			originalTracepointState: statuspb.RUNNING_STATE,
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectTTLUpdateOnly: true,
		},
		{
			name: "existing tracepoint, not exactly same (1)",
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc"},
								},
							},
						},
					},
				},
			},
			originalTracepointState: statuspb.RUNNING_STATE,
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectOldUpdated: true,
		},
		{
			name:                    "existing tracepoint, not exactly same (2)",
			originalTracepointState: statuspb.RUNNING_STATE,
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
							Probes: []*logicalpb.Probe{
								{Name: "test"},
							},
						},
					},
				},
			},
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectOldUpdated: true,
		},
		{
			name:                    "existing terminated tracepoint",
			originalTracepointState: statuspb.TERMINATED_STATE,
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
							Probes: []*logicalpb.Probe{
								{Name: "test"},
							},
						},
					},
				},
			},
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectError: false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			// Set up mock.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

			origID := uuid.Must(uuid.NewV4())

			if test.originalTracepoint == nil {
				mockTracepointStore.
					EXPECT().
					GetTracepointsWithNames([]string{"test_tracepoint"}).
					Return([]*uuid.UUID{nil}, nil)
			} else {
				mockTracepointStore.
					EXPECT().
					GetTracepointsWithNames([]string{"test_tracepoint"}).
					Return([]*uuid.UUID{&origID}, nil)
				mockTracepointStore.
					EXPECT().
					GetTracepoint(origID).
					Return(&storepb.TracepointInfo{
						ExpectedState: test.originalTracepointState,
						Tracepoint:    test.originalTracepoint,
					}, nil)
			}

			if test.expectTTLUpdateOnly {
				mockTracepointStore.
					EXPECT().
					SetTracepointTTL(origID, time.Second*5)
			}

			if test.expectOldUpdated {
				mockTracepointStore.
					EXPECT().
					DeleteTracepointTTLs([]uuid.UUID{origID}).
					Return(nil)
			}

			var newID uuid.UUID

			if !test.expectError && !test.expectTTLUpdateOnly {
				mockTracepointStore.
					EXPECT().
					UpsertTracepoint(gomock.Any(), gomock.Any()).
					DoAndReturn(func(id uuid.UUID, tpInfo *storepb.TracepointInfo) error {
						newID = id
						assert.Equal(t, &storepb.TracepointInfo{
							Tracepoint:    test.newTracepoint,
							Name:          "test_tracepoint",
							ID:            utils.ProtoFromUUID(id),
							ExpectedState: statuspb.RUNNING_STATE,
						}, tpInfo)
						return nil
					})

				mockTracepointStore.
					EXPECT().
					SetTracepointWithName("test_tracepoint", gomock.Any()).
					DoAndReturn(func(name string, id uuid.UUID) error {
						assert.Equal(t, newID, id)
						return nil
					})

				mockTracepointStore.
					EXPECT().
					SetTracepointTTL(gomock.Any(), time.Second*5).
					DoAndReturn(func(id uuid.UUID, ttl time.Duration) error {
						assert.Equal(t, newID, id)
						return nil
					})
			}

			mockAgtMgr := mock_agent.NewMockManager(ctrl)
			tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
			defer tracepointMgr.Close()

			actualTpID, err := tracepointMgr.CreateTracepoint("test_tracepoint", test.newTracepoint, time.Second*5)
			if test.expectError || test.expectTTLUpdateOnly {
				assert.Equal(t, tracepoint.ErrTracepointAlreadyExists, err)
			} else {
				require.NoError(t, err)
				assert.Equal(t, &newID, actualTpID)
			}
		})
	}
}

func TestGetTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	tID1 := uuid.Must(uuid.NewV4())
	tID2 := uuid.Must(uuid.NewV4())
	expectedTracepointInfo := []*storepb.TracepointInfo{
		{
			ID: utils.ProtoFromUUID(tID1),
		},
		{
			ID: utils.ProtoFromUUID(tID2),
		},
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetAllTracepoints()
	require.NoError(t, err)
	assert.Equal(t, expectedTracepointInfo, tracepoints)
}

func TestGetTracepointInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	tID1 := uuid.Must(uuid.NewV4())
	expectedTracepointInfo := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(tID1),
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tID1).
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetTracepointInfo(tID1)
	require.NoError(t, err)
	assert.Equal(t, expectedTracepointInfo, tracepoints)
}

func TestGetTracepointStates(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID1 := uuid.Must(uuid.NewV4())
	tID1 := uuid.Must(uuid.NewV4())
	expectedTracepointStatus1 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tID1),
		AgentID: utils.ProtoFromUUID(agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	agentUUID2 := uuid.Must(uuid.NewV4())
	expectedTracepointStatus2 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tID1),
		AgentID: utils.ProtoFromUUID(agentUUID2),
		State:   statuspb.PENDING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		GetTracepointStates(tID1).
		Return([]*storepb.AgentTracepointStatus{expectedTracepointStatus1, expectedTracepointStatus2}, nil)

	tracepoints, err := tracepointMgr.GetTracepointStates(tID1)
	require.NoError(t, err)
	assert.Equal(t, expectedTracepointStatus1, tracepoints[0])
	assert.Equal(t, expectedTracepointStatus2, tracepoints[1])
}

func TestRegisterTracepoint(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID := uuid.Must(uuid.NewV4())
	tracepointID := uuid.Must(uuid.NewV4())
	program := &logicalpb.TracepointDeployment{
		Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
			{
				TableName: "test",
			},
			{
				TableName: "anotherTracepoint",
			},
		},
	}

	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messages.RegisterTracepointRequest{
						TracepointDeployment: program,
						ID:                   utils.ProtoFromUUID(tracepointID),
					},
				},
			},
		},
	}
	msg, err := tracepointReq.Marshal()
	if err != nil {
		t.Fatal(err)
	}

	mockAgtMgr.
		EXPECT().
		MessageAgents([]uuid.UUID{agentUUID}, msg).
		Return(nil)

	err = tracepointMgr.RegisterTracepoint([]uuid.UUID{agentUUID}, tracepointID, program)
	require.NoError(t, err)
}

func TestUpdateAgentTracepointStatus(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID1 := uuid.Must(uuid.NewV4())
	tpID := uuid.Must(uuid.NewV4())
	expectedTracepointState := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(tpID),
		AgentID: utils.ProtoFromUUID(agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(expectedTracepointState).
		Return(nil)

	err := tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(tpID), utils.ProtoFromUUID(agentUUID1), statuspb.RUNNING_STATE, nil)
	require.NoError(t, err)
}

func TestUpdateAgentTracepointStatus_Terminated(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

	agentUUID1 := uuid.Must(uuid.NewV4())
	tpID := uuid.Must(uuid.NewV4())
	agentUUID2 := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		GetTracepointStates(tpID).
		Return([]*storepb.AgentTracepointStatus{
			{AgentID: utils.ProtoFromUUID(agentUUID1), State: statuspb.TERMINATED_STATE},
			{AgentID: utils.ProtoFromUUID(agentUUID2), State: statuspb.RUNNING_STATE},
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepoint(tpID).
		Return(nil)

	err := tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(tpID), utils.ProtoFromUUID(agentUUID2), statuspb.TERMINATED_STATE, nil)
	require.NoError(t, err)
}

func TestTTLExpiration(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)
	mockAgtMgr := mock_agent.NewMockManager(ctrl)

	tpID1 := uuid.Must(uuid.NewV4())
	tpID2 := uuid.Must(uuid.NewV4())
	tpID3 := uuid.Must(uuid.NewV4())
	tpID4 := uuid.Must(uuid.NewV4())

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		Return([]*storepb.TracepointInfo{
			{
				ID: utils.ProtoFromUUID(tpID1),
			},
			{
				ID: utils.ProtoFromUUID(tpID2),
			},
			{
				ID: utils.ProtoFromUUID(tpID3),
			},
			{
				ID:            utils.ProtoFromUUID(tpID4),
				ExpectedState: statuspb.TERMINATED_STATE,
			},
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepointTTLs().
		Return([]uuid.UUID{
			tpID1,
			tpID3,
			tpID4,
		}, []time.Time{
			time.Now().Add(1 * time.Hour),
			time.Now().Add(-1 * time.Minute),
			time.Now().Add(-1 * time.Hour),
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tpID2).
		Return(&storepb.TracepointInfo{
			ID: utils.ProtoFromUUID(tpID2),
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tpID3).
		Return(&storepb.TracepointInfo{
			ID: utils.ProtoFromUUID(tpID3),
		}, nil)

	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(tpID2, &storepb.TracepointInfo{ID: utils.ProtoFromUUID(tpID2), ExpectedState: statuspb.TERMINATED_STATE}).
		Return(nil)

	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(tpID3, &storepb.TracepointInfo{ID: utils.ProtoFromUUID(tpID3), ExpectedState: statuspb.TERMINATED_STATE}).
		Return(nil)

	var wg sync.WaitGroup
	wg.Add(2)

	var seenDeletions []string
	msgHandler := func(msg []byte) error {
		vzMsg := &messages.VizierMessage{}
		err := proto.Unmarshal(msg, vzMsg)
		require.NoError(t, err)
		req := vzMsg.GetTracepointMessage().GetRemoveTracepointRequest()
		assert.NotNil(t, req)
		seenDeletions = append(seenDeletions, utils.ProtoToUUIDStr(req.ID))

		wg.Done()
		return nil
	}

	mockAgtMgr.
		EXPECT().
		MessageActiveAgents(gomock.Any()).
		Times(2).
		DoAndReturn(msgHandler)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 25*time.Millisecond)
	defer tracepointMgr.Close()

	wg.Wait()
	assert.Contains(t, seenDeletions, tpID2.String())
	assert.Contains(t, seenDeletions, tpID3.String())
}

func TestUpdateAgentTracepointStatus_RemoveTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_agent.NewMockManager(ctrl)
	mockTracepointStore := mock_tracepoint.NewMockStore(ctrl)

	tracepointMgr := tracepoint.NewManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	defer tracepointMgr.Close()

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

	err := tracepointMgr.RemoveTracepoints([]string{"test1", "test2"})
	require.NoError(t, err)
}
