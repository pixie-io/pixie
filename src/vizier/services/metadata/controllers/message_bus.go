package controllers

import (
	"fmt"
	"reflect"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// MessageBusController handles and responds to any incoming NATS messages.
type MessageBusController struct {
	conn              *nats.Conn
	agentSubscription *nats.Subscription
	ch                chan *nats.Msg
	clock             utils.Clock
	agentManager      AgentManager
	isLeader          *bool
	mdStore           MetadataStore
}

func newMessageBusController(natsURL string, agentTopic string, agentManager AgentManager, mdStore MetadataStore, isLeader *bool, clock utils.Clock) (*MessageBusController, error) {
	var conn *nats.Conn
	var err error
	if viper.GetBool("disable_ssl") {
		conn, err = nats.Connect(natsURL)
	} else {
		conn, err = nats.Connect(natsURL,
			nats.ClientCert(viper.GetString("client_tls_cert"), viper.GetString("client_tls_key")),
			nats.RootCAs(viper.GetString("tls_ca_cert")))
	}

	if err != nil {
		return nil, err
	}

	mc := &MessageBusController{conn: conn, clock: clock, agentManager: agentManager, mdStore: mdStore, isLeader: isLeader}

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

// NewTestMessageBusController creates a new message bus controller where you can specify a test clock.
func NewTestMessageBusController(natsURL string, agentTopic string, agentManager AgentManager, mdStore MetadataStore, isLeader *bool, clock utils.Clock) (*MessageBusController, error) {
	return newMessageBusController(natsURL, agentTopic, agentManager, mdStore, isLeader, clock)
}

// NewMessageBusController creates a new message bus controller.
func NewMessageBusController(natsURL string, agentTopic string, agentManager AgentManager, mdStore MetadataStore, isLeader *bool) (*MessageBusController, error) {
	clock := utils.SystemClock{}
	return newMessageBusController(natsURL, agentTopic, agentManager, mdStore, isLeader, clock)
}

// AgentTopicListener handles any incoming messages on the controller's channel.
func (mc *MessageBusController) AgentTopicListener() {
	for {
		msg, more := <-mc.ch
		if !more {
			return
		}

		if !*mc.isLeader {
			continue
		}

		pb := &messages.VizierMessage{}
		proto.Unmarshal(msg.Data, pb)

		if pb.Msg == nil {
			log.
				Error("Received empty VizierMessage.")
			continue
		}

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
	agentID, err := utils.UUIDFromProto(m.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
	}

	// Update agent's heartbeat in agent manager.
	err = mc.agentManager.UpdateHeartbeat(agentID)
	if err != nil {
		log.WithError(err).Error("Could not update agent heartbeat.")
		resp := messages.VizierMessage{
			Msg: &messages.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messages.HeartbeatNack{},
			},
		}
		mc.sendMessageToAgent(agentID, resp)
		return
	}

	// Get any queued agent updates.
	updates, err := mc.agentManager.GetFromAgentQueue(agentID.String())

	// Create heartbeat ACK message.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatAck{
			HeartbeatAck: &messages.HeartbeatAck{
				Time: mc.clock.Now().UnixNano(),
				UpdateInfo: &messages.MetadataUpdateInfo{
					Updates: updates,
				},
				SequenceNumber: m.SequenceNumber,
			},
		},
	}

	err = mc.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send heartbeat ack to agent.")
		// Add updates back to the queue, so that they can be sent in the next ack.
		for i := len(updates) - 1; i >= 0; i-- {
			mc.agentManager.AddToFrontOfAgentQueue(agentID.String(), updates[i])
		}
	}

	// Get agent's container/schema updates and add to update queue.
	if m.UpdateInfo != nil {
		mc.agentManager.AddToUpdateQueue(agentID, m.UpdateInfo)
	}
}

func (mc *MessageBusController) onAgentRegisterRequest(m *messages.RegisterAgentRequest) {
	// Create RegisterAgentResponse.
	agentID, err := utils.UUIDFromProto(m.Info.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
		return
	}

	log.WithField("agent", agentID.String()).Infof("Received AgentRegisterRequest for agent")

	// Create agent in agent manager.
	agentInfo := &agentpb.Agent{
		Info:            m.Info,
		LastHeartbeatNS: mc.clock.Now().UnixNano(),
		CreateTimeNS:    mc.clock.Now().UnixNano(),
	}

	asid, err := mc.agentManager.RegisterAgent(agentInfo)
	if err != nil {
		log.WithError(err).Error("Could not create agent.")
		return
	}

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID:        asid,
				ClusterCIDR: mc.mdStore.GetClusterCIDR(),
			},
		},
	}

	err = mc.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send registerAgentResponse to agent.")
		return
	}

	hostname := m.Info.HostInfo.Hostname
	if m.Info.Capabilities != nil && !m.Info.Capabilities.CollectsData {
		hostname = ""
	}
	updates, err := mc.agentManager.GetMetadataUpdates(hostname)
	if err != nil {
		log.WithError(err).Error("Could not get metadata updates.")
		return
	}

	log.WithField("agent", agentID.String()).WithField("updates", updates).Infof("Queuing up initial updates for agent")

	err = mc.agentManager.AddUpdatesToAgentQueue(agentID, updates)
	if err != nil {
		log.WithError(err).Error("Could not add initial metadata updates to agent's queue")
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
		return
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
