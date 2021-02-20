package controllers_test

import (
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func setup(t *testing.T, sendMsgFn func(topic string, b []byte) error) (*controllers.AgentTopicListener, *mock_controllers.MockAgentManager, *mock_controllers.MockTracepointStore, func()) {
	ctrl := gomock.NewController(t)

	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockTracepointStore := mock_controllers.NewMockTracepointStore(ctrl)

	// Load some existing agents.
	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return([]*agentpb.Agent{
			&agentpb.Agent{
				LastHeartbeatNS: 0,
				CreateTimeNS:    0,
				Info: &agentpb.AgentInfo{
					AgentID: &uuidpb.UUID{Data: []byte("5ba7b8109dad11d180b400c04fd430c8")},
					HostInfo: &agentpb.HostInfo{
						Hostname: "abcd",
						HostIP:   "127.0.0.3",
					},
					Capabilities: &agentpb.AgentCapabilities{
						CollectsData: false,
					},
				},
				ASID: 789,
			},
		}, nil)

	clock := testingutils.NewTestClock(time.Unix(0, 10))

	tracepointMgr := controllers.NewTracepointManager(mockTracepointStore, mockAgtMgr, 5*time.Second)
	atl, _ := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, tracepointMgr, sendMsgFn, clock)

	cleanup := func() {
		ctrl.Finish()
		tracepointMgr.Close()
	}

	return atl, mockAgtMgr, mockTracepointStore, cleanup
}

func TestAgentRegisterRequest(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID: 1,
			},
		},
	}
	respPb, err := resp.Marshal()

	// Set up mock.
	var wg sync.WaitGroup
	wg.Add(1)
	atl, mockAgtMgr, mockTracepointStore, cleanup := setup(t, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "Agent/"+uuidStr, topic)
		return nil
	})
	defer cleanup()

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"", "127.0.0.1"}).
		Return("", nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		DoAndReturn(func() ([]*storepb.TracepointInfo, error) {
			wg.Done()
			return nil, nil
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetRegisterAgentRequest().Info.Capabilities = &agentpb.AgentCapabilities{
		CollectsData: true,
	}
	reqPb, err := req.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			return uint32(1), nil
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)

	defer wg.Wait()
}

func TestKelvinRegisterRequest(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID: 1,
			},
		},
	}
	respPb, err := resp.Marshal()

	// Set up mock.
	var wg sync.WaitGroup
	wg.Add(1)
	atl, mockAgtMgr, mockTracepointStore, cleanup := setup(t, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "Agent/"+uuidStr, topic)
		return nil
	})
	defer cleanup()

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: false,
			},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"test-host", "127.0.0.1"}).
		Return("", nil)

	mockTracepointStore.
		EXPECT().
		GetTracepoints().
		DoAndReturn(func() ([]*storepb.TracepointInfo, error) {
			wg.Done()
			return nil, nil
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterKelvinRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			return uint32(1), nil
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)

	defer wg.Wait()
}

func TestAgentRegisterRequestInvalidUUID(t *testing.T) {
	// Set up mock.
	atl, _, _, cleanup := setup(t, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	})
	defer cleanup()

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidRegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentCreateFailed(t *testing.T) {
	var wg sync.WaitGroup
	wg.Add(1)
	atl, mockAgtMgr, _, cleanup := setup(t, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	})
	defer cleanup()

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: false,
			},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetRegisterAgentRequest().Info.Capabilities = &agentpb.AgentCapabilities{
		CollectsData: false,
	}
	reqPb, err := req.Marshal()

	mockAgtMgr.
		EXPECT().
		GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"test-host", "127.0.0.1"}).
		Return("", nil)

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			wg.Done()
			return uint32(0), errors.New("could not create agent")
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)

	defer wg.Wait()
}

func TestAgentHeartbeat(t *testing.T) {
	uuidStr := "5ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Create request and expected response protos.
	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetHeartbeat().AgentID = &uuidpb.UUID{Data: []byte("5ba7b8109dad11d180b400c04fd430c8")}
	reqPb, err := req.Marshal()

	resp := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, resp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	respPb, err := resp.Marshal()

	// Set up mock.
	var wg sync.WaitGroup
	atl, mockAgtMgr, _, cleanup := setup(t, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "Agent/"+uuidStr, topic)
		wg.Done()
		return nil
	})
	defer cleanup()

	mockAgtMgr.
		EXPECT().
		GetServiceCIDR().
		Return("10.64.4.0/22")

	mockAgtMgr.
		EXPECT().
		GetPodCIDRs().
		Return([]string{"10.64.4.0/21"})

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(uuid.FromStringOrNil(uuidStr)).
		DoAndReturn(func(agentID uuid.UUID) error {
			return nil
		})

	createdProcesses := make([]*metadatapb.ProcessCreated, 1)
	createdProcesses[0] = &metadatapb.ProcessCreated{
		PID: 1,
	}
	agentUpdatePb := &messages.AgentUpdateInfo{
		ProcessCreated: createdProcesses,
	}

	mockAgtMgr.
		EXPECT().
		ApplyAgentUpdate(&controllers.AgentUpdate{AgentID: uuid.FromStringOrNil(uuidStr), UpdateInfo: agentUpdatePb}).
		DoAndReturn(func(msg *controllers.AgentUpdate) error {
			wg.Done()
			return nil
		})

	wg.Add(2)
	defer wg.Wait()

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentHeartbeat_Failed(t *testing.T) {
	uuidStr := "5ba7b810-9dad-11d1-80b4-00c04fd430c8"

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatNack{
			HeartbeatNack: &messages.HeartbeatNack{
				Reregister: true,
			},
		},
	}
	respPb, err := resp.Marshal()

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	req.GetHeartbeat().AgentID = &uuidpb.UUID{Data: []byte("5ba7b8109dad11d180b400c04fd430c8")}
	reqPb, err := req.Marshal()

	// Set up mock.
	var wg sync.WaitGroup
	atl, mockAgtMgr, _, cleanup := setup(t, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "Agent/"+uuidStr, topic)
		wg.Done()
		return nil
	})
	defer cleanup()

	wg.Add(1)
	defer wg.Wait()

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(uuid.FromStringOrNil(uuidStr)).
		DoAndReturn(func(agentID uuid.UUID) error {
			return errors.New("Could not update heartbeat")
		})

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestHeartbeatNonExisting(t *testing.T) {
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatNack{
			HeartbeatNack: &messages.HeartbeatNack{
				Reregister: true,
			},
		},
	}
	respPb, err := resp.Marshal()

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Set up mock.
	atl, _, _, cleanup := setup(t, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "Agent/11285cdd-1de9-4ab1-ae6a-0ba08c8c676c", topic)
		return nil
	})
	defer cleanup()

	// Send update.
	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestEmptyMessage(t *testing.T) {
	// Set up mock.
	atl, _, _, cleanup := setup(t, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	})
	defer cleanup()
	req := new(messages.VizierMessage)
	reqPb, err := req.Marshal()

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestUnhandledMessage(t *testing.T) {
	// Set up mock.
	atl, _, _, cleanup := setup(t, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	})
	defer cleanup()

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()
	// Send update.
	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentTracepointInfoUpdate(t *testing.T) {
	// Set up mock.
	atl, _, mockTracepointStore, cleanup := setup(t, func(topic string, b []byte) error {
		return nil
	})
	defer cleanup()

	agentID := uuid.NewV4()
	tpID := uuid.NewV4()

	mockTracepointStore.
		EXPECT().
		UpdateTracepointState(&storepb.AgentTracepointStatus{
			ID:      utils.ProtoFromUUID(tpID),
			AgentID: utils.ProtoFromUUID(agentID),
			State:   statuspb.RUNNING_STATE,
		}).
		Return(nil)

	req := &messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_TracepointInfoUpdate{
					TracepointInfoUpdate: &messages.TracepointInfoUpdate{
						ID:      utils.ProtoFromUUID(tpID),
						AgentID: utils.ProtoFromUUID(agentID),
						State:   statuspb.RUNNING_STATE,
					},
				},
			},
		},
	}
	reqPb, err := req.Marshal()
	assert.Nil(t, err)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentStop(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	assert.Nil(t, err)
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatNack{
			HeartbeatNack: &messages.HeartbeatNack{
				Reregister: false,
			},
		},
	}
	respPb, err := resp.Marshal()

	// Set up mock.
	atl, _, _, cleanup := setup(t, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "Agent/"+uuidStr, topic)
		return nil
	})
	defer cleanup()

	atl.StopAgent(u)
}
