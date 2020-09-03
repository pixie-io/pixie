package controllers

import (
	"fmt"
	"reflect"
	"sync"
	"time"

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

	// Map from agent ID -> the agentHandler that's responsible for handling that particular
	// agent's messages.
	agentMap map[uuid.UUID]*AgentHandler
	// Mutex that should be locked when acessing or deleting from the agent map.
	mapMu sync.Mutex
}

// AgentHandler is responsible for handling messages for a specific agent.
type AgentHandler struct {
	id                uuid.UUID
	clock             utils.Clock
	agentManager      AgentManager
	tracepointManager *TracepointManager
	mdStore           MetadataStore
	atl               *AgentTopicListener

	MsgChannel chan *nats.Msg
}

// NewAgentTopicListener creates a new agent topic listener.
func NewAgentTopicListener(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore, sendMsgFn SendMessageFn) (*AgentTopicListener, error) {
	clock := utils.SystemClock{}
	return NewAgentTopicListenerWithClock(agentManager, tracepointManager, mdStore, sendMsgFn, clock)
}

// NewAgentTopicListenerWithClock creates a new agent topic listener with a clock.
func NewAgentTopicListenerWithClock(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore, sendMsgFn SendMessageFn, clock utils.Clock) (*AgentTopicListener, error) {
	atl := &AgentTopicListener{
		clock:             clock,
		agentManager:      agentManager,
		tracepointManager: tracepointManager,
		sendMessage:       sendMsgFn,
		mdStore:           mdStore,
		agentMap:          make(map[uuid.UUID]*AgentHandler),
	}

	// Initialize map with existing agents.
	err := atl.Initialize()
	if err != nil {
		return nil, err
	}

	return atl, nil
}

// Initialize loads in the current agent state into the agent topic listener.
func (a *AgentTopicListener) Initialize() error {
	agents, err := a.agentManager.GetActiveAgents()
	if err != nil {
		return err
	}
	a.mapMu.Lock()
	defer a.mapMu.Unlock()

	for _, agent := range agents {
		agentID, err := utils.UUIDFromProto(agent.Info.AgentID)
		if err != nil {
			return err
		}

		a.createAgentHandler(agentID)
	}

	return nil
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
		a.forwardAgentHeartBeat(m.Heartbeat, msg)
	case *messages.VizierMessage_RegisterAgentRequest:
		a.forwardAgentRegisterRequest(m.RegisterAgentRequest, msg)
	// TODO(michelle): All of the above messages now are placed in a separate channel to unblock
	// this message handler, except for the TracepointRequest below. We should consider putting this on a separate
	// channel or have this run in a separate goroutine.
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

// SendMessageToAgent sends the given message to the agent over the NATS agent channel.
func (a *AgentTopicListener) SendMessageToAgent(agentID uuid.UUID, msg messages.VizierMessage) error {
	topic := GetAgentTopicFromUUID(agentID)
	b, err := msg.Marshal()
	if err != nil {
		return err
	}

	return a.sendMessage(topic, b)
}

// This function should only be called when the mutex is already held. It creates a new agent handler for the given id.
func (a *AgentTopicListener) createAgentHandler(agentID uuid.UUID) *AgentHandler {
	if ah, ok := a.agentMap[agentID]; ok {
		log.WithField("agentID", agentID.String()).Info("Trying to create agent handler that already exists")
		return ah
	}

	newAgentHandler := &AgentHandler{
		id:                agentID,
		clock:             a.clock,
		agentManager:      a.agentManager,
		tracepointManager: a.tracepointManager,
		mdStore:           a.mdStore,
		atl:               a,
		MsgChannel:        make(chan *nats.Msg, 10),
	}
	a.agentMap[agentID] = newAgentHandler
	go newAgentHandler.ProcessMessages()

	return newAgentHandler
}

func (a *AgentTopicListener) forwardAgentHeartBeat(m *messages.Heartbeat, msg *nats.Msg) {
	// Check if this is a known agent and forward it to that agentHandler if it exists.
	// Otherwise, send back a NACK because the agent doesn't exist.
	agentID, err := utils.UUIDFromProto(m.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
		return
	}

	var agentHandler *AgentHandler

	a.mapMu.Lock()
	if ah, ok := a.agentMap[agentID]; ok {
		agentHandler = ah
	}
	a.mapMu.Unlock()

	if agentHandler != nil {
		// Add to agent handler to process.
		agentHandler.MsgChannel <- msg
	} else {
		log.WithField("agentID", agentID.String()).Info("Received heartbeat for agent whose agenthandler doesn't exist. Sending NACK.")
		// Agent does not exist. Send NACK.
		resp := messages.VizierMessage{
			Msg: &messages.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messages.HeartbeatNack{},
			},
		}
		a.SendMessageToAgent(agentID, resp)
	}
}

func (a *AgentTopicListener) forwardAgentRegisterRequest(m *messages.RegisterAgentRequest, msg *nats.Msg) {
	agentID, err := utils.UUIDFromProto(m.Info.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
		return
	}

	var agentHandler *AgentHandler

	a.mapMu.Lock()
	if ah, ok := a.agentMap[agentID]; ok {
		agentHandler = ah
	}
	a.mapMu.Unlock()

	if agentHandler != nil {
		// Add to agent handler to process.
		agentHandler.MsgChannel <- msg
	} else {
		newAgentHandler := a.createAgentHandler(agentID)
		newAgentHandler.MsgChannel <- msg
	}
}

// DeleteAgent deletes the agent from the map. The agent should already be deleted in the agent manager.
func (a *AgentTopicListener) DeleteAgent(agentID uuid.UUID) {
	a.mapMu.Lock()
	defer a.mapMu.Unlock()

	delete(a.agentMap, agentID)
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
	err := a.tracepointManager.UpdateAgentTracepointStatus(m.ID, m.AgentID, m.State, m.Status)
	if err != nil {
		log.WithError(err).Error("Could not update agent tracepoint status")
	}
}

// ProcessMessages handles all of the agent messages for this agent. If it does not receive a heartbeat from the agent
// after a certain amount of time, it will declare the agent dead and perform deletion.
func (ah *AgentHandler) ProcessMessages() {
	defer ah.Stop()
	timer := time.NewTimer(AgentExpirationTimeout)
	for {
		select {
		case msg := <-ah.MsgChannel:
			if !timer.Reset(AgentExpirationTimeout) {
				<-timer.C
				continue
			}

			pb := &messages.VizierMessage{}
			proto.Unmarshal(msg.Data, pb)

			switch m := pb.Msg.(type) {
			case *messages.VizierMessage_Heartbeat:
				ah.onAgentHeartbeat(m.Heartbeat)
			case *messages.VizierMessage_RegisterAgentRequest:
				ah.onAgentRegisterRequest(m.RegisterAgentRequest)
			default:
				log.WithField("message-type", reflect.TypeOf(pb.Msg).String()).
					Error("Unhandled message.")
			}
		case <-timer.C:
			log.WithField("agentID", ah.id.String()).Info("AgentHandler timed out, deleting agent")
			return
		}
	}
}

func (ah *AgentHandler) onAgentRegisterRequest(m *messages.RegisterAgentRequest) {
	// Create RegisterAgentResponse.
	agentID := ah.id
	log.WithField("agent", agentID.String()).Infof("Received AgentRegisterRequest for agent")

	// Create agent in agent manager.
	agentInfo := &agentpb.Agent{
		Info:            m.Info,
		LastHeartbeatNS: ah.clock.Now().UnixNano(),
		CreateTimeNS:    ah.clock.Now().UnixNano(),
	}

	asid, err := ah.agentManager.RegisterAgent(agentInfo)
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

	err = ah.atl.SendMessageToAgent(agentID, resp)
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
		updates, err := ah.agentManager.GetMetadataUpdates(hostname)
		if err != nil {
			log.WithError(err).Error("Could not get metadata updates.")
			return
		}

		log.WithField("agent", agentID.String()).WithField("updates", updates).Trace("Queuing up initial updates for agent")
		err = ah.agentManager.AddUpdatesToAgentQueue(agentID.String(), updates)
		if err != nil {
			log.WithError(err).Error("Could not add initial metadata updates to agent's queue")
		}

		// Register all tracepoints on new agent.
		tracepoints, err := ah.tracepointManager.GetAllTracepoints()
		if err != nil {
			log.WithError(err).Error("Could not get all tracepoints")
			return
		}

		agentIDs := []uuid.UUID{agentID}

		for _, tracepoint := range tracepoints {
			if tracepoint.ExpectedState != statuspb.TERMINATED_STATE {
				err = ah.tracepointManager.RegisterTracepoint(agentIDs, utils.UUIDFromProtoOrNil(tracepoint.ID), tracepoint.Tracepoint)
				if err != nil {
					log.WithError(err).Error("Failed to send RegisterTracepoint request")
				}
			}
		}
	}()
}

func (ah *AgentHandler) onAgentHeartbeat(m *messages.Heartbeat) {
	agentID := ah.id

	// Update agent's heartbeat in agent manager.
	err := ah.agentManager.UpdateHeartbeat(agentID)
	if err != nil {
		log.WithError(err).Error("Could not update agent heartbeat.")
		resp := messages.VizierMessage{
			Msg: &messages.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messages.HeartbeatNack{},
			},
		}
		ah.atl.SendMessageToAgent(agentID, resp)
		return
	}

	// Get any queued agent updates.
	updates, err := ah.agentManager.GetFromAgentQueue(agentID.String())

	// Create heartbeat ACK message.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatAck{
			HeartbeatAck: &messages.HeartbeatAck{
				Time: ah.clock.Now().UnixNano(),
				UpdateInfo: &messages.MetadataUpdateInfo{
					Updates:     updates,
					ServiceCIDR: ah.mdStore.GetServiceCIDR(),
					PodCIDRs:    ah.mdStore.GetPodCIDRs(),
				},
				SequenceNumber: m.SequenceNumber,
			},
		},
	}

	err = ah.atl.SendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send heartbeat ack to agent.")
		// Add updates back to the queue, so that they can be sent in the next ack.
		for i := len(updates) - 1; i >= 0; i-- {
			ah.agentManager.AddToFrontOfAgentQueue(agentID.String(), updates[i])
		}
	}

	// Get agent's container/schema updates and add to update queue.
	if m.UpdateInfo != nil {
		ah.agentManager.AddToUpdateQueue(agentID, m.UpdateInfo)
	}
}

// Stop stops the agent handler from listening to any messages.
func (ah *AgentHandler) Stop() {
	close(ah.MsgChannel)
	ah.agentManager.DeleteAgent(ah.id)
	ah.atl.DeleteAgent(ah.id)
}
