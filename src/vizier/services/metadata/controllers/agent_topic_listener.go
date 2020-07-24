package controllers

import (
	"fmt"
	"reflect"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/utils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// AgentTopicListener is responsible for listening to and handling messages on the agent topic.
type AgentTopicListener struct {
	clock             utils.Clock
	agentManager      AgentManager
	tracepointManager *TracepointManager
	sendMessage       SendMessageFn
	mdStore           MetadataStore
}

// NewAgentTopicListener creates a new agent topic listener.
func NewAgentTopicListener(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore, sendMsgFn SendMessageFn) (*AgentTopicListener, error) {
	clock := utils.SystemClock{}
	return NewAgentTopicListenerWithClock(agentManager, tracepointManager, mdStore, sendMsgFn, clock)
}

// NewAgentTopicListenerWithClock creates a new agent topic listener with a clock.
func NewAgentTopicListenerWithClock(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore, sendMsgFn SendMessageFn, clock utils.Clock) (*AgentTopicListener, error) {
	return &AgentTopicListener{
		clock:             clock,
		agentManager:      agentManager,
		tracepointManager: tracepointManager,
		sendMessage:       sendMsgFn,
		mdStore:           mdStore,
	}, nil
}

// HandleMessage handles a message on the agent topic.
func (a *AgentTopicListener) HandleMessage(msg *nats.Msg) error {
	pb := &messages.VizierMessage{}
	proto.Unmarshal(msg.Data, pb)

	if pb.Msg == nil {
		log.
			Error("Received empty VizierMessage.")
		return nil
	}

	switch m := pb.Msg.(type) {
	case *messages.VizierMessage_Heartbeat:
		a.onAgentHeartBeat(m.Heartbeat)
	case *messages.VizierMessage_RegisterAgentRequest:
		a.onAgentRegisterRequest(m.RegisterAgentRequest)
	case *messages.VizierMessage_UpdateAgentRequest:
		a.onAgentUpdateRequest(m.UpdateAgentRequest)
	case *messages.VizierMessage_TracepointMessage:
		a.onAgentTracepointMessage(m.TracepointMessage)
	default:
		log.WithField("message-type", reflect.TypeOf(pb.Msg).String()).
			Error("Unhandled message.")
	}
	return nil
}

// GetAgentTopicFromUUID gets the agent topic given the agent's ID in UUID format.
func GetAgentTopicFromUUID(agentID uuid.UUID) string {
	return GetAgentTopic(agentID.String())
}

// GetAgentTopic gets the agent topic given the agent's ID in string format.
func GetAgentTopic(agentID string) string {
	return fmt.Sprintf("/agent/%s", agentID)
}

func (a *AgentTopicListener) sendMessageToAgent(agentID uuid.UUID, msg messages.VizierMessage) error {
	topic := GetAgentTopicFromUUID(agentID)
	b, err := msg.Marshal()
	if err != nil {
		return err
	}

	return a.sendMessage(topic, b)
}

func (a *AgentTopicListener) onAgentHeartBeat(m *messages.Heartbeat) {
	agentID, err := utils.UUIDFromProto(m.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
	}

	// Update agent's heartbeat in agent manager.
	err = a.agentManager.UpdateHeartbeat(agentID)
	if err != nil {
		log.WithError(err).Error("Could not update agent heartbeat.")
		resp := messages.VizierMessage{
			Msg: &messages.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messages.HeartbeatNack{},
			},
		}
		a.sendMessageToAgent(agentID, resp)
		return
	}

	// Get any queued agent updates.
	updates, err := a.agentManager.GetFromAgentQueue(agentID.String())

	// Create heartbeat ACK message.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatAck{
			HeartbeatAck: &messages.HeartbeatAck{
				Time: a.clock.Now().UnixNano(),
				UpdateInfo: &messages.MetadataUpdateInfo{
					Updates:     updates,
					ServiceCIDR: a.mdStore.GetServiceCIDR(),
					PodCIDRs:    a.mdStore.GetPodCIDRs(),
				},
				SequenceNumber: m.SequenceNumber,
			},
		},
	}

	err = a.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send heartbeat ack to agent.")
		// Add updates back to the queue, so that they can be sent in the next ack.
		for i := len(updates) - 1; i >= 0; i-- {
			a.agentManager.AddToFrontOfAgentQueue(agentID.String(), updates[i])
		}
	}

	// Get agent's container/schema updates and add to update queue.
	if m.UpdateInfo != nil {
		a.agentManager.AddToUpdateQueue(agentID, m.UpdateInfo)
	}
}

func (a *AgentTopicListener) onAgentRegisterRequest(m *messages.RegisterAgentRequest) {
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
		LastHeartbeatNS: a.clock.Now().UnixNano(),
		CreateTimeNS:    a.clock.Now().UnixNano(),
	}

	asid, err := a.agentManager.RegisterAgent(agentInfo)
	if err != nil {
		log.WithError(err).Error("Could not create agent.")
		return
	}

	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messages.RegisterAgentResponse{
				ASID: asid,
			},
		},
	}

	err = a.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send registerAgentResponse to agent.")
		return
	}

	go func() {
		// Fetch metadata updates for this agent in a separate goroutine, since
		// accessing etcd for all metadata updates may take a while.
		hostname := &HostnameIPPair{m.Info.HostInfo.Hostname, m.Info.HostInfo.HostIP}
		if m.Info.Capabilities != nil && !m.Info.Capabilities.CollectsData {
			hostname = nil
		}
		updates, err := a.agentManager.GetMetadataUpdates(hostname)
		if err != nil {
			log.WithError(err).Error("Could not get metadata updates.")
			return
		}

		log.WithField("agent", agentID.String()).WithField("updates", updates).Trace("Queuing up initial updates for agent")
		err = a.agentManager.AddUpdatesToAgentQueue(agentID.String(), updates)
		if err != nil {
			log.WithError(err).Error("Could not add initial metadata updates to agent's queue")
		}

		// Register all tracepoints on new agent.
		tracepoints, err := a.tracepointManager.GetAllTracepoints()
		if err != nil {
			log.WithError(err).Error("Could not get all tracepoints")
			return
		}

		agentIDs := []uuid.UUID{agentID}

		for _, tracepoint := range tracepoints {
			if tracepoint.ExpectedState != statuspb.TERMINATED_STATE {
				err = a.tracepointManager.RegisterTracepoint(agentIDs, utils.UUIDFromProtoOrNil(tracepoint.TracepointID), tracepoint.Program)
				if err != nil {
					log.WithError(err).Error("Failed to send RegisterTracepoint request")
				}
			}
		}
	}()
}

func (a *AgentTopicListener) onAgentUpdateRequest(m *messages.UpdateAgentRequest) {
	// Create UpdateAgentResponse.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_UpdateAgentResponse{},
	}
	agentID, err := utils.UUIDFromProto(m.Info.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
		return
	}
	err = a.sendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send registerAgentResponse to agent.")
	}

	// TODO(michelle): Update agent on etcd through agent manager.
}

func (a *AgentTopicListener) onAgentTracepointMessage(pbMessage *messages.TracepointMessage) {
	switch m := pbMessage.Msg.(type) {
	case *messages.TracepointMessage_TracepointInfoUpdate:
		a.onAgentTracepointInfoUpdate(m.TracepointInfoUpdate)
	default:
		log.WithField("message-type", reflect.TypeOf(pbMessage.Msg).String()).
			Error("Unhandled message.")
	}
}

func (a *AgentTopicListener) onAgentTracepointInfoUpdate(m *messages.TracepointInfoUpdate) {
	err := a.tracepointManager.UpdateAgentTracepointStatus(m.TracepointID, m.AgentID, m.State, m.Status)
	if err != nil {
		log.WithError(err).Error("Could not update agent tracepoint status")
	}
}
