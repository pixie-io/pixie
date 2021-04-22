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

package agent

import (
	"errors"
	"fmt"
	"path"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/shared/k8s"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	types "px.dev/pixie/src/shared/types/gotypes"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
	"px.dev/pixie/src/vizier/utils/datastore"
)

const (
	agentKeyPrefix      = "/agent/"
	agentDataInfoPrefix = "/agentDataInfo/"
	asidKey             = "/asid"
	computedSchemaKey   = "/computedSchema"
)

// ErrNoComputedSchemas is an error indicating the lack of computedSchemas.
var ErrNoComputedSchemas = errors.New("Could not find any computed schemas")

// HostnameIPPair is a unique identifies for a K8s node.
type HostnameIPPair struct {
	Hostname string
	IP       string
}

// Datastore implements the Store interface on a given Datastore.
type Datastore struct {
	ds             datastore.MultiGetterSetterDeleterCloser
	expiryDuration time.Duration

	asidMu sync.Mutex
}

// NewDatastore wraps the datastore in a Store
func NewDatastore(ds datastore.MultiGetterSetterDeleterCloser, expiryDuration time.Duration) *Datastore {
	return &Datastore{ds: ds, expiryDuration: expiryDuration}
}

func getAgentKey(agentID uuid.UUID) string {
	return path.Join(agentKeyPrefix, agentID.String())
}

func getAgentDataInfoKey(agentID uuid.UUID) string {
	return path.Join(agentDataInfoPrefix, agentID.String())
}

func getHostnamePairAgentKey(pair *HostnameIPPair) string {
	return path.Join("/hostnameIP", fmt.Sprintf("%s-%s", pair.Hostname, pair.IP), "agent")
}

func getKelvinAgentKey(agentID uuid.UUID) string {
	return path.Join("/kelvin", agentID.String())
}

func getPodNameToAgentIDKey(podName string) string {
	return path.Join("/podToAgentID", podName)
}

func getProcessKey(upid string) string {
	return path.Join("/processes", upid)
}

// CreateAgent creates a new agent.
func (a *Datastore) CreateAgent(agentID uuid.UUID, agt *agentpb.Agent) error {
	i, err := agt.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent protobuf: " + err.Error())
	}
	hostname := ""
	if !agt.Info.Capabilities.CollectsData {
		hostname = agt.Info.HostInfo.Hostname
	}
	hnPair := &HostnameIPPair{
		Hostname: hostname,
		IP:       agt.Info.HostInfo.HostIP,
	}

	err = a.ds.Set(getHostnamePairAgentKey(hnPair), agentID.String())
	if err != nil {
		return err
	}
	err = a.ds.Set(getAgentKey(agentID), string(i))
	if err != nil {
		return err
	}
	err = a.ds.Set(getPodNameToAgentIDKey(agt.Info.HostInfo.PodName), agentID.String())
	if err != nil {
		return err
	}

	collectsData := agt.Info.Capabilities == nil || agt.Info.Capabilities.CollectsData
	if !collectsData {
		err = a.ds.Set(getKelvinAgentKey(agentID), agentID.String())
		if err != nil {
			return err
		}
	}

	log.WithField("hostname", hnPair.Hostname).WithField("HostIP", hnPair.IP).Info("Registering agent")
	return nil
}

// GetAgent gets the agent info for the agent with the given id.
func (a *Datastore) GetAgent(agentID uuid.UUID) (*agentpb.Agent, error) {
	resp, err := a.ds.Get(getAgentKey(agentID))
	if err != nil {
		return nil, err
	}
	if resp == nil {
		return nil, nil
	}
	aPb := &agentpb.Agent{}
	err = proto.Unmarshal(resp, aPb)
	if err != nil {
		return nil, err
	}
	return aPb, nil
}

// UpdateAgent updates the agent info for the agent with the given ID.
func (a *Datastore) UpdateAgent(agentID uuid.UUID, agt *agentpb.Agent) error {
	i, err := agt.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent protobuf: " + err.Error())
	}

	return a.ds.Set(getAgentKey(agentID), string(i))
}

// DeleteAgent deletes the agent with the given ID.
func (a *Datastore) DeleteAgent(agentID uuid.UUID) error {
	resp, err := a.ds.Get(getAgentKey(agentID))
	if err != nil {
		return err
	}

	// Agent does not exist, no need to delete.
	if resp == nil {
		log.Info("Tried to delete an agent that was already deleted")
		return nil
	}

	aPb := &agentpb.Agent{}
	err = proto.Unmarshal(resp, aPb)
	if err != nil {
		return err
	}

	hostname := ""
	if !aPb.Info.Capabilities.CollectsData {
		hostname = aPb.Info.HostInfo.Hostname
	}

	hnPair := &HostnameIPPair{
		Hostname: hostname,
		IP:       aPb.Info.HostInfo.HostIP,
	}
	delKeys := []string{getAgentKey(agentID), getHostnamePairAgentKey(hnPair), getPodNameToAgentIDKey(aPb.Info.HostInfo.PodName)}

	// Info.Capabiltiies should never be nil with our new PEMs/Kelvin. If it is nil,
	// this means that the protobuf we retrieved from etcd belongs to an older agent.
	collectsData := aPb.Info.Capabilities == nil || aPb.Info.Capabilities.CollectsData
	if !collectsData {
		delKeys = append(delKeys, getKelvinAgentKey(agentID))
	}

	err = a.ds.DeleteAll(delKeys)
	if err != nil {
		return err
	}

	// Deletes from the computedSchema
	err = a.UpdateSchemas(agentID, []*storepb.TableInfo{})
	if err != nil {
		return err
	}

	return a.ds.DeleteWithPrefix(getAgentDataInfoKey(agentID))
}

// GetAgents gets all of the current active agents.
func (a *Datastore) GetAgents() ([]*agentpb.Agent, error) {
	var agents []*agentpb.Agent

	keys, vals, err := a.ds.GetWithPrefix(agentKeyPrefix)
	if err != nil {
		return nil, err
	}

	for i, key := range keys {
		// Filter out keys that aren't of the form /agent/<uuid>.
		splitKey := strings.Split(string(key), "/")
		if len(splitKey) != 3 {
			continue
		}

		pb := &agentpb.Agent{}
		err = proto.Unmarshal(vals[i], pb)
		if err != nil {
			return nil, err
		}
		if pb.Info != nil && !utils.IsNilUUIDProto(pb.Info.AgentID) {
			agents = append(agents, pb)
		}
	}

	return agents, nil
}

// GetASID gets the next assignable ASID.
func (a *Datastore) GetASID() (uint32, error) {
	a.asidMu.Lock()
	defer a.asidMu.Unlock()
	asid := "1" // Starting ASID.

	resp, err := a.ds.Get(asidKey)
	if err != nil {
		return 0, err
	}
	if resp != nil {
		asid = string(resp)
	}

	// Convert ASID from etcd into uint32.
	asidInt, err := strconv.ParseUint(asid, 10, 32)
	if err != nil {
		return 0, err
	}

	// Increment ASID in datastore.
	updatedAsid := asidInt + 1
	err = a.ds.Set(asidKey, fmt.Sprint(updatedAsid))
	if err != nil {
		return 0, err
	}

	return uint32(asidInt), nil
}

// GetAgentIDFromPodName gets the agent ID for the agent with the given name.
func (a *Datastore) GetAgentIDFromPodName(podName string) (string, error) {
	id, err := a.ds.Get(getPodNameToAgentIDKey(podName))
	if err != nil {
		return "", err
	}

	if id == nil {
		return "", nil
	}

	return string(id), nil
}

// GetAgentsDataInfo returns all of the information about data tables that each agent has.
func (a *Datastore) GetAgentsDataInfo() (map[uuid.UUID]*messagespb.AgentDataInfo, error) {
	dataInfos := make(map[uuid.UUID]*messagespb.AgentDataInfo)

	keys, vals, err := a.ds.GetWithPrefix(agentDataInfoPrefix)
	if err != nil {
		return nil, err
	}

	for i, key := range keys {
		// Filter out keys that aren't of the form /agentDataInfo/<uuid>.
		splitKey := strings.Split(string(key), "/")
		if len(splitKey) != 3 {
			continue
		}
		agentID, err := uuid.FromString(splitKey[2])
		if err != nil {
			return nil, err
		}

		pb := &messagespb.AgentDataInfo{}
		err = proto.Unmarshal(vals[i], pb)
		if err != nil {
			return nil, err
		}
		dataInfos[agentID] = pb
	}
	return dataInfos, nil
}

// UpdateAgentDataInfo updates the information about data tables that a particular agent has.
func (a *Datastore) UpdateAgentDataInfo(agentID uuid.UUID, dataInfo *messagespb.AgentDataInfo) error {
	i, err := dataInfo.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent data info protobuf: " + err.Error())
	}

	return a.ds.Set(getAgentDataInfoKey(agentID), string(i))
}

// GetComputedSchema returns the raw CombinedComputedSchema.
func (a *Datastore) GetComputedSchema() (*storepb.ComputedSchema, error) {
	cSchemas, err := a.ds.Get(computedSchemaKey)
	if err != nil {
		return nil, err
	}
	if cSchemas == nil {
		return nil, ErrNoComputedSchemas
	}

	computedSchemaPb := &storepb.ComputedSchema{}
	err = proto.Unmarshal(cSchemas, computedSchemaPb)
	if err != nil {
		return nil, err
	}

	return computedSchemaPb, nil
}

func deleteTableFromComputed(computedSchemaPb *storepb.ComputedSchema, tableName string) error {
	idx := -1
	for i, schemaPb := range computedSchemaPb.Tables {
		if schemaPb.Name == tableName {
			idx = i
			break
		}
	}
	if idx < 0 {
		return fmt.Errorf("schema name not found, cannot delete")
	}

	// Set the ith element equal to last element.
	computedSchemaPb.Tables[idx] = computedSchemaPb.Tables[len(computedSchemaPb.Tables)-1]
	// Truncate last element away.
	computedSchemaPb.Tables = computedSchemaPb.Tables[:len(computedSchemaPb.Tables)-1]
	delete(computedSchemaPb.TableNameToAgentIDs, tableName)
	return nil
}

func deleteAgentFromComputed(computedSchemaPb *storepb.ComputedSchema, tableName string, agentIDPb *uuidpb.UUID) error {
	agents := computedSchemaPb.TableNameToAgentIDs[tableName]
	// If the number of agents is 1, delete the schema period.
	if len(agents.AgentID) == 1 {
		// A check to make sure the only element is the agent to delete. ideally would be a DCHECK.
		if !agents.AgentID[0].Equal(agentIDPb) {
			return fmt.Errorf("Agent %v marked for deletion not found in list of agents for %s", agentIDPb, tableName)
		}
		return deleteTableFromComputed(computedSchemaPb, tableName)
	}

	// If there are more than 1 agents then we delete the agents from the list.
	idx := -1
	for i, a := range agents.AgentID {
		if a.Equal(agentIDPb) {
			idx = i
			break
		}
	}

	if idx < 0 {
		return fmt.Errorf("Agent %v marked for deletion not found in list of agents for %s", agentIDPb, tableName)
	}

	// Set the ith element equal to last element.
	agents.AgentID[idx] = agents.AgentID[len(agents.AgentID)-1]
	// Truncate last element away.
	agents.AgentID = agents.AgentID[:len(agents.AgentID)-1]
	computedSchemaPb.TableNameToAgentIDs[tableName] = agents
	return nil
}

// UpdateSchemas updates the given schemas in the metadata store.
func (a *Datastore) UpdateSchemas(agentID uuid.UUID, schemas []*storepb.TableInfo) error {
	computedSchemaPb, err := a.GetComputedSchema()
	// If there are no computed schemas, that means we have yet to set one.
	if err == ErrNoComputedSchemas {
		// Reset error as this is not actually an error.
		err = nil
	}

	// Other errors are still errors.
	if err != nil {
		log.WithError(err).Error("Could not get old schema.")
		return err
	}

	// Make sure the computedSchema is non-nil and fields are non-nil.
	if computedSchemaPb == nil {
		computedSchemaPb = &storepb.ComputedSchema{}
	}
	if computedSchemaPb.TableNameToAgentIDs == nil {
		computedSchemaPb.TableNameToAgentIDs = make(map[string]*storepb.ComputedSchema_AgentIDs)
	}

	// Tracker for tables that were potentially deleted in the agent. We first track all the tables
	// agent previously had, then compare to the tables it currently has. Any entry that's false here
	// will have the entry deleted.
	previousAgentTableTracker := make(map[string]bool)

	agentIDPb := utils.ProtoFromUUID(agentID)

	// Build up a table map so that we can update tables by name.
	tableMap := make(map[string]*storepb.TableInfo)
	for _, existingTable := range computedSchemaPb.Tables {
		tableMap[existingTable.Name] = existingTable
	}

	// Add the list of tables that the agent currently belongs to.
	for name, agents := range computedSchemaPb.TableNameToAgentIDs {
		for _, agt := range agents.AgentID {
			if agt.Equal(agentIDPb) {
				previousAgentTableTracker[name] = false
				break
			}
		}
	}

	// Now for each schema, update the agent references to that table.
	for _, schemaPb := range schemas {
		// Update the table map to contain the new version of the table, in the event
		// that the aggent mapping is the same but the table itself has changed.
		tableMap[schemaPb.Name] = schemaPb

		_, agentHasTable := previousAgentTableTracker[schemaPb.Name]
		if agentHasTable {
			// If it's in the new schema, we don't need to do anything, so we mark this true.
			previousAgentTableTracker[schemaPb.Name] = true
			continue
		}
		// We haven't seen the agent w/ this schema. That means two cases
		// 1. The table exists in Vizier but agent is not associated.
		// 2. The table does not exist in Vizier yet, so we need to add it.

		agents, tableExists := computedSchemaPb.TableNameToAgentIDs[schemaPb.Name]

		// Case 1 tableExists, but agent needs to be associated.
		if tableExists {
			agents.AgentID = append(agents.AgentID, agentIDPb)
			continue
		}

		computedSchemaPb.TableNameToAgentIDs[schemaPb.Name] = &storepb.ComputedSchema_AgentIDs{
			AgentID: []*uuidpb.UUID{agentIDPb},
		}
	}

	// Update the tables in the schema to the new version.
	var resultTableInfos []*storepb.TableInfo
	for _, tableInfo := range tableMap {
		resultTableInfos = append(resultTableInfos, tableInfo)
	}
	computedSchemaPb.Tables = resultTableInfos

	// Now find any tables that might have been deleted from an Agent.
	// This will also delete tables entirely when there are no more agents after the last agent is deleted.
	for tableName, agentHasTable := range previousAgentTableTracker {
		if agentHasTable {
			continue
		}
		err := deleteAgentFromComputed(computedSchemaPb, tableName, agentIDPb)
		if err != nil {
			log.WithError(err).Errorf("Could not delete table to agent mapping %s -> %v", tableName, agentID)
			return err
		}
	}

	computedSchema, err := computedSchemaPb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshal computed schema update message.")
		return err
	}

	return a.ds.Set(computedSchemaKey, string(computedSchema))
}

// PruneComputedSchema cleans any dead agents from the computed schema. This is a temporary fix, to address a larger
// consistency and race-condition problem that will be addressed by the upcoming extensive refactor of the metadata service.
func (a *Datastore) PruneComputedSchema() error {
	// Fetch all existing agents.
	agents, err := a.GetAgents()
	if err != nil {
		return err
	}
	// Make a map from the agent IDs.
	existingAgents := make(map[uuid.UUID]bool)
	for _, agt := range agents {
		existingAgents[utils.UUIDFromProtoOrNil(agt.Info.AgentID)] = true
	}

	// Fetch current computed schema.
	computedSchemaPb, err := a.GetComputedSchema()
	// If there are no computed schemas, that means we don't have to do anything.
	if err == ErrNoComputedSchemas || computedSchemaPb == nil {
		return nil
	}
	if err != nil {
		return err
	}

	if computedSchemaPb.TableNameToAgentIDs == nil {
		computedSchemaPb.TableNameToAgentIDs = make(map[string]*storepb.ComputedSchema_AgentIDs)
	}

	var tableInfos []*storepb.TableInfo
	tableToAgents := make(map[string]*storepb.ComputedSchema_AgentIDs)

	existingTables := make(map[string]bool)

	// Filter out any dead agents from the table -> agent mapping.
	for tableName, agentIDs := range computedSchemaPb.TableNameToAgentIDs {
		prunedIDs := []*uuidpb.UUID{}
		for i, agentID := range agentIDs.AgentID {
			if existingAgents[utils.UUIDFromProtoOrNil(agentID)] {
				prunedIDs = append(prunedIDs, agentIDs.AgentID[i])
			}
		}
		if len(prunedIDs) > 0 {
			tableToAgents[tableName] = &storepb.ComputedSchema_AgentIDs{
				AgentID: prunedIDs,
			}
			existingTables[tableName] = true
		}
	}

	// Filter out any tables that should now be deleted.
	for i, table := range computedSchemaPb.Tables {
		if existingTables[table.Name] {
			tableInfos = append(tableInfos, computedSchemaPb.Tables[i])
		}
	}

	// Write pruned schema to the datastore.
	newComputedSchemaPb := &storepb.ComputedSchema{
		Tables:              tableInfos,
		TableNameToAgentIDs: tableToAgents,
	}
	computedSchema, err := newComputedSchemaPb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not marshal computed schema update message.")
		return err
	}

	return a.ds.Set(computedSchemaKey, string(computedSchema))
}

// GetProcesses gets the process infos for the given process upids.
func (a *Datastore) GetProcesses(upids []*types.UInt128) ([]*metadatapb.ProcessInfo, error) {
	processes := make([]*metadatapb.ProcessInfo, len(upids))

	for i, upid := range upids {
		process, err := a.ds.Get(getProcessKey(k8s.StringFromUPID(upid)))
		if err != nil {
			return nil, err
		}
		if process == nil {
			processes[i] = nil
		} else {
			processPb := metadatapb.ProcessInfo{}
			if err := proto.Unmarshal(process, &processPb); err != nil {
				log.WithError(err).Error("Could not unmarshal process pb.")
				processes[i] = nil
				continue
			}
			processes[i] = &processPb
		}
	}
	return processes, nil
}

// UpdateProcesses updates the given processes in the metadata store.
func (a *Datastore) UpdateProcesses(processes []*metadatapb.ProcessInfo) error {
	for _, processPb := range processes {
		process, err := processPb.Marshal()
		if err != nil {
			log.WithError(err).Error("Could not marshall processInfo.")
			continue
		}
		upid := types.UInt128FromProto(processPb.UPID)
		processKey := getProcessKey(k8s.StringFromUPID(upid))

		if processPb.StopTimestampNS > 0 {
			err = a.ds.SetWithTTL(processKey, string(process), a.expiryDuration)
			if err != nil {
				return err
			}
		} else {
			err = a.ds.Set(processKey, string(process))
			if err != nil {
				return err
			}
		}
	}
	return nil
}

// GetAgentIDForHostnamePair gets the agent for the given hostnamePair, if it exists.
func (a *Datastore) GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error) {
	id, err := a.ds.Get(getHostnamePairAgentKey(hnPair))
	if err != nil {
		return "", err
	}
	if id == nil {
		return "", nil
	}

	return string(id), err
}
