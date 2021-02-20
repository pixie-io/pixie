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

// UpdateAgentTopic is the NATS topic over which agent updates are sent.
const UpdateAgentTopic = "UpdateAgent"
const agentTopicPrefix = "Agent"

// GetAgentTopicFromUUID gets the agent topic given the agent's ID in UUID format.
func GetAgentTopicFromUUID(agentID uuid.UUID) string {
	return GetAgentTopic(agentID.String())
}

// GetAgentTopic gets the agent topic given the agent's ID in string format.
func GetAgentTopic(agentID string) string {
	return fmt.Sprintf("%s/%s", agentTopicPrefix, agentID)
}

type concurrentAgentMap struct {
	unsafeMap map[uuid.UUID]*AgentHandler
	mapMu     sync.RWMutex
}

func (c *concurrentAgentMap) read(agentID uuid.UUID) *AgentHandler {
	c.mapMu.RLock()
	defer c.mapMu.RUnlock()
	return c.unsafeMap[agentID]
}

func (c *concurrentAgentMap) values() []*AgentHandler {
	c.mapMu.RLock()
	defer c.mapMu.RUnlock()
	allHandlers := make([]*AgentHandler, len(c.unsafeMap))
	i := 0
	for _, ah := range c.unsafeMap {
		allHandlers[i] = ah
		i++
	}
	return allHandlers
}

func (c *concurrentAgentMap) write(agentID uuid.UUID, ah *AgentHandler) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	c.unsafeMap[agentID] = ah
}

func (c *concurrentAgentMap) delete(agentID uuid.UUID) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	delete(c.unsafeMap, agentID)
}

// AgentTopicListener is responsible for listening to and handling messages on the agent topic.
type AgentTopicListener struct {
	clock             utils.Clock
	agentManager      AgentManager
	tracepointManager *TracepointManager
	sendMessage       SendMessageFn
	mdStore           MetadataStore

	// Map from agent ID -> the agentHandler that's responsible for handling that particular
	// agent's messages.
	agentMap *concurrentAgentMap

	// computes internal stats
	statsHandler *StatsHandler
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
	quitCh     chan struct{}

	once sync.Once
	wg   sync.WaitGroup
}

// NewAgentTopicListener creates a new agent topic listener.
func NewAgentTopicListener(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore,
	sendMsgFn SendMessageFn, statsHandler *StatsHandler) (*AgentTopicListener, error) {
	clock := utils.SystemClock{}
	return NewAgentTopicListenerWithClock(agentManager, tracepointManager, mdStore, sendMsgFn, statsHandler, clock)
}

// NewAgentTopicListenerWithClock creates a new agent topic listener with a clock.
func NewAgentTopicListenerWithClock(agentManager AgentManager, tracepointManager *TracepointManager, mdStore MetadataStore,
	sendMsgFn SendMessageFn, statsHandler *StatsHandler, clock utils.Clock) (*AgentTopicListener, error) {
	atl := &AgentTopicListener{
		clock:             clock,
		agentManager:      agentManager,
		tracepointManager: tracepointManager,
		sendMessage:       sendMsgFn,
		mdStore:           mdStore,
		agentMap:          &concurrentAgentMap{unsafeMap: make(map[uuid.UUID]*AgentHandler)},
		statsHandler:      statsHandler,
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
	if ah := a.agentMap.read(agentID); ah != nil {
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
		quitCh:            make(chan struct{}),
	}
	a.agentMap.write(agentID, newAgentHandler)
	go newAgentHandler.processMessages()

	return newAgentHandler
}

func (a *AgentTopicListener) forwardAgentHeartBeat(m *messages.Heartbeat, msg *nats.Msg) {
	if a.statsHandler != nil {
		a.statsHandler.HandleAgentHeartbeat(m)
	}

	// Check if this is a known agent and forward it to that agentHandler if it exists.
	// Otherwise, send back a NACK because the agent doesn't exist.
	agentID, err := utils.UUIDFromProto(m.AgentID)
	if err != nil {
		log.WithError(err).Error("Could not parse UUID from proto.")
		return
	}

	agentHandler := a.agentMap.read(agentID)
	if agentHandler != nil {
		// Add to agent handler to process.
		agentHandler.MsgChannel <- msg
	} else {
		log.WithField("agentID", agentID.String()).Info("Received heartbeat for agent whose agenthandler doesn't exist. Sending NACK.")
		// Agent does not exist in handler map. Send NACK asking agent to reregister.
		resp := messages.VizierMessage{
			Msg: &messages.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messages.HeartbeatNack{
					Reregister: true,
				},
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

	agentHandler := a.agentMap.read(agentID)
	if agentHandler == nil {
		agentHandler = a.createAgentHandler(agentID)
	}
	// Add to agent handler to process.
	agentHandler.MsgChannel <- msg
}

// StopAgent should be called when an agent should be deleted and its message processing should be stopped.
func (a *AgentTopicListener) StopAgent(agentID uuid.UUID) {
	agentHandler := a.agentMap.read(agentID)
	if agentHandler != nil {
		agentHandler.Stop()
	}
}

// DeleteAgent deletes the agent from the map. The agent should already be deleted in the agent manager.
func (a *AgentTopicListener) deleteAgent(agentID uuid.UUID) {
	// Sends a NACK to the agent with reregister set to false.
	// This handles the scenario where the agent missed a heartbeat
	// timeout window, and so we assume the agent is likely in
	// a bad state and probably needs to be recreated. This NACK
	// will get the agent to kill itself.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatNack{
			HeartbeatNack: &messages.HeartbeatNack{
				Reregister: false,
			},
		},
	}
	a.SendMessageToAgent(agentID, resp)

	a.agentMap.delete(agentID)
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

// Stop stops processing any agent messages.
func (a *AgentTopicListener) Stop() {
	// Grab all the handlers in one go since calling stop will modify the map and need
	// the write mutex to be held.
	handlers := a.agentMap.values()
	for _, ah := range handlers {
		ah.Stop()
	}
}

// processMessages handles all of the agent messages for this agent. If it does not receive a heartbeat from the agent
// after a certain amount of time, it will declare the agent dead and perform deletion.
func (ah *AgentHandler) processMessages() {
	ah.wg.Add(1)

	defer ah.stop()
	timer := time.NewTimer(AgentExpirationTimeout)
	for {
		select {
		case <-ah.quitCh: // Prioritize the quitChannel.
			log.WithField("agentID", ah.id.String()).Info("Quit called on agent handler, deleting agent")
			return
		default:
		}

		select {
		case <-ah.quitCh:
			log.WithField("agentID", ah.id.String()).Info("Quit called on agent handler, deleting agent")
			return
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

	// Delete agent with same hostname, if any.
	hostname := ""
	if !m.Info.Capabilities.CollectsData {
		hostname = m.Info.HostInfo.Hostname
	}
	hostnameAgID, err := ah.mdStore.GetAgentIDForHostnamePair(&HostnameIPPair{hostname, m.Info.HostInfo.HostIP})
	if err != nil {
		log.WithError(err).Error("Failed to get agent hostname")
	}
	if hostnameAgID != "" {
		delAgID, err := uuid.FromString(hostnameAgID)
		if err != nil {
			log.WithError(err).Error("Could not parse agent ID")
		} else {
			ah.atl.StopAgent(delAgID)
		}
	}

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
				HeartbeatNack: &messages.HeartbeatNack{
					Reregister: true,
				},
			},
		}
		ah.atl.SendMessageToAgent(agentID, resp)
		return
	}

	// Create heartbeat ACK message.
	resp := messages.VizierMessage{
		Msg: &messages.VizierMessage_HeartbeatAck{
			HeartbeatAck: &messages.HeartbeatAck{
				Time: ah.clock.Now().UnixNano(),
				UpdateInfo: &messages.MetadataUpdateInfo{
					ServiceCIDR: ah.agentManager.GetServiceCIDR(),
					PodCIDRs:    ah.agentManager.GetPodCIDRs(),
				},
				SequenceNumber: m.SequenceNumber,
			},
		},
	}

	err = ah.atl.SendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Could not send heartbeat ack to agent.")
	}

	// Apply agent's container/schema updates.
	if m.UpdateInfo != nil {
		ah.agentManager.ApplyAgentUpdate(&AgentUpdate{AgentID: agentID, UpdateInfo: m.UpdateInfo})
	}
}

func (ah *AgentHandler) stop() {
	defer ah.wg.Done()

	ah.agentManager.DeleteAgent(ah.id)
	ah.atl.deleteAgent(ah.id)
	ah.tracepointManager.DeleteAgent(ah.id)
	close(ah.MsgChannel)
}

// Stop immediately stops the agent handler from listening to any messages. It blocks until
// the agent is cleaned up.
func (ah *AgentHandler) Stop() {
	ah.once.Do(func() {
		close(ah.quitCh)
	})

	ah.wg.Wait()
}
