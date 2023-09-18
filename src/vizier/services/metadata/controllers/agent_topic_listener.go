/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

import (
	"reflect"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/controllers/agent"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
	"px.dev/pixie/src/vizier/utils/messagebus"
)

const (
	agentExpirationTimeout = 1 * time.Minute
)

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
	agtMgr      agent.Manager
	tpMgr       *tracepoint.Manager
	sendMessage SendMessageFn

	// Map from agent ID -> the agentHandler that's responsible for handling that particular
	// agent's messagespb.
	agentMap *concurrentAgentMap
}

// AgentHandler is responsible for handling messages for a specific agent.
type AgentHandler struct {
	id     uuid.UUID
	agtMgr agent.Manager
	tpMgr  *tracepoint.Manager
	atl    *AgentTopicListener

	MsgChannel chan *nats.Msg
	quitCh     chan struct{}

	once sync.Once
	wg   sync.WaitGroup
}

// NewAgentTopicListener creates a new agent topic listener.
func NewAgentTopicListener(agtMgr agent.Manager, tpMgr *tracepoint.Manager,
	sendMsgFn SendMessageFn) (*AgentTopicListener, error) {
	atl := &AgentTopicListener{
		agtMgr:      agtMgr,
		tpMgr:       tpMgr,
		sendMessage: sendMsgFn,
		agentMap:    &concurrentAgentMap{unsafeMap: make(map[uuid.UUID]*AgentHandler)},
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
	agents, err := a.agtMgr.GetActiveAgents()
	if err != nil {
		return err
	}

	for _, agt := range agents {
		agentID, err := utils.UUIDFromProto(agt.Info.AgentID)
		if err != nil {
			return err
		}

		a.createAgentHandler(agentID)
	}

	return nil
}

// HandleMessage handles a message on the agent topic.
func (a *AgentTopicListener) HandleMessage(msg *nats.Msg) error {
	pb := &messagespb.VizierMessage{}
	err := proto.Unmarshal(msg.Data, pb)
	if err != nil {
		log.WithError(err).Error("Failed to unmarshal vizier message")
		return nil
	}

	if pb.Msg == nil {
		log.
			Error("Received empty VizierMessage.")
		return nil
	}

	switch m := pb.Msg.(type) {
	case *messagespb.VizierMessage_Heartbeat:
		a.forwardAgentHeartBeat(m.Heartbeat, msg)
	case *messagespb.VizierMessage_RegisterAgentRequest:
		a.forwardAgentRegisterRequest(m.RegisterAgentRequest, msg)
	case *messagespb.VizierMessage_TracepointMessage:
		a.onAgentTracepointMessage(m.TracepointMessage)
	default:
		log.WithField("message-type", reflect.TypeOf(pb.Msg).String()).
			Error("Unhandled message.")
	}
	return nil
}

// SendMessageToAgent sends the given message to the agent over the NATS agent channel.
func (a *AgentTopicListener) SendMessageToAgent(agentID uuid.UUID, msg messagespb.VizierMessage) error {
	topic := messagebus.AgentUUIDTopic(agentID)
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
		id:         agentID,
		agtMgr:     a.agtMgr,
		tpMgr:      a.tpMgr,
		atl:        a,
		MsgChannel: make(chan *nats.Msg, 10),
		quitCh:     make(chan struct{}),
	}
	a.agentMap.write(agentID, newAgentHandler)
	go newAgentHandler.processMessages()

	return newAgentHandler
}

func (a *AgentTopicListener) forwardAgentHeartBeat(m *messagespb.Heartbeat, msg *nats.Msg) {
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
		resp := messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messagespb.HeartbeatNack{
					Reregister: true,
				},
			},
		}
		err = a.SendMessageToAgent(agentID, resp)
		if err != nil {
			log.WithError(err).Error("Failed to send message to agent")
		}
	}
}

func (a *AgentTopicListener) forwardAgentRegisterRequest(m *messagespb.RegisterAgentRequest, msg *nats.Msg) {
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
	resp := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_HeartbeatNack{
			HeartbeatNack: &messagespb.HeartbeatNack{
				Reregister: false,
			},
		},
	}
	err := a.SendMessageToAgent(agentID, resp)
	if err != nil {
		log.WithError(err).Error("Failed to send message to agent")
	}
	a.agentMap.delete(agentID)
}

func (a *AgentTopicListener) onAgentTracepointMessage(pbMessage *messagespb.TracepointMessage) {
	switch m := pbMessage.Msg.(type) {
	case *messagespb.TracepointMessage_TracepointInfoUpdate:
		a.onAgentTracepointInfoUpdate(m.TracepointInfoUpdate)
	default:
		log.WithField("message-type", reflect.TypeOf(pbMessage.Msg).String()).
			Error("Unhandled message.")
	}
}

func (a *AgentTopicListener) onAgentTracepointInfoUpdate(m *messagespb.TracepointInfoUpdate) {
	err := a.tpMgr.UpdateAgentTracepointStatus(m.ID, m.AgentID, m.State, m.Status)
	if err != nil {
		log.WithError(err).Error("Could not update agent tracepoint status")
	}
}

// Stop stops processing any agent messagespb.
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

	defer func() {
		err := ah.agtMgr.DeleteAgent(ah.id)
		if err != nil {
			log.WithError(err).Error("Failed to delete agent from agent manager")
		}
		ah.atl.deleteAgent(ah.id)
		err = ah.tpMgr.DeleteAgent(ah.id)
		if err != nil {
			log.WithError(err).Error("Failed to delete agent from tracepoint manager")
		}
		ah.wg.Done()
	}()

	timer := time.NewTimer(agentExpirationTimeout)
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
			pb := &messagespb.VizierMessage{}
			err := proto.Unmarshal(msg.Data, pb)
			if err != nil {
				continue
			}

			switch m := pb.Msg.(type) {
			case *messagespb.VizierMessage_Heartbeat:
				ah.onAgentHeartbeat(m.Heartbeat)
			case *messagespb.VizierMessage_RegisterAgentRequest:
				ah.onAgentRegisterRequest(m.RegisterAgentRequest)
			default:
				log.WithField("message-type", reflect.TypeOf(pb.Msg).String()).
					Error("Unhandled message.")
			}

			if !timer.Stop() {
				<-timer.C
			}
			timer.Reset(agentExpirationTimeout)
		case <-timer.C:
			log.WithField("agentID", ah.id.String()).Info("AgentHandler timed out, deleting agent")
			return
		}
	}
}

func (ah *AgentHandler) onAgentRegisterRequest(m *messagespb.RegisterAgentRequest) {
	// Create RegisterAgentResponse.
	agentID := ah.id
	log.WithField("agent", agentID.String()).Infof("Received AgentRegisterRequest for agent")

	// Delete agent with same hostname, if any.
	hostname := ""
	if !m.Info.Capabilities.CollectsData {
		hostname = m.Info.HostInfo.Hostname
	}
	hostnameAgID, err := ah.agtMgr.GetAgentIDForHostnamePair(&agent.HostnameIPPair{
		Hostname: hostname,
		IP:       m.Info.HostInfo.HostIP,
	})
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
		LastHeartbeatNS: time.Now().UnixNano(),
		CreateTimeNS:    time.Now().UnixNano(),
		// This will be set if this is an agent trying to reregister.
		ASID: m.ASID,
	}
	agentInfo.Info.AgentID = utils.ProtoFromUUID(agentID)

	asid, err := ah.agtMgr.RegisterAgent(agentInfo)
	if err != nil {
		log.WithError(err).Error("Could not create agent.")
		return
	}
	resp := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_RegisterAgentResponse{
			RegisterAgentResponse: &messagespb.RegisterAgentResponse{
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
		tracepoints, err := ah.tpMgr.GetAllTracepoints()
		if err != nil {
			log.WithError(err).Error("Could not get all tracepoints")
			return
		}

		agent := []*agentpb.Agent{agentInfo}
		for _, tp := range tracepoints {
			if tp.ExpectedState != statuspb.TERMINATED_STATE {
				err = ah.tpMgr.RegisterTracepoint(agent, utils.UUIDFromProtoOrNil(tp.ID), tp.Tracepoint)
				if err != nil {
					log.WithError(err).Error("Failed to send RegisterTracepoint request")
				}
			}
		}
	}()
}

func (ah *AgentHandler) onAgentHeartbeat(m *messagespb.Heartbeat) {
	agentID := ah.id

	// Update agent's heartbeat in agent manager.
	err := ah.agtMgr.UpdateHeartbeat(agentID)
	if err != nil {
		log.WithError(err).Error("Could not update agent heartbeat.")
		resp := messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_HeartbeatNack{
				HeartbeatNack: &messagespb.HeartbeatNack{
					Reregister: true,
				},
			},
		}
		err = ah.atl.SendMessageToAgent(agentID, resp)
		if err != nil {
			log.WithError(err).Error("Failed to send message to agent")
		}
		return
	}

	// Create heartbeat ACK message.
	resp := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_HeartbeatAck{
			HeartbeatAck: &messagespb.HeartbeatAck{
				Time: time.Now().UnixNano(),
				UpdateInfo: &messagespb.MetadataUpdateInfo{
					ServiceCIDR: ah.agtMgr.GetServiceCIDR(),
					PodCIDRs:    ah.agtMgr.GetPodCIDRs(),
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
		err = ah.agtMgr.ApplyAgentUpdate(&agent.Update{AgentID: agentID, UpdateInfo: m.UpdateInfo})
		if err != nil {
			log.WithError(err).Error("Could not apply agent updates")
		}
	}
}

// Stop immediately stops the agent handler from listening to any messages. It blocks until
// the agent is cleaned up.
func (ah *AgentHandler) Stop() {
	ah.once.Do(func() {
		close(ah.quitCh)
	})

	ah.wg.Wait()
}
