package controllers_test

import (
	"sync"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir/logicalpb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func TestCreateTracepoint(t *testing.T) {
	tests := []struct {
		name                    string
		originalTracepoint      *logicalpb.TracepointDeployment
		originalTracepointState statuspb.LifeCycleState
		newTracepoint           *logicalpb.TracepointDeployment
		expectError             bool
		expectOldUpdated        bool
	}{
		{
			name:               "new_tracepoint",
			originalTracepoint: nil,
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
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
			name: "existing tracepoint, not exactly same (1)",
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
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
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
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
			name: "existing tracepoint, match",
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
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
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectError: true,
		},
		{
			name:                    "existing tracepoint, not exactly same (2)",
			originalTracepointState: statuspb.RUNNING_STATE,
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
							Probes: []*logicalpb.Probe{
								&logicalpb.Probe{Name: "test"},
							},
						},
					},
				},
			},
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
						},
					},
				},
			},
			expectError:      false,
			expectOldUpdated: true,
		},
		{
			name:                    "existing terminated tracepoint",
			originalTracepointState: statuspb.TERMINATED_STATE,
			originalTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
									Name:   "table1",
									Fields: []string{"abc", "def"},
								},
							},
							Probes: []*logicalpb.Probe{
								&logicalpb.Probe{Name: "test"},
							},
						},
					},
				},
			},
			newTracepoint: &logicalpb.TracepointDeployment{
				Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
					&logicalpb.TracepointDeployment_Tracepoint{
						TableName: "table1",
						Program: &logicalpb.TracepointSpec{
							Outputs: []*logicalpb.Output{
								&logicalpb.Output{
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
			mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

			origID := uuid.NewV4()

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

			var newID uuid.UUID

			if test.expectOldUpdated {
				mockTracepointStore.
					EXPECT().
					DeleteTracepointTTLs([]uuid.UUID{origID}).
					Return(nil)
			}

			if !test.expectError {
				mockTracepointStore.
					EXPECT().
					UpsertTracepoint(gomock.Any(), gomock.Any()).
					DoAndReturn(func(id uuid.UUID, tpInfo *storepb.TracepointInfo) error {
						newID = id
						assert.Equal(t, &storepb.TracepointInfo{
							Tracepoint:    test.newTracepoint,
							Name:          "test_tracepoint",
							ID:            utils.ProtoFromUUID(&id),
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

			tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)
			actualTpID, err := tracepointMgr.CreateTracepoint("test_tracepoint", test.newTracepoint, time.Second*5)
			if test.expectError {
				assert.Equal(t, controllers.ErrTracepointAlreadyExists, err)
			} else {
				assert.Nil(t, err)
				assert.Equal(t, &newID, actualTpID)
			}
		})
	}
}

func TestGetTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	tID1 := uuid.NewV4()
	tID2 := uuid.NewV4()
	expectedTracepointInfo := []*storepb.TracepointInfo{
		&storepb.TracepointInfo{
			ID: utils.ProtoFromUUID(&tID1),
		},
		&storepb.TracepointInfo{
			ID: utils.ProtoFromUUID(&tID2),
		},
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetAllTracepoints()
	assert.Nil(t, err)
	assert.Equal(t, expectedTracepointInfo, tracepoints)
}

func TestGetTracepointInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tID1 := uuid.NewV4()
	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	expectedTracepointInfo := &storepb.TracepointInfo{
		ID: utils.ProtoFromUUID(&tID1),
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tID1).
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetTracepointInfo(tID1)
	assert.Nil(t, err)
	assert.Equal(t, expectedTracepointInfo, tracepoints)
}

func TestGetTracepointStates(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	agentUUID1 := uuid.NewV4()
	tID1 := uuid.NewV4()
	expectedTracepointStatus1 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(&tID1),
		AgentID: utils.ProtoFromUUID(&agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	agentUUID2 := uuid.NewV4()
	expectedTracepointStatus2 := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(&tID1),
		AgentID: utils.ProtoFromUUID(&agentUUID2),
		State:   statuspb.PENDING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		GetTracepointStates(tID1).
		Return([]*storepb.AgentTracepointStatus{expectedTracepointStatus1, expectedTracepointStatus2}, nil)

	tracepoints, err := tracepointMgr.GetTracepointStates(tID1)
	assert.Nil(t, err)
	assert.Equal(t, expectedTracepointStatus1, tracepoints[0])
	assert.Equal(t, expectedTracepointStatus2, tracepoints[1])
}

func TestRegisterTracepoint(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	natsPort, natsCleanup := testingutils.StartNATS(t)
	nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
	if err != nil {
		t.Fatal(err)
	}
	defer natsCleanup()

	tracepointMgr := controllers.NewTracepointManager(nc, mockTracepointStore)

	agentUUID := uuid.NewV4()
	tracepointID := uuid.NewV4()
	program := &logicalpb.TracepointDeployment{
		Tracepoints: []*logicalpb.TracepointDeployment_Tracepoint{
			&logicalpb.TracepointDeployment_Tracepoint{
				TableName: "test",
			},
			&logicalpb.TracepointDeployment_Tracepoint{
				TableName: "anotherTracepoint",
			},
		},
	}

	var wg sync.WaitGroup
	wg.Add(1)

	mdSub, err := nc.Subscribe("/agent/"+agentUUID.String(), func(msg *nats.Msg) {
		vzMsg := &messages.VizierMessage{}
		proto.Unmarshal(msg.Data, vzMsg)
		req := vzMsg.GetTracepointMessage().GetRegisterTracepointRequest()
		assert.NotNil(t, req)
		assert.Equal(t, utils.ProtoFromUUID(&tracepointID), req.ID)
		assert.Equal(t, program, req.TracepointDeployment)
		wg.Done()
	})
	assert.Nil(t, err)
	defer mdSub.Unsubscribe()

	go tracepointMgr.RegisterTracepoint([]uuid.UUID{agentUUID}, tracepointID, program)

	defer wg.Wait()
}

func TestUpdateAgentTracepointStatus(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	agentUUID1 := uuid.NewV4()
	tpID := uuid.NewV4()
	expectedTracepointState := &storepb.AgentTracepointStatus{
		ID:      utils.ProtoFromUUID(&tpID),
		AgentID: utils.ProtoFromUUID(&agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(expectedTracepointState).
		Return(nil)

	tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(&tpID), utils.ProtoFromUUID(&agentUUID1), statuspb.RUNNING_STATE, nil)
}

func TestUpdateAgentTracepointStatus_Terminated(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	agentUUID1 := uuid.NewV4()
	tpID := uuid.NewV4()
	agentUUID2 := uuid.NewV4()

	mockTracepointStore.
		EXPECT().
		GetTracepointStates(tpID).
		Return([]*storepb.AgentTracepointStatus{
			&storepb.AgentTracepointStatus{AgentID: utils.ProtoFromUUID(&agentUUID1), State: statuspb.TERMINATED_STATE},
			&storepb.AgentTracepointStatus{AgentID: utils.ProtoFromUUID(&agentUUID2), State: statuspb.RUNNING_STATE},
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepoint(tpID).
		Return(nil)

	tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(&tpID), utils.ProtoFromUUID(&agentUUID2), statuspb.TERMINATED_STATE, nil)
}

func TestWatchTTLs(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	natsPort, natsCleanup := testingutils.StartNATS(t)
	nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
	if err != nil {
		t.Fatal(err)
	}
	defer natsCleanup()

	tracepointMgr := controllers.NewTracepointManager(nc, mockTracepointStore)

	watcherQuitCh := make(chan bool, 1)
	fakeEvCh := make(chan uuid.UUID, 2)
	quitCh := make(chan bool, 1)
	defer func() { watcherQuitCh <- true }()

	tpID1 := uuid.NewV4()
	agentID := uuid.NewV4()

	mockTracepointStore.
		EXPECT().
		WatchTracepointTTLs().
		Return(fakeEvCh, quitCh)

	mockTracepointStore.
		EXPECT().
		GetTracepoint(tpID1).
		Return(&storepb.TracepointInfo{ExpectedState: statuspb.RUNNING_STATE}, nil)

	mockTracepointStore.
		EXPECT().
		UpsertTracepoint(tpID1, &storepb.TracepointInfo{ExpectedState: statuspb.TERMINATED_STATE}).
		Return(nil)

	mockTracepointStore.
		EXPECT().
		GetAgents().
		Return([]*agentpb.Agent{&agentpb.Agent{Info: &agentpb.AgentInfo{AgentID: utils.ProtoFromUUID(&agentID)}}}, nil)

	go func() {
		fakeEvCh <- tpID1
	}()

	go tracepointMgr.WatchTTLs(watcherQuitCh)

	var wg sync.WaitGroup
	wg.Add(1)

	mdSub, err := nc.Subscribe("/agent/"+agentID.String(), func(msg *nats.Msg) {
		vzMsg := &messages.VizierMessage{}
		proto.Unmarshal(msg.Data, vzMsg)
		req := vzMsg.GetTracepointMessage().GetRemoveTracepointRequest()
		assert.NotNil(t, req)
		assert.Equal(t, utils.ProtoFromUUID(&tpID1), req.ID)
		wg.Done()
	})
	assert.Nil(t, err)
	defer mdSub.Unsubscribe()

	defer wg.Wait()
}

func TestUpdateAgentTracepointStatus_SyncTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	tpID1 := uuid.NewV4()
	tpID2 := uuid.NewV4()

	mockTracepointStore.
		EXPECT().
		GetTracepointTTLs().
		Return([]uuid.UUID{
			tpID1,
		}, nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		Return([]*storepb.TracepointInfo{
			&storepb.TracepointInfo{
				ID: utils.ProtoFromUUID(&tpID1),
			},
			&storepb.TracepointInfo{
				ID: utils.ProtoFromUUID(&tpID2),
			},
		}, nil)

	mockTracepointStore.
		EXPECT().
		DeleteTracepoint(tpID2).
		Return(nil)

	err := tracepointMgr.SyncTracepoints()
	assert.Nil(t, err)
}

func TestUpdateAgentTracepointStatus_RemoveTracepoints(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
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

	err := tracepointMgr.RemoveTracepoints([]string{"test1", "test2"})
	assert.Nil(t, err)
}
