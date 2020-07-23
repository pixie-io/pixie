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
	// uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
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
			expectedError: false,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			// Set up mock.
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()
			mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

			if test.originalTracepoint == nil {
				mockTracepointStore.
					EXPECT().
					GetTracepoint("test_tracepoint").
					Return(nil, nil)
			} else {
				mockTracepointStore.
					EXPECT().
					GetTracepoint("test_tracepoint").
					Return(&storepb.TracepointInfo{
						Program: test.originalTracepoint,
					}, nil)
			}

			if !test.expectedError {
				mockTracepointStore.
					EXPECT().
					UpsertTracepoint("test_tracepoint", &storepb.TracepointInfo{
						Program:      test.newTracepoint,
						TracepointID: "test_tracepoint",
					}).
					Return(nil)
			}

			tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)
			tracepointName, err := tracepointMgr.CreateTracepoint("test_tracepoint", test.newTracepoint)
			if test.expectedError {
				assert.Equal(t, controllers.ErrTracepointAlreadyExists, err)
			} else {
				assert.Nil(t, err)
				assert.Equal(t, "test_tracepoint", tracepointName)
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

	expectedTracepointInfo := []*storepb.TracepointInfo{
		&storepb.TracepointInfo{
			TracepointID: "test_tracepoint",
		},
		&storepb.TracepointInfo{
			TracepointID: "some_tracepoint",
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

	tracepointMgr := controllers.NewTracepointManager(nil, mockTracepointStore)

	expectedTracepointInfo := &storepb.TracepointInfo{
		TracepointID: "test_tracepoint",
	}

	mockTracepointStore.
		EXPECT().
		GetTracepoint("test_tracepoint").
		Return(expectedTracepointInfo, nil)

	tracepoints, err := tracepointMgr.GetTracepointInfo("test_tracepoint")
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
	expectedTracepointStatus1 := &storepb.AgentTracepointStatus{
		TracepointID: "test_tracepoint",
		AgentID:      utils.ProtoFromUUID(&agentUUID1),
		State:        statuspb.RUNNING_STATE,
	}

	agentUUID2 := uuid.NewV4()
	expectedTracepointStatus2 := &storepb.AgentTracepointStatus{
		TracepointID: "test_tracepoint",
		AgentID:      utils.ProtoFromUUID(&agentUUID2),
		State:        statuspb.PENDING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		GetTracepointStates("test_tracepoint").
		Return([]*storepb.AgentTracepointStatus{expectedTracepointStatus1, expectedTracepointStatus2}, nil)

	tracepoints, err := tracepointMgr.GetTracepointStates("test_tracepoint")
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
	tracepointID := "test_tracepoint"
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
		assert.Equal(t, "test_tracepoint", req.TracepointID)
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
	expectedTracepointState := &storepb.AgentTracepointStatus{
		TracepointID: "test_tracepoint",
		AgentID:      utils.ProtoFromUUID(&agentUUID1),
		State:        statuspb.RUNNING_STATE,
	}

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(expectedTracepointState).
		Return(nil)

	tracepointMgr.UpdateAgentTracepointStatus("test_tracepoint", utils.ProtoFromUUID(&agentUUID1), statuspb.RUNNING_STATE, nil)
}
