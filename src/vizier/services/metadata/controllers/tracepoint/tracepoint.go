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
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/carnot/planner/dynamic_tracing/ir/logicalpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/utils"
	messages "px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
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
	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RemoveTracepointRequest{
					RemoveTracepointRequest: &messages.RemoveTracepointRequest{
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

// RegisterTracepoint sends requests to the given agents to register the specified tracepoint.
func (m *Manager) RegisterTracepoint(agentIDs []uuid.UUID, tracepointID uuid.UUID, tracepointDeployment *logicalpb.TracepointDeployment) error {
	tracepointReq := messages.VizierMessage{
		Msg: &messages.VizierMessage_TracepointMessage{
			TracepointMessage: &messages.TracepointMessage{
				Msg: &messages.TracepointMessage_RegisterTracepointRequest{
					RegisterTracepointRequest: &messages.RegisterTracepointRequest{
						TracepointDeployment: tracepointDeployment,
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

	return m.agtMgr.MessageAgents(agentIDs, msg)
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
