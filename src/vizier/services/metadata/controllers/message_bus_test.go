package controllers_test

import (
	"errors"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/gnatsd/server"
	"github.com/nats-io/gnatsd/test"
	"github.com/nats-io/go-nats"
	"github.com/phayes/freeport"
	uuid "github.com/satori/go.uuid"
	"github.com/spf13/viper"
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

var testOptions = server.Options{
	Host:   "localhost",
	NoLog:  true,
	NoSigs: true,
}

func startNATS(t *testing.T) (int, func()) {
	// Find available port.
	port, err := freeport.GetFreePort()
	if err != nil {
		t.Fatal("Could not find free port.")
	}

	testOptions.Port = port
	gnatsd := test.RunServer(&testOptions)
	if gnatsd == nil {
		t.Fail()
	}

	url := getNATSURL(port)
	conn, err := nats.Connect(url)
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	cleanup := func() {
		gnatsd.Shutdown()
		conn.Close()
	}

	return port, cleanup
}

func getNATSURL(port int) string {
	return fmt.Sprintf("nats://%s:%d", testOptions.Host, port)
}

func getTestNATSInstance(t *testing.T, port int, agtMgr controllers.AgentManager, mdStore controllers.MetadataStore, isLeader *bool) (*nats.Conn, *controllers.MessageBusController) {
	viper.Set("disable_ssl", true)
	clock := testingutils.NewTestClock(time.Unix(0, 10))

	mc, err := controllers.NewTestMessageBusController(getNATSURL(port), "agent_update", agtMgr, mdStore, isLeader, clock)
	assert.Equal(t, err, nil)

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	return nc, mc
}

func TestAgentRegisterRequest(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(3)

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
		GetClusterCIDR().
		Return("10.24.0.0/14")

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
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

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates("test-host").
		DoAndReturn(func(hostname string) ([]*metadatapb.ResourceUpdate, error) {
			wg.Done()
			return updates, nil
		})

	mockAgtMgr.
		EXPECT().
		AddUpdatesToAgentQueue(u, updates).
		DoAndReturn(func(uuid.UUID, []*metadatapb.ResourceUpdate) error {
			wg.Done()
			return nil
		})

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID:        1,
				ClusterCIDR: "10.24.0.0/14",
			},
		},
	}
	respPb, err := resp.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			wg.Done()
			return uint32(1), nil
		})

	// Send update.
	nc.Publish("agent_update", reqPb)

	wg.Wait()

	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestKelvinRegisterRequest(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(3)

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
		GetClusterCIDR().
		Return("10.24.0.0/14")

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
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

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates("").
		DoAndReturn(func(hostname string) ([]*metadatapb.ResourceUpdate, error) {
			wg.Done()
			return updates, nil
		})

	mockAgtMgr.
		EXPECT().
		AddUpdatesToAgentQueue(u, updates).
		DoAndReturn(func(uuid.UUID, []*metadatapb.ResourceUpdate) error {
			wg.Done()
			return nil
		})

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterKelvinRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID:        1,
				ClusterCIDR: "10.24.0.0/14",
			},
		},
	}
	respPb, err := resp.Marshal()

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			wg.Done()
			return uint32(1), nil
		})

	// Send update.
	nc.Publish("agent_update", reqPb)

	wg.Wait()

	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentMetadataUpdatesFailed(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(2)

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)
	mockMdStore.
		EXPECT().
		GetClusterCIDR().
		Return("10.24.0.0/14")

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test-host",
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
			wg.Done()
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

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates("test-host").
		DoAndReturn(func(hostname string) ([]*metadatapb.ResourceUpdate, error) {
			wg.Done()
			return updates, errors.New("Could not get metadata info")
		})

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID:        1,
				ClusterCIDR: "10.24.0.0/14",
			},
		},
	}
	respPb, err := resp.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	wg.Wait()
	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentRegisterRequestInvalidUUID(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidRegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}

func TestAgentCreateFailed(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

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
			},
			AgentID: &uuidpb.UUID{Data: []byte("11285cdd1de94ab1ae6a0ba08c8c676c")},
		},
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
	}

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Wait and read reponse.
	var wg sync.WaitGroup
	wg.Add(1)

	mockAgtMgr.
		EXPECT().
		RegisterAgent(agentInfo).
		DoAndReturn(func(info *agentpb.Agent) (uint32, error) {
			wg.Done()
			return uint32(0), errors.New("could not create agent")
		})

	// Send update.
	nc.Publish("agent_update", reqPb)

	defer wg.Wait()
	_, err = sub.NextMsg(time.Second)
}

func TestAgentUpdateRequest(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_UpdateAgentResponse{},
	}
	respPb, err := resp.Marshal()

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.UpdateAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentUpdateRequestInvalidUUID(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidUpdateAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}

func TestAgentHeartbeat(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	var wg sync.WaitGroup
	wg.Add(3)

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		DoAndReturn(func(agentID uuid.UUID) error {
			wg.Done()
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
			wg.Done()
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
			wg.Done()
			return
		})

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

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

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	wg.Wait()
	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentHeartbeat_Failed(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	var wg sync.WaitGroup
	wg.Add(1)

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		DoAndReturn(func(agentID uuid.UUID) error {
			wg.Done()
			return errors.New("Could not update heartbeat")
		})

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

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

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	wg.Wait()
	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentHeartbeatInvalidUUID(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

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
		Return(nil)

	updates := []*metadatapb.ResourceUpdate{}
	mockAgtMgr.
		EXPECT().
		GetFromAgentQueue(uuidStr).
		Return(updates, nil)

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.InvalidHeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}

func TestEmptyMessage(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	_, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)
}

func TestUnhandledMessage(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	// Create Metadata Service controller.
	isLeader := true
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	_, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.HeartbeatAckPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()
	// Send update.
	nc.Publish("agent_update", reqPb)
}

func TestClose(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)
	mockMdStore := mock_controllers.NewMockMetadataStore(ctrl)

	// Create Metadata Service controller.
	isLeader := true
	nc, mc := getTestNATSInstance(t, port, mockAgtMgr, mockMdStore, &isLeader)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(testutils.RegisterAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	mc.Close()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}
