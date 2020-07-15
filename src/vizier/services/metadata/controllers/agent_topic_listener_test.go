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
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func TestAgentRegisterRequest(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	updatePb := metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	updates := []*metadatapb.ResourceUpdate{&updatePb}

	var wg sync.WaitGroup
	wg.Add(1)

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates(&controllers.HostnameIPPair{"test-host", "127.0.0.1"}).
		DoAndReturn(func(hostname *controllers.HostnameIPPair) ([]*metadatapb.ResourceUpdate, error) {
			return updates, nil
		})

	mockAgtMgr.
		EXPECT().
		AddUpdatesToAgentQueue(u.String(), updates).
		DoAndReturn(func(string, []*metadatapb.ResourceUpdate) error {
			wg.Done()
			return nil
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID: 1,
			},
		},
	}
	respPb, err := resp.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			return uint32(1), nil
		})

	// Create Metadata Service controller.
	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "/agent/"+uuidStr, topic)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)

	defer wg.Wait()
}

func TestKelvinRegisterRequest(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

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

	updatePb := metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	updates := []*metadatapb.ResourceUpdate{&updatePb}

	var wg sync.WaitGroup
	wg.Add(1)

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates(nil).
		DoAndReturn(func(hostname *controllers.HostnameIPPair) ([]*metadatapb.ResourceUpdate, error) {
			return updates, nil
		})

	mockAgtMgr.
		EXPECT().
		AddUpdatesToAgentQueue(u.String(), updates).
		DoAndReturn(func(string, []*metadatapb.ResourceUpdate) error {
			wg.Done()
			return nil
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterKelvinRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID: 1,
			},
		},
	}
	respPb, err := resp.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			return uint32(1), nil
		})

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "/agent/"+uuidStr, topic)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)

	defer wg.Wait()
}

func TestAgentMetadataUpdatesFailed(t *testing.T) {
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			return uint32(1), nil
		})

	updatePb := metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	updates := []*metadatapb.ResourceUpdate{&updatePb}

	var wg sync.WaitGroup
	wg.Add(1)

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates(&controllers.HostnameIPPair{"test-host", "127.0.0.1"}).
		DoAndReturn(func(hostname *controllers.HostnameIPPair) ([]*metadatapb.ResourceUpdate, error) {
			wg.Done()
			return updates, errors.New("Could not get metadata info")
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID: 1,
			},
		},
	}
	respPb, err := resp.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "/agent/"+uuidStr, topic)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)

	defer wg.Wait()
}

func TestAgentRegisterRequestInvalidUUID(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidRegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentCreateFailed(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
				HostIP:   "127.0.0.1",
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			return uint32(0), errors.New("could not create agent")
		})

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentUpdateRequest(t *testing.T) {
	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_UpdateAgentResponse{},
	}
	respPb, err := resp.Marshal()

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.UpdateAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "/agent/"+uuidStr, topic)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentUpdateRequestInvalidUUID(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidUpdateAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentHeartbeat(t *testing.T) {
	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	mockMdStore.
		EXPECT().
		GetServiceCIDR().
		Return("10.64.4.0/22")

	mockMdStore.
		EXPECT().
		GetPodCIDRs().
		Return([]string{"10.64.4.0/21"})

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		DoAndReturn(func(agentID uuid.UUID) error {
			return nil
		})

	updatePb1 := metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}
	updatePb2 := metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid2",
				Name: "podName2",
			},
		},
	}
	updates := []*metadatapb.ResourceUpdate{&updatePb1, &updatePb2}

	mockAgtMgr.
		EXPECT().
		GetFromAgentQueue(uuidStr).
		DoAndReturn(func(string) ([]*metadatapb.ResourceUpdate, error) {
			return updates, nil
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
		AddToUpdateQueue(u, agentUpdatePb).
		DoAndReturn(func(uuid.UUID, *messages.AgentUpdateInfo) {
			return
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, resp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	respPb, err := resp.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "/agent/"+uuidStr, topic)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestAgentHeartbeat_Failed(t *testing.T) {
	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		DoAndReturn(func(agentID uuid.UUID) error {
			return errors.New("Could not update heartbeat")
		})

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatNack{
			HeartbeatNack: &messages.HeartbeatNack{},
		},
	}
	respPb, err := resp.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		assert.Equal(t, respPb, b)
		assert.Equal(t, "/agent/"+uuidStr, topic)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestEmptyMessage(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	req := new(messages.VizierMessage)
	reqPb, err := req.Marshal()

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}

func TestUnhandledMessage(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()
	// Send update.

	clock := testingutils.NewTestClock(time.Unix(0, 10))
	atl, err := controllers.NewAgentTopicListenerWithClock(mockAgtMgr, mockMdStore, func(topic string, b []byte) error {
		// This function should never be called.
		assert.Equal(t, true, false)
		return nil
	}, clock)

	msg := nats.Msg{}
	msg.Data = reqPb
	err = atl.HandleMessage(&msg)
	assert.Nil(t, err)
}
