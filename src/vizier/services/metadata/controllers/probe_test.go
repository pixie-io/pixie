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

func TestCreateProbe(t *testing.T) {
	tests := []struct {
		name          string
		originalProbe *logicalpb.Program
		newProbe      *logicalpb.Program
		expectedError bool
	}{
		{
			name:          "new_probe",
			originalProbe: nil,
			newProbe: &logicalpb.Program{
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
			name: "existing probe, no field match",
			originalProbe: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc"},
					},
				},
			},
			newProbe: &logicalpb.Program{
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
			name: "existing probe, no output match",
			originalProbe: &logicalpb.Program{
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
			newProbe: &logicalpb.Program{
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
			name: "existing probe, match",
			originalProbe: &logicalpb.Program{
				Outputs: []*irpb.Output{
					&irpb.Output{
						Name:   "table1",
						Fields: []string{"abc", "def"},
					},
				},
			},
			newProbe: &logicalpb.Program{
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
			mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

			if test.originalProbe == nil {
				mockProbeStore.
					EXPECT().
					GetProbe("test_probe").
					Return(nil, nil)
			} else {
				mockProbeStore.
					EXPECT().
					GetProbe("test_probe").
					Return(&storepb.ProbeInfo{
						Program: test.originalProbe,
					}, nil)
			}

			if !test.expectedError {
				mockProbeStore.
					EXPECT().
					UpsertProbe("test_probe", &storepb.ProbeInfo{
						Program: test.newProbe,
						ProbeID: "test_probe",
					}).
					Return(nil)
			}

			probeMgr := controllers.NewProbeManager(nil, mockProbeStore)
			probeName, err := probeMgr.CreateProbe("test_probe", test.newProbe)
			if test.expectedError {
				assert.Equal(t, controllers.ErrProbeAlreadyExists, err)
			} else {
				assert.Nil(t, err)
				assert.Equal(t, "test_probe", probeName)
			}
		})
	}
}

func TestGetProbes(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

	expectedProbeInfo := []*storepb.ProbeInfo{
		&storepb.ProbeInfo{
			ProbeID: "test_probe",
		},
		&storepb.ProbeInfo{
			ProbeID: "some_probe",
		},
	}

	mockProbeStore.
		EXPECT().
		GetProbes().
		Return(expectedProbeInfo, nil)

	probes, err := probeMgr.GetAllProbes()
	assert.Nil(t, err)
	assert.Equal(t, expectedProbeInfo, probes)
}

func TestGetProbeInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

	expectedProbeInfo := &storepb.ProbeInfo{
		ProbeID: "test_probe",
	}

	mockProbeStore.
		EXPECT().
		GetProbe("test_probe").
		Return(expectedProbeInfo, nil)

	probes, err := probeMgr.GetProbeInfo("test_probe")
	assert.Nil(t, err)
	assert.Equal(t, expectedProbeInfo, probes)
}

func TestGetProbeStates(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

	agentUUID1 := uuid.NewV4()
	expectedProbeStatus1 := &storepb.AgentProbeStatus{
		ProbeID: "test_probe",
		AgentID: utils.ProtoFromUUID(&agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	agentUUID2 := uuid.NewV4()
	expectedProbeStatus2 := &storepb.AgentProbeStatus{
		ProbeID: "test_probe",
		AgentID: utils.ProtoFromUUID(&agentUUID2),
		State:   statuspb.PENDING_STATE,
	}

	mockProbeStore.
		EXPECT().
		GetProbeStates("test_probe").
		Return([]*storepb.AgentProbeStatus{expectedProbeStatus1, expectedProbeStatus2}, nil)

	probes, err := probeMgr.GetProbeStates("test_probe")
	assert.Nil(t, err)
	assert.Equal(t, expectedProbeStatus1, probes[0])
	assert.Equal(t, expectedProbeStatus2, probes[1])
}

func TestRegisterProbe(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	natsPort, natsCleanup := testingutils.StartNATS(t)
	nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
	if err != nil {
		t.Fatal(err)
	}
	defer natsCleanup()

	probeMgr := controllers.NewProbeManager(nc, mockProbeStore)

	agentUUID := uuid.NewV4()
	probeID := "test_probe"
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

	var wg sync.WaitGroup
	wg.Add(1)

	mdSub, err := nc.Subscribe("/agent/"+agentUUID.String(), func(msg *nats.Msg) {
		vzMsg := &messages.VizierMessage{}
		proto.Unmarshal(msg.Data, vzMsg)
		req := vzMsg.GetRegisterProbeRequest()
		assert.NotNil(t, req)
		assert.Equal(t, "test_probe", req.ProbeID)
		assert.Equal(t, program, req.Program)
		wg.Done()
	})
	assert.Nil(t, err)
	defer mdSub.Unsubscribe()

	go probeMgr.RegisterProbe([]uuid.UUID{agentUUID}, probeID, program)

	defer wg.Wait()
}

func TestUpdateAgentProbeStatus(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockProbeStore := mock_controllers.NewMockProbeStore(ctrl)

	probeMgr := controllers.NewProbeManager(nil, mockProbeStore)

	agentUUID1 := uuid.NewV4()
	expectedProbeState := &storepb.AgentProbeStatus{
		ProbeID: "test_probe",
		AgentID: utils.ProtoFromUUID(&agentUUID1),
		State:   statuspb.RUNNING_STATE,
	}

	mockProbeStore.
		EXPECT().
		UpdateProbeState(expectedProbeState).
		Return(nil)

	probeMgr.UpdateAgentProbeStatus("test_probe", utils.ProtoFromUUID(&agentUUID1), statuspb.RUNNING_STATE, nil)
}
