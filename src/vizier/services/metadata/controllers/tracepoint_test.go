package controllers_test

import (
	"sync"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	logicalpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir"
	irpb "pixielabs.ai/pixielabs/src/stirling/dynamic_tracing/ir/logical/shared"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

func TestCreateTracepoint(t *testing.T) {
	tests := []struct {
		name               string
		originalTracepoint *logicalpb.Program
		newTracepoint      *logicalpb.Program
		expectedError      bool
		expectOldUpdated   bool
	}{
		{
			name:               "new_tracepoint",
			originalTracepoint: nil,
			newTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			expectedError: false,
		},
		{
			name: "existing tracepoint, no field match",
			originalTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc"},
					},
				},
			},
			newTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			expectedError: true,
		},
		{
			name: "existing tracepoint, no output match",
			originalTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table2",
						Fields: []string{"abc"},
					},
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			newTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			expectedError: true,
		},
		{
			name: "existing tracepoint, match",
			originalTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			newTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			expectedError: true,
		},
		{
			name: "existing tracepoint, not exactly same",
			originalTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
				Probes: []*logicalpb.Probe{
					&logicalpb.Probe{Name: "test"},
				},
			},
			newTracepoint: &logicalpb.Program{
				Outputs: []*irpb.Output{

					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			expectedError:    false,
			expectOldUpdated: true,
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
					GetTracepointWithName("test_tracepoint").
					Return(nil, nil)
			} else {
				mockTracepointStore.
					EXPECT().
					GetTracepointWithName("test_tracepoint").
					Return(&origID, nil)
				mockTracepointStore.
					EXPECT().
					GetTracepoint(origID).
					Return(&storepb.TracepointInfo{
						Program: test.originalTracepoint,
					}, nil)
			}

			var newID uuid.UUID

			if test.expectOldUpdated {
				mockTracepointStore.
					EXPECT().
					UpsertTracepoint(origID, &storepb.TracepointInfo{
						Program:       test.originalTracepoint,
						ExpectedState: statuspb.TERMINATED_STATE,
					}).
					Return(nil)
			}

			if !test.expectedError {
				mockTracepointStore.
					EXPECT().
					UpsertTracepoint(gomock.Any(), gomock.Any()).
					DoAndReturn(func(id uuid.UUID, tpInfo *storepb.TracepointInfo) error {
						newID = id
						assert.Equal(t, &storepb.TracepointInfo{
							Program:        test.newTracepoint,
							TracepointName: "test_tracepoint",
							TracepointID:   utils.ProtoFromUUID(&id),
							ExpectedState:  statuspb.RUNNING_STATE,
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
			}

			tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)
			actualTpID, err := tracepointMgr.CreateTracepoint("test_tracepoint", test.newTracepoint)
			if test.expectedError {
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
			TracepointID: utils.ProtoFromUUID(&tID1),
		},
		&storepb.TracepointInfo{
			TracepointID: utils.ProtoFromUUID(&tID2),
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
		TracepointID: utils.ProtoFromUUID(&tID1),
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
		TracepointID: utils.ProtoFromUUID(&tID1),
		AgentID:      utils.ProtoFromUUID(&agentUUID1),
		State:        statuspb.RUNNING_STATE,
	}

	agentUUID2 := uuid.NewV4()
	expectedTracepointStatus2 := &storepb.AgentTracepointStatus{
		TracepointID: utils.ProtoFromUUID(&tID1),
		AgentID:      utils.ProtoFromUUID(&agentUUID2),
		State:        statuspb.PENDING_STATE,
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
	program := &logicalpb.Program{
		Probes: []*logicalpb.Probe{
			&logicalpb.Probe{
				Name: "test",
			},
			&logicalpb.Probe{
				Name: "anotherTracepoint",
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
		assert.Equal(t, utils.ProtoFromUUID(&tracepointID), req.TracepointID)
		assert.Equal(t, program, req.Program)
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
		TracepointID: utils.ProtoFromUUID(&tpID),
		AgentID:      utils.ProtoFromUUID(&agentUUID1),
		State:        statuspb.RUNNING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(expectedTracepointState).
		Return(nil)

	tracepointMgr.UpdateAgentTracepointStatus(utils.ProtoFromUUID(&tpID), utils.ProtoFromUUID(&agentUUID1), statuspb.RUNNING_STATE, nil)
}
