package controllers_test

import (
	"errors"
	"fmt"
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
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
)

var registerAgentRequestPB = `
register_agent_request {
	info {
		agent_id {
			data: "11285cdd1de94ab1ae6a0ba08c8c676c"
		}
		host_info {
			hostname: "test-host"
		}
	}
}
`

var invalidRegisterAgentRequestPB = `
register_agent_request {
	info {
		agent_id {
			data: "11285cdd1de94ab1ae6a0ba08c8c676c11285cdd1de94ab1ae6a0ba08c8c676c"
		}
		host_info {
			hostname: "test-host"
		}
	}
}
`

var updateAgentRequestPB = `
update_agent_request {
	info {
		agent_id {
			data: "11285cdd1de94ab1ae6a0ba08c8c676c"
		}
		host_info {
			hostname: "test-host"
		}
	}
}
`

var invalidUpdateAgentRequestPB = `
update_agent_request {
	info {
		agent_id {
			data: "11285cdd1de94ab1ae6a0ba08c8c676c11285cdd1de94ab1ae6a0ba08c8c676c"
		}
		host_info {
			hostname: "test-host"
		}
	}
}
`

var heartbeatAckPB = `
heartbeat_ack {
	time: 10
	update_info {
		updates {
			pod_update {
				uid:  "podUid"
				name: "podName"	
			}		
		}
	}
}
`

var heartbeatPB = `
heartbeat {
	time: 1,
	agent_id: {
		data: "11285cdd1de94ab1ae6a0ba08c8c676c"
	}
	update_info {
		containers {
			name: "container_1"
			uid: "c_abcd"
		}
	}
}
`

var invalidHeartbeatPB = `
heartbeat {
	time: 1,
	agent_id: {
		data: "11285cdd1de94ab1ae6a0ba08c8c676c"
	}
}
`

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

func getTestNATSInstance(t *testing.T, port int, agtMgr controllers.AgentManager) (*nats.Conn, *controllers.MessageBusController) {
	viper.Set("disable_ssl", true)
	clock := testingutils.NewTestClock(time.Unix(0, 10))

	mc, err := controllers.NewTestMessageBusController(getNATSURL(port), "agent_update", agtMgr, clock)
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

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
		Hostname:        "test-host",
		AgentID:         u,
	}

	mockAgtMgr.
		EXPECT().
		CreateAgent(agentInfo).
		Return(nil)

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
		GetMetadataUpdates().
		Return(&updates, nil)

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(registerAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				UpdateInfo: &messages.MetadataUpdateInfo{
					Updates: updates,
				},
			},
		},
	}
	respPb, err := resp.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentMetadataUpdatesFailed(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	_, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)

	mockAgtMgr.
		EXPECT().
		GetMetadataUpdates().
		Return(nil, errors.New("Could not get metadata info"))

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(registerAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}

func TestAgentRegisterRequestInvalidUUID(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(invalidRegisterAgentRequestPB, req); err != nil {
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
	u, err := uuid.FromString(uuidStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 10,
		CreateTimeNS:    10,
		Hostname:        "test-host",
		AgentID:         u,
	}

	mockAgtMgr.
		EXPECT().
		CreateAgent(agentInfo).
		Return(errors.New("could not create agent"))

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
		GetMetadataUpdates().
		Return(&updates, nil)

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(registerAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
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

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

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
	if err := proto.UnmarshalText(updateAgentRequestPB, req); err != nil {
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

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(invalidUpdateAgentRequestPB, req); err != nil {
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

	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		Return(nil)

	updatePb := metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}
	updates := []metadatapb.ResourceUpdate{updatePb}

	mockAgtMgr.
		EXPECT().
		GetFromAgentQueue(uuidStr).
		Return(&updates, nil)

	agentContainers := make([]*metadatapb.ContainerInfo, 1)
	agentContainers[0] = &metadatapb.ContainerInfo{
		Name: "container_1",
		UID:  "c_abcd",
	}
	agentUpdatePb := &messages.AgentUpdateInfo{
		Containers: agentContainers,
	}

	mockAgtMgr.
		EXPECT().
		AddToUpdateQueue(u, agentUpdatePb).
		Return()

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(heartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := new(messages.VizierMessage)
	if err := proto.UnmarshalText(heartbeatAckPB, resp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	respPb, err := resp.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
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

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		Return(nil)

	updates := []metadatapb.ResourceUpdate{}
	mockAgtMgr.
		EXPECT().
		GetFromAgentQueue(uuidStr).
		Return(&updates, nil)

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(invalidHeartbeatPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}

func TestUpdateHeartbeatFailed(t *testing.T) {
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

	mockAgtMgr.
		EXPECT().
		UpdateHeartbeat(u).
		Return(errors.New("could not update heartbeat"))

	updates := []metadatapb.ResourceUpdate{}
	mockAgtMgr.
		EXPECT().
		GetFromAgentQueue(uuidStr).
		Return(&updates, nil)

	agentContainers := make([]*metadatapb.ContainerInfo, 1)
	agentContainers[0] = &metadatapb.ContainerInfo{
		Name: "container_1",
		UID:  "c_abcd",
	}
	agentUpdatePb := &messages.AgentUpdateInfo{
		Containers: agentContainers,
	}

	mockAgtMgr.
		EXPECT().
		AddToUpdateQueue(u, agentUpdatePb).
		Return()

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(heartbeatPB, req); err != nil {
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

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

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

	// Create Metadata Service controller.
	nc, _ := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	_, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(heartbeatAckPB, req); err != nil {
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

	// Create Metadata Service controller.
	nc, mc := getTestNATSInstance(t, port, mockAgtMgr)

	// Listen for response.
	sub, err := nc.SubscribeSync(controllers.GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(registerAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	mc.Close()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	_, err = sub.NextMsg(time.Second)
}
