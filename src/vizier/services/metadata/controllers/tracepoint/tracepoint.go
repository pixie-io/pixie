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

package tracepoint

import (
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/carnot/planner/dynamic_tracing/ir/logicalpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
)

var (
	// ErrTracepointAlreadyExists is produced if a tracepoint already exists with the given name
	// and does not have a matching schema.
	ErrTracepointAlreadyExists = errors.New("TracepointDeployment already exists")
)

// agentMessenger is a controller that lets us message all agents and all active agents.
type agentMessenger interface {
	MessageAgents(agentIDs []uuid.UUID, msg []byte) error
	MessageActiveAgents(msg []byte) error
}

// Store is a datastore which can store, update, and retrieve information about tracepoints.
type Store interface {
	UpsertTracepoint(uuid.UUID, *storepb.TracepointInfo) error
	GetTracepoint(uuid.UUID) (*storepb.TracepointInfo, error)
	GetTracepoints() ([]*storepb.TracepointInfo, error)
	UpdateTracepointState(*storepb.AgentTracepointStatus) error
	GetTracepointStates(uuid.UUID) ([]*storepb.AgentTracepointStatus, error)
	SetTracepointWithName(string, uuid.UUID) error
	GetTracepointsWithNames([]string) ([]*uuid.UUID, error)
	GetTracepointsForIDs([]uuid.UUID) ([]*storepb.TracepointInfo, error)
	SetTracepointTTL(uuid.UUID, time.Duration) error
	DeleteTracepointTTLs([]uuid.UUID) error
	DeleteTracepoint(uuid.UUID) error
	DeleteTracepointsForAgent(uuid.UUID) error
	GetTracepointTTLs() ([]uuid.UUID, []time.Time, error)
}

// Manager manages the tracepoints deployed in the cluster.
type Manager struct {
	ts     Store
	agtMgr agentMessenger

	done chan struct{}
	once sync.Once
}

// NewManager creates a new tracepoint manager.
func NewManager(ts Store, agtMgr agentMessenger, ttlReaperDuration time.Duration) *Manager {
	tm := &Manager{
		ts:     ts,
		agtMgr: agtMgr,
		done:   make(chan struct{}),
	}

	go tm.watchForTracepointExpiry(ttlReaperDuration)
	return tm
}

func (m *Manager) watchForTracepointExpiry(ttlReaperDuration time.Duration) {
	ticker := time.NewTicker(ttlReaperDuration)
	defer ticker.Stop()
	for {
		select {
		case <-m.done:
			return
		case <-ticker.C:
			m.terminateExpiredTracepoints()
		}
	}
}

func (m *Manager) terminateExpiredTracepoints() {
	tps, err := m.ts.GetTracepoints()
	if err != nil {
		log.WithError(err).Warn("error encountered when trying to terminating expired tracepoints")
		return
	}

	ttlKeys, ttlVals, err := m.ts.GetTracepointTTLs()
	if err != nil {
		log.WithError(err).Warn("error encountered when trying to terminating expired tracepoints")
		return
	}

	now := time.Now()

	// Lookup for tracepoints that still have an active ttl
	tpActive := make(map[uuid.UUID]bool)
	for i, tp := range ttlKeys {
		tpActive[tp] = ttlVals[i].After(now)
	}

	for _, tp := range tps {
		tpID := utils.UUIDFromProtoOrNil(tp.ID)
		if tpActive[tpID] {
			// Tracepoint TTL exists and is in the future
			continue
		}
		if tp.ExpectedState == statuspb.TERMINATED_STATE {
			// Tracepoint is already in terminated state
			continue
		}
		err = m.terminateTracepoint(tpID)
		if err != nil {
			log.WithError(err).Warn("error encountered when trying to terminating expired tracepoints")
		}
	}
}

func (m *Manager) terminateTracepoint(id uuid.UUID) error {
	// Update state in datastore to terminated.
	tp, err := m.ts.GetTracepoint(id)
	if err != nil {
		return err
	}

	if tp == nil {
		return nil
	}

	tp.ExpectedState = statuspb.TERMINATED_STATE
	err = m.ts.UpsertTracepoint(id, tp)
	if err != nil {
		return err
	}

	// Send termination messages to PEMs.
	tracepointReq := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_TracepointMessage{
			TracepointMessage: &messagespb.TracepointMessage{
				Msg: &messagespb.TracepointMessage_RemoveTracepointRequest{
					RemoveTracepointRequest: &messagespb.RemoveTracepointRequest{
						ID: utils.ProtoFromUUID(id),
					},
				},
			},
		},
	}
	msg, err := tracepointReq.Marshal()
	if err != nil {
		return err
	}

	return m.agtMgr.MessageActiveAgents(msg)
}

func (m *Manager) deleteTracepoint(id uuid.UUID) error {
	return m.ts.DeleteTracepoint(id)
}

// CreateTracepoint creates and stores info about the given tracepoint.
func (m *Manager) CreateTracepoint(tracepointName string, tracepointDeployment *logicalpb.TracepointDeployment, ttl time.Duration) (*uuid.UUID, error) {
	// Check to see if a tracepoint with the matching name already exists.
	resp, err := m.ts.GetTracepointsWithNames([]string{tracepointName})
	if err != nil {
		return nil, err
	}

	if len(resp) != 1 {
		return nil, errors.New("Could not fetch tracepoint")
	}
	prevTracepointID := resp[0]

	if prevTracepointID != nil { // Existing tracepoint already exists.
		prevTracepoint, err := m.ts.GetTracepoint(*prevTracepointID)
		if err != nil {
			return nil, err
		}
		if prevTracepoint != nil && prevTracepoint.ExpectedState != statuspb.TERMINATED_STATE {
			// If everything is exactly the same, no need to redeploy
			//   - return prevTracepointID, ErrTracepointAlreadyExists
			// If anything inside tracepoints has changed
			//   - delete old tracepoints, and insert new tracepoints.

			// Check if the tracepoints are exactly the same.
			allTpsSame := true

			if len(prevTracepoint.Tracepoint.Programs) == len(tracepointDeployment.Programs) {
				for i := range prevTracepoint.Tracepoint.Programs {
					if tracepointDeployment.Programs[i] != nil {
						if !proto.Equal(tracepointDeployment.Programs[i], prevTracepoint.Tracepoint.Programs[i]) {
							allTpsSame = false
							break
						}
					}
				}
			} else {
				allTpsSame = false
			}

			if allTpsSame {
				err = m.ts.SetTracepointTTL(*prevTracepointID, ttl)
				if err != nil {
					return nil, err
				}
				return prevTracepointID, ErrTracepointAlreadyExists
			}

			// Something has changed, so trigger termination of the old tracepoint.
			err = m.ts.DeleteTracepointTTLs([]uuid.UUID{*prevTracepointID})
			if err != nil {
				return nil, err
			}
		}
	}

	tpID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}
	newTracepoint := &storepb.TracepointInfo{
		ID:            utils.ProtoFromUUID(tpID),
		Tracepoint:    tracepointDeployment,
		Name:          tracepointName,
		ExpectedState: statuspb.RUNNING_STATE,
	}
	err = m.ts.UpsertTracepoint(tpID, newTracepoint)
	if err != nil {
		return nil, err
	}
	err = m.ts.SetTracepointTTL(tpID, ttl)
	if err != nil {
		return nil, err
	}
	err = m.ts.SetTracepointWithName(tracepointName, tpID)
	if err != nil {
		return nil, err
	}
	return &tpID, nil
}

// GetAllTracepoints gets all the tracepoints currently tracked by the metadata service.
func (m *Manager) GetAllTracepoints() ([]*storepb.TracepointInfo, error) {
	return m.ts.GetTracepoints()
}

// UpdateAgentTracepointStatus updates the tracepoint info with the new agent tracepoint status.
func (m *Manager) UpdateAgentTracepointStatus(tracepointID *uuidpb.UUID, agentID *uuidpb.UUID, state statuspb.LifeCycleState, status *statuspb.Status) error {
	if state == statuspb.TERMINATED_STATE { // If all agent tracepoint statuses are now terminated, we can finally delete the tracepoint from the datastore.
		tID := utils.UUIDFromProtoOrNil(tracepointID)
		states, err := m.GetTracepointStates(tID)
		if err != nil {
			return err
		}
		allTerminated := true
		for _, s := range states {
			if s.State != statuspb.TERMINATED_STATE && !s.AgentID.Equal(agentID) {
				allTerminated = false
				break
			}
		}

		if allTerminated {
			return m.deleteTracepoint(tID)
		}
	}

	tracepointState := &storepb.AgentTracepointStatus{
		State:   state,
		Status:  status,
		ID:      tracepointID,
		AgentID: agentID,
	}

	return m.ts.UpdateTracepointState(tracepointState)
}

func (m *Manager) FilterAgentsBySelector(agents []*agentpb.Agent, selector *logicalpb.TracepointSelector) []*agentpb.Agent {
	// Each selector successively filters the list of agents.
	var filteredAgents []*agentpb.Agent
	switch selector.SelectorType {
	case logicalpb.MIN_KERNEL:
		filteredAgents = m.filterByMinKernel(agents, selector.Value)
	case logicalpb.MAX_KERNEL:
		filteredAgents = m.filterByMaxKernel(agents, selector.Value)
	case logicalpb.HOST_NAME:
		filteredAgents = m.filterByHostName(agents, selector.Value)
	// Other selector types can be added here in the future.
	default:
		// If NO_CONDITION or unknown condition, return all agents
		filteredAgents = agents
	}
	return filteredAgents
}

func (m *Manager) filterByHostName(agents []*agentpb.Agent, hostname string) []*agentpb.Agent {
	filteredAgents := make([]*agentpb.Agent, 0)
	for _, agent := range agents {
		if agent.Info.HostInfo.Hostname == hostname {
			filteredAgents = append(filteredAgents, agent)
		}
	}
	return filteredAgents
}

func parseKernelVersion(versionStr string) (uint32, uint32, uint32, error) {
	var version, major, minor uint32
	parts := strings.Split(versionStr, ".")
	var err error
	switch len(parts) {
	case 1:
		_, err = fmt.Sscanf(versionStr, "%d", &version)
	case 2:
		_, err = fmt.Sscanf(versionStr, "%d.%d", &version, &major)
	case 3:
		_, err = fmt.Sscanf(versionStr, "%d.%d.%d", &version, &major, &minor)
	default:
		err = fmt.Errorf("Invalid kernel version string: %s", versionStr)
	}
	return version, major, minor, err
}

func (m *Manager) filterByMinKernel(agents []*agentpb.Agent, versionStr string) []*agentpb.Agent {
	version, major, minor, err := parseKernelVersion(versionStr)
	if err != nil {
		log.Warnf("Error parsing version string: %s, Error: %v", versionStr, err)
		return nil
	}

	filteredAgents := make([]*agentpb.Agent, 0)
	for _, agent := range agents {
		kv := agent.Info.HostInfo.Kernel
		if kv == nil {
			continue
		}
		if kv.Version > version || (kv.Version == version && kv.MajorRev > major) || (kv.Version == version && kv.MajorRev == major && kv.MinorRev >= minor) {
			filteredAgents = append(filteredAgents, agent)
		}
	}
	return filteredAgents
}

func (m *Manager) filterByMaxKernel(agents []*agentpb.Agent, versionStr string) []*agentpb.Agent {
	version, major, minor, err := parseKernelVersion(versionStr)
	if err != nil {
		log.Warnf("Error parsing version string: %s, Error: %v", versionStr, err)
		return nil
	}

	filteredAgents := make([]*agentpb.Agent, 0)
	for _, agent := range agents {
		kv := agent.Info.HostInfo.Kernel
		if kv == nil {
			continue
		}
		if kv.Version < version || (kv.Version == version && kv.MajorRev < major) || (kv.Version == version && kv.MajorRev == major && kv.MinorRev <= minor) {
			filteredAgents = append(filteredAgents, agent)
		}
	}
	return filteredAgents
}

// RegisterTracepoint sends requests to the given agents to register the specified tracepoint.
// For each tracepoint program in this deployment, we look at the selectors and pick a list of agents
// that match those selectors. For that list of agents, we send out a tracepoint request with
// a new tracepointDeployment. If multiple programs may have the same list of allowed agents,
// we collapse them into one tracepoint deployment and send those in one request.
// Note: stirling current only supports one tracepoint per tracepoint deployment.
func (m *Manager) RegisterTracepoint(agents []*agentpb.Agent, tracepointID uuid.UUID, tracepointDeployment *logicalpb.TracepointDeployment) error {
	// Map where key is the hash of agent IDs and value is the list of programs for those agents.
	agentsHashToPrograms := make(map[string][]*logicalpb.TracepointDeployment_TracepointProgram)
	// Map where key is the hash of agent IDs and value is a struct containing the list of agents and their IDs.
	agentsHashToAgents := make(map[string]struct {
		Agents   []*agentpb.Agent
		AgentIDs []uuid.UUID
	})

	for _, prgm := range tracepointDeployment.Programs {
		validAgents := agents // Start with all agents as potential targets.

		for _, selector := range prgm.Selectors {
			validAgents = m.FilterAgentsBySelector(validAgents, selector)
		}

		// Generate a hash for the list of valid agents.
		agentIDs := make([]uuid.UUID, len(validAgents))
		for i, agt := range validAgents {
			agentIDs[i] = utils.UUIDFromProtoOrNil(agt.Info.AgentID)
		}
		hash := utils.HashUUIDs(agentIDs)

		agentsHashToPrograms[hash] = append(agentsHashToPrograms[hash], prgm)
		agentsHashToAgents[hash] = struct {
			Agents   []*agentpb.Agent
			AgentIDs []uuid.UUID
		}{Agents: validAgents, AgentIDs: agentIDs}
	}

	for hash, validAgentsForProgram := range agentsHashToAgents {
		// Build a new TracepointDeployment with the group of programs.
		newDeployment := &logicalpb.TracepointDeployment{
			Name:           tracepointDeployment.Name,
			TTL:            tracepointDeployment.TTL,
			DeploymentSpec: tracepointDeployment.DeploymentSpec,
			Programs:       agentsHashToPrograms[hash],
		}

		// Send a RegisterTracepointRequest to each agent that supports this program.
		tracepointReq := messagespb.VizierMessage{
			Msg: &messagespb.VizierMessage_TracepointMessage{
				TracepointMessage: &messagespb.TracepointMessage{
					Msg: &messagespb.TracepointMessage_RegisterTracepointRequest{
						RegisterTracepointRequest: &messagespb.RegisterTracepointRequest{
							TracepointDeployment: newDeployment,
							ID:                   utils.ProtoFromUUID(tracepointID),
						},
					},
				},
			},
		}
		msg, err := tracepointReq.Marshal()
		if err != nil {
			return err
		}

		err = m.agtMgr.MessageAgents(validAgentsForProgram.AgentIDs, msg)

		if err != nil {
			return err
		}
	}

	return nil
}

// GetTracepointInfo gets the status for the tracepoint with the given ID.
func (m *Manager) GetTracepointInfo(tracepointID uuid.UUID) (*storepb.TracepointInfo, error) {
	return m.ts.GetTracepoint(tracepointID)
}

// GetTracepointStates gets all the known agent states for the given tracepoint.
func (m *Manager) GetTracepointStates(tracepointID uuid.UUID) ([]*storepb.AgentTracepointStatus, error) {
	return m.ts.GetTracepointStates(tracepointID)
}

// GetTracepointsForIDs gets all the tracepoint infos for the given ids.
func (m *Manager) GetTracepointsForIDs(ids []uuid.UUID) ([]*storepb.TracepointInfo, error) {
	return m.ts.GetTracepointsForIDs(ids)
}

// RemoveTracepoints starts the termination process for the tracepoints with the given names.
func (m *Manager) RemoveTracepoints(names []string) error {
	tpIDs, err := m.ts.GetTracepointsWithNames(names)
	if err != nil {
		return err
	}

	ids := make([]uuid.UUID, len(tpIDs))

	for i, id := range tpIDs {
		if id == nil {
			return fmt.Errorf("Could not find tracepoint for given name: %s", names[i])
		}
		ids[i] = *id
	}

	return m.ts.DeleteTracepointTTLs(ids)
}

// DeleteAgent deletes tracepoints on the given agent.
func (m *Manager) DeleteAgent(agentID uuid.UUID) error {
	return m.ts.DeleteTracepointsForAgent(agentID)
}

// Close cleans up the goroutines created and renders this no longer useable.
func (m *Manager) Close() {
	m.once.Do(func() {
		close(m.done)
	})
	m.ts = nil
	m.agtMgr = nil
}
