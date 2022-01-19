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

package tracker

import (
	"fmt"
	"sync"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

// KelvinSSLTargetOverride the hostname used for SSL target override when sending data to Kelvin.
// Note: This value may differ in the future. When that happens, this should instead come from the
// Kelvins themselves. rather than this variable, when Kelvins send their updated state.
const KelvinSSLTargetOverride = "kelvin.%s.svc"

// AgentsInfo tracks information about the distributed state of the system.
type AgentsInfo interface {
	ClearPendingState()
	UpdateAgentsInfo(update *metadatapb.AgentUpdatesResponse) error
	DistributedState() distributedpb.DistributedState
}

// AgentsInfoImpl implements AgentsInfo to track information about the distributed state of the system.
type AgentsInfoImpl struct {
	ds distributedpb.DistributedState
	// Controls access to ds.
	dsMutex sync.Mutex

	pendingDs *distributedpb.DistributedState
}

// NewAgentsInfo creates an empty agents info.
func NewAgentsInfo() AgentsInfo {
	return &AgentsInfoImpl{
		ds: distributedpb.DistributedState{},
		pendingDs: &distributedpb.DistributedState{
			SchemaInfo: []*distributedpb.SchemaInfo{},
			CarnotInfo: []*distributedpb.CarnotInfo{},
		},
	}
}

// NewTestAgentsInfo creates an agents info from a passed in distributed state.
func NewTestAgentsInfo(ds *distributedpb.DistributedState) AgentsInfo {
	return &AgentsInfoImpl{
		ds:        *(ds),
		pendingDs: nil,
	}
}

// ClearPendingState clears the pending agents info state (the upcoming version).
func (a *AgentsInfoImpl) ClearPendingState() {
	log.Infof("Clearing distributed state")
	a.pendingDs = &distributedpb.DistributedState{
		SchemaInfo: []*distributedpb.SchemaInfo{},
		CarnotInfo: []*distributedpb.CarnotInfo{},
	}
}

// UpdateAgentsInfo creates a new agent info.
// This function must be resilient to receiving the same update twice for a given agent.
func (a *AgentsInfoImpl) UpdateAgentsInfo(update *metadatapb.AgentUpdatesResponse) error {
	if update.AgentSchemasUpdated {
		log.Infof("Updating schemas to %d tables", len(update.AgentSchemas))
		a.pendingDs.SchemaInfo = update.AgentSchemas
	}

	carnotInfoMap := make(map[uuid.UUID]*distributedpb.CarnotInfo)
	for _, carnotInfo := range a.pendingDs.CarnotInfo {
		agentUUID, err := utils.UUIDFromProto(carnotInfo.AgentID)
		if err != nil {
			return err
		}
		carnotInfoMap[agentUUID] = carnotInfo
	}

	log.Tracef("%d agents present in tracker before update", len(carnotInfoMap))

	createdAgents := 0
	deletedAgents := 0
	updatedAgents := 0
	updatedAgentsDataInfo := 0

	for _, agentUpdate := range update.AgentUpdates {
		agentUUID, err := utils.UUIDFromProto(agentUpdate.AgentID)
		if err != nil {
			return err
		}
		// case 1: agent info update
		agent := agentUpdate.GetAgent()
		if agent != nil {
			if _, present := carnotInfoMap[agentUUID]; present {
				updatedAgents++
			} else {
				createdAgents++
			}

			if agent.Info.Capabilities == nil || agent.Info.Capabilities.CollectsData {
				var metadataInfo *distributedpb.MetadataInfo
				if carnotInfo, present := carnotInfoMap[agentUUID]; present {
					metadataInfo = carnotInfo.MetadataInfo
				}
				// this is a PEM
				carnotInfoMap[agentUUID] = makeAgentCarnotInfo(agentUUID, agent.ASID, metadataInfo)
			} else {
				// this is a Kelvin
				kelvinGRPCAddress := agent.Info.IPAddress
				carnotInfoMap[agentUUID] = makeKelvinCarnotInfo(agentUUID, kelvinGRPCAddress, agent.ASID)
			}
		}
		// case 2: agent data info update
		dataInfo := agentUpdate.GetDataInfo()
		if dataInfo != nil {
			updatedAgentsDataInfo++
			carnotInfo, present := carnotInfoMap[agentUUID]
			if !present {
				// It's possible that an agent may be deleted, but we still receive the schema. We should be robust to this case.
				continue
			}
			if carnotInfo == nil {
				return fmt.Errorf("Carnot info is nil for agent %s, but received agent data info", agentUUID.String())
			}
			if dataInfo.MetadataInfo != nil {
				carnotInfo.MetadataInfo = dataInfo.MetadataInfo
			}
		}
		// case 3: agent deleted
		if agentUpdate.GetDeleted() {
			deletedAgents++
			delete(carnotInfoMap, agentUUID)
		}
	}

	log.Tracef("Created %d agents, deleted %d agents, updated %d agents, updated %d agents data info",
		createdAgents, deletedAgents, updatedAgents, updatedAgentsDataInfo)
	log.Tracef("%d agents present in tracker after update", len(carnotInfoMap))

	// reset the array and recreate.
	a.pendingDs.CarnotInfo = []*distributedpb.CarnotInfo{}
	for _, carnotInfo := range carnotInfoMap {
		a.pendingDs.CarnotInfo = append(a.pendingDs.CarnotInfo, carnotInfo)
	}

	// If we have reached the end of version, promote the pending DistributedState to the current external-facing
	// distributed state accessible by clients of `Agents`.
	if update.EndOfVersion {
		a.dsMutex.Lock()
		a.ds = *(a.pendingDs)
		a.dsMutex.Unlock()
	}

	return nil
}

// DistributedState returns the current distributed state.
// Returns a non-pointer because a.ds will change over time and we want the consumer of DistributedState()
// to have a consistent result.
func (a *AgentsInfoImpl) DistributedState() distributedpb.DistributedState {
	a.dsMutex.Lock()
	defer a.dsMutex.Unlock()
	return a.ds
}

func makeAgentCarnotInfo(agentID uuid.UUID, asid uint32, agentMetadata *distributedpb.MetadataInfo) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		AgentID:              utils.ProtoFromUUID(agentID),
		ASID:                 asid,
		HasGRPCServer:        false,
		HasDataStore:         true,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		MetadataInfo:         agentMetadata,
	}
}

func makeKelvinCarnotInfo(agentID uuid.UUID, grpcAddress string, asid uint32) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		AgentID:              utils.ProtoFromUUID(agentID),
		ASID:                 asid,
		HasGRPCServer:        true,
		GRPCAddress:          grpcAddress,
		HasDataStore:         false,
		ProcessesData:        true,
		AcceptsRemoteSources: true,
		// When we support persistent storage, Kelvins will also have MetadataInfo.
		MetadataInfo:  nil,
		SSLTargetName: fmt.Sprintf(KelvinSSLTargetOverride, viper.GetString("pod_namespace")),
	}
}
