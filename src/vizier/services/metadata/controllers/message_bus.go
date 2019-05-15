package controllers

import (
	"fmt"
	"reflect"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
)

// MessageBusController handles and responds to any incoming NATS messages.
type MessageBusController struct {
	conn              *nats.Conn
	agentSubscription *nats.Subscription
	ch                chan *nats.Msg
	clock             utils.Clock
	agentManager      AgentManager
}

// NewTestMessageBusController creates a new message bus controller where you can specify a test clock.
func NewTestMessageBusController(natsURL string, agentTopic string, agentManager AgentManager, clock utils.Clock) (*MessageBusController, error) {
	conn, err := nats.Connect(natsURL)
	if err != nil {
		return nil, err
	}

	mc := &MessageBusController{conn: conn, clock: clock, agentManager: agentManager}

	ch := make(chan *nats.Msg, 64)

	sub, err := conn.ChanSubscribe(agentTopic, ch)
	if err != nil {
		return nil, err
	}
	mc.ch = ch

	go mc.AgentTopicListener()

	mc.agentSubscription = sub
	return mc, err
}

// NewMessageBusController creates a new message bus controller.
func NewMessageBusController(natsURL string, agentTopic string, agentManager AgentManager) (*MessageBusController, error) {
	clock := utils.SystemClock{}
	return NewTestMessageBusController(natsURL, agentTopic, agentManager, clock)
}

// AgentTopicListener handles any incoming messages on the controller's channel.
func (mc *MessageBusController) AgentTopicListener() {
	for {
		msg, more := <-mc.ch
		if !more {
			return
		}
		pb := &messages.VizierMessage{}
		proto.Unmarshal(msg.Data, pb)

		switch m := pb.Msg.(type) {
		case *messages.VizierMessage_Heartbeat:
			mc.onAgentHeartBeat(m.Heartbeat)
		case *messages.VizierMessage_RegisterAgentRequest:
			mc.onAgentRegisterRequest(m.RegisterAgentRequest)
		case *messages.VizierMessage_UpdateAgentRequest:
			mc.onAgentUpdateRequest(m.UpdateAgentRequest)
		default:
			log.WithField("message-type", reflect.TypeOf(pb.Msg).String()).
				Error("Unhandled message.")
		}
	}
}

// GetAgentTopicFromUUID gets the agent topic given the agent's ID in UUID format.
func GetAgentTopicFromUUID(agentID uuid.UUID) string {
	return GetAgentTopic(agentID.String())
}

// GetAgentTopic gets the agent topic given the agent's ID in string format.
func GetAgentTopic(agentID string) string {
	return fmt.Sprintf("/agent/%s", agentID)
}

func (mc *MessageBusController) sendMessageToAgent(agentID uuid.UUID, msg messages.VizierMessage) error {
	topic := GetAgentTopicFromUUID(agentID)
	b, err := msg.Marshal()
	if err != nil {
		return err
	}

	err = mc.conn.Publish(topic, b)
	if err != nil {
		log.WithError(err).Error("Could not publish message to message bus.")
		return err
	}
	return nil
}

func (mc *MessageBusController) onAgentHeartBeat(m *messages.Heartbeat) {
	// Create heartbeat ACK message.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatAck{
			HeartbeatAck: &messages.HeartbeatAck{
				Time: mc.clock.Now().UnixNano(),
			},
		},
	}

	agentID, err := utils.UUIDFromProto(m.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
	}
	err = mc.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send heartbeat ack to agent.")
	}

	// Update agent's heartbeat in agent manager.
	err = mc.agentManager.UpdateHeartbeat(agentID)
	if err != nil {
		log.WithError(err).Error("Could not update agent heartbeat.")
	}
}

func (mc *MessageBusController) onAgentRegisterRequest(m *messages.RegisterAgentRequest) {
	// Create RegisterAgentResponse.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{},
	}
	agentID, err := utils.UUIDFromProto(m.Info.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
	}
	err = mc.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send registerAgentResponse to agent.")
	}

	// Create agent in agent manager.
	agentInfo := &AgentInfo{
		LastHeartbeatNS: mc.clock.Now().UnixNano(),
		CreateTimeNS:    mc.clock.Now().UnixNano(),
		Hostname:        m.Info.HostInfo.Hostname,
		AgentID:         agentID,
	}
	err = mc.agentManager.CreateAgent(agentInfo)
	if err != nil {
		log.WithError(err).Error("Could not create agent.")
	}
}

func (mc *MessageBusController) onAgentUpdateRequest(m *messages.UpdateAgentRequest) {
	// Create UpdateAgentResponse.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_UpdateAgentResponse{},
	}
	agentID, err := utils.UUIDFromProto(m.Info.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
	}
	err = mc.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send registerAgentResponse to agent.")
	}

	// TODO(michelle): Update agent on etcd through agent manager.
}

// Close closes the subscription and NATS connection.
func (mc *MessageBusController) Close() {
	mc.agentSubscription.Unsubscribe()
	mc.agentSubscription.Drain()

	mc.conn.Drain()
	mc.conn.Close()
}
