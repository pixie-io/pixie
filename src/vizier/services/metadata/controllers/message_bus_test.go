package controllers

import (
	"fmt"
	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/gnatsd/server"
	"github.com/nats-io/gnatsd/test"
	"github.com/nats-io/go-nats"
	"github.com/phayes/freeport"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"testing"
	"time"
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

var heartbeatAckPB = `
heartbeat_ack {
	time: 10
}
`

var heartbeatPB = `
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

func getTestNATSInstance(t *testing.T, port int) *nats.Conn {
	clock := testingutils.NewTestClock(time.Unix(0, 10))
	_, err := newMessageBusController(getNATSURL(port), "agent_update", clock)
	assert.Equal(t, err, nil)

	nc, err := nats.Connect(getNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	return nc
}

func TestAgentRegisterRequest(t *testing.T) {
	port, cleanup := startNATS(t)
	defer cleanup()

	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Create Metadata Service controller.
	nc := getTestNATSInstance(t, port)

	// Listen for response.
	sub, err := nc.SubscribeSync(GetAgentTopic(uuidStr))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	req := new(messages.VizierMessage)
	if err := proto.UnmarshalText(registerAgentRequestPB, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	reqPb, err := req.Marshal()

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{},
	}
	respPb, err := resp.Marshal()

	// Send update.
	nc.Publish("agent_update", reqPb)

	// Wait and read reponse.
	m, err := sub.NextMsg(time.Second)
	assert.Equal(t, m.Data, respPb)
}

func TestAgentUpdateRequest(t *testing.T) {
	port, cleanup := startNATS(t)
	defer cleanup()

	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Create Metadata Service controller.
	nc := getTestNATSInstance(t, port)

	// Listen for response.
	sub, err := nc.SubscribeSync(GetAgentTopic(uuidStr))
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

func TestAgentHeartbeat(t *testing.T) {
	port, cleanup := startNATS(t)
	defer cleanup()

	// Create request and expected response protos.
	uuidStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

	// Create Metadata Service controller.
	nc := getTestNATSInstance(t, port)

	// Listen for response.
	sub, err := nc.SubscribeSync(GetAgentTopic(uuidStr))
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
