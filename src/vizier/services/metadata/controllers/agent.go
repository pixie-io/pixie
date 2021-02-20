package controllers

import (
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	types "pixielabs.ai/pixielabs/src/shared/types/go"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	metadata_servicepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// AgentExpirationTimeout is the amount of time that we should wait to receive a heartbeat
// from an agent before marking it as unhealthy.
const AgentExpirationTimeout time.Duration = 1 * time.Minute

// AgentStore is the interface that a persistent datastore needs to implement for tracking
// agent data.
type AgentStore interface {
	CreateAgent(agentID uuid.UUID, a *agentpb.Agent) error
	GetAgent(agentID uuid.UUID) (*agentpb.Agent, error)
	UpdateAgent(agentID uuid.UUID, a *agentpb.Agent) error
	DeleteAgent(agentID uuid.UUID) error

	GetAgents() ([]*agentpb.Agent, error)

	GetASID() (uint32, error)
	GetAgentIDFromPodName(podName string) (string, error)

	GetAgentsDataInfo() (map[uuid.UUID]*messagespb.AgentDataInfo, error)
	UpdateAgentDataInfo(agentID uuid.UUID, dataInfo *messagespb.AgentDataInfo) error

	GetComputedSchema() (*storepb.ComputedSchema, error)
	UpdateSchemas(agentID uuid.UUID, schemas []*storepb.TableInfo) error
	PruneComputedSchema() error

	GetProcesses(upids []*types.UInt128) ([]*metadatapb.ProcessInfo, error)
	UpdateProcesses(processes []*metadatapb.ProcessInfo) error

	GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error)
}

// AgentUpdate describes the update info for a given agent.
type AgentUpdate struct {
	UpdateInfo *messagespb.AgentUpdateInfo
	AgentID    uuid.UUID
}

// AgentManager handles any agent updates and requests.
type AgentManager interface {
	// RegisterAgent registers a new agent.
	RegisterAgent(info *agentpb.Agent) (uint32, error)

	// UpdateHeartbeat updates the agent heartbeat with the current time.
	UpdateHeartbeat(agentID uuid.UUID) error

	// UpdateAgent updates agent info, such as schema.
	UpdateAgent(info *agentpb.Agent) error

	// Delete agent deletes the agent.
	DeleteAgent(uuid.UUID) error

	// GetActiveAgents gets all of the current active agents.
	GetActiveAgents() ([]*agentpb.Agent, error)

	MessageAgents(agentIDs []uuid.UUID, msg []byte) error
	MessageActiveAgents(msg []byte) error

	ApplyAgentUpdate(update *AgentUpdate) error

	// NewAgentUpdateCursor creates a unique ID for an agent update tracking cursor.
	// It, when used with GetAgentUpdates, can be used by clients of the agent manager
	// to get the initial agent state and track updates as deltas to that state.
	NewAgentUpdateCursor() uuid.UUID

	// DeleteAgentUpdateCursor deletes a cursor from the AgentManager so that it no longer
	// tracks updates.
	DeleteAgentUpdateCursor(cursorID uuid.UUID)

	// GetAgentUpdates returns all of the updates that have occurred for agents since
	// the last invocation of GetAgentUpdates. If GetAgentUpdates has never been called for
	// a given cursorID, the full initial state will be read first.
	GetAgentUpdates(cursorID uuid.UUID) ([]*metadata_servicepb.AgentUpdate, *storepb.ComputedSchema, error)

	// UpdateConfig updates the config for the specified agent.
	UpdateConfig(string, string, string, string) error

	// GetComputedSchema gets the computed schemas
	GetComputedSchema() (*storepb.ComputedSchema, error)
	// GetAgentIDForHostnamePair gets the agent for the given hostnamePair, if it exists.
	GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error)

	// GetServiceCIDR returns the service CIDR for the current cluster.
	GetServiceCIDR() string
	// GetPodCIDRs returns the PodCIDRs for the cluster.
	GetPodCIDRs() []string
}

// AgentUpdateTracker stores the updates (in order) for agents for GetAgentUpdates.
type AgentUpdateTracker struct {
	updates             []*metadata_servicepb.AgentUpdate
	schemaUpdated       bool
	hasReadInitialState bool
}

// newAgentUpdateTracker creates an AgentUpdateTracker in the default state.
func newAgentUpdateTracker() *AgentUpdateTracker {
	return &AgentUpdateTracker{
		updates:             []*metadata_servicepb.AgentUpdate{},
		schemaUpdated:       false,
		hasReadInitialState: false,
	}
}

// clearUpdates clears the agent tracker's current update state.
func (a *AgentUpdateTracker) clearUpdates() {
	a.updates = []*metadata_servicepb.AgentUpdate{}
	a.schemaUpdated = false
}

// AgentManagerImpl is an implementation for AgentManager which talks to the metadata store.
type AgentManagerImpl struct {
	clock    utils.Clock
	agtStore AgentStore
	k8sMdh   *K8sMetadataHandler
	conn     *nats.Conn

	// The agent manager may have multiple clients requesting updates to the current agent state
	// compared to the state they last saw. This map keeps all of the various trackers (per client)
	agentUpdateTrackers map[uuid.UUID]*AgentUpdateTracker
	// Protects agentUpdateTrackers.
	agentUpdateTrackersMutex sync.Mutex
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(agtStore AgentStore, k8sMdh *K8sMetadataHandler, conn *nats.Conn, clock utils.Clock) *AgentManagerImpl {
	agentManager := &AgentManagerImpl{
		clock:               clock,
		agtStore:            agtStore,
		k8sMdh:              k8sMdh,
		conn:                conn,
		agentUpdateTrackers: make(map[uuid.UUID]*AgentUpdateTracker),
	}

	return agentManager
}

// NewAgentUpdateCursor creates a new cursor that keeps track of agent state over time.
func (m *AgentManagerImpl) NewAgentUpdateCursor() uuid.UUID {
	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()
	cursor := uuid.NewV4()
	m.agentUpdateTrackers[cursor] = newAgentUpdateTracker()
	return cursor
}

// DeleteAgentUpdateCursor deletes a created cursor so that it no longer needs to keep
// track of agent updates when it's not used anymore.
func (m *AgentManagerImpl) DeleteAgentUpdateCursor(cursorID uuid.UUID) {
	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()
	delete(m.agentUpdateTrackers, cursorID)
}

// A helper function for all cases where we call m.agtStore.UpdateSchemas
// This should be called instead of m.agtStore.UpdateSchemas in order to make sure that the agent
// schema update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentSchemaWrapper(agentID uuid.UUID, schema []*storepb.TableInfo) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `updateAgentSchemaWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.agtStore.UpdateSchemas(agentID, schema)
	if err != nil {
		log.WithError(err).Errorf("Failed to update agent schema for agent %s", agentID.String())
		return err
	}

	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()

	// Mark this change across all of the agent update trackers.
	for _, tracker := range m.agentUpdateTrackers {
		tracker.schemaUpdated = true
	}

	return nil
}

// A helper function for all cases where we call m.agtStore.DeleteAgent.
// This should be called instead of agtStore.DeleteAgent in order to make sure that the agent
// deletion is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) deleteAgentWrapper(agentID uuid.UUID) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `deleteAgentWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.agtStore.DeleteAgent(agentID)

	if err != nil {
		log.WithError(err).Errorf("Failed to delete agent %s", agentID.String())
		return err
	}

	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()

	// Create a single update object so we don't make one for each tracker.
	update := &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(agentID),
		Update: &metadata_servicepb.AgentUpdate_Deleted{
			Deleted: true,
		},
	}

	// Mark this change across all of the agent update trackers.
	for _, tracker := range m.agentUpdateTrackers {
		tracker.updates = append(tracker.updates, update)
	}

	return nil
}

// A helper function for all cases where we call m.agtStore.CreateAgent.
// This should be called instead of agtStore.CreateAgent in order to make sure that the agent
// creation is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) createAgentWrapper(agentID uuid.UUID, agentInfo *agentpb.Agent) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `createAgentWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.agtStore.CreateAgent(agentID, agentInfo)

	if err != nil {
		log.WithError(err).Errorf("Failed to create agent %s", agentID.String())
		return err
	}

	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()

	// Create a single update object so we don't make one for each tracker.
	update := &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(agentID),
		Update: &metadata_servicepb.AgentUpdate_Agent{
			Agent: agentInfo,
		},
	}

	// Mark this change across all of the agent update trackers.
	for _, tracker := range m.agentUpdateTrackers {
		tracker.updates = append(tracker.updates, update)
	}

	return nil
}

// A helper function for all cases where we call m.agtStore.CreateAgent.
// This should be called instead of agtStore.CreateAgent in order to make sure that the agent
// update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentWrapper(agentID uuid.UUID, agentInfo *agentpb.Agent) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `updateAgentWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.agtStore.UpdateAgent(agentID, agentInfo)

	if err != nil {
		log.WithError(err).Errorf("Failed to update agent %s", agentID.String())
		return err
	}

	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()

	// Create a single update object so we don't make one for each tracker.
	update := &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(agentID),
		Update: &metadata_servicepb.AgentUpdate_Agent{
			Agent: agentInfo,
		},
	}

	// Mark this change across all of the agent update trackers.
	for _, tracker := range m.agentUpdateTrackers {
		tracker.updates = append(tracker.updates, update)
	}

	return nil
}

// A helper function for all cases where we call m.agtStore.UpdateAgentDataInfo.
// This should be called instead of agtStore.UpdateAgentDataInfo in order to make sure that the agent
// update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentDataInfoWrapper(agentID uuid.UUID, agentDataInfo *messagespb.AgentDataInfo) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `updateAgentDataInfoWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.agtStore.UpdateAgentDataInfo(agentID, agentDataInfo)

	if err != nil {
		log.WithError(err).Errorf("Failed to update agent data info for agent %s", agentID.String())
		return err
	}

	m.agentUpdateTrackersMutex.Lock()
	defer m.agentUpdateTrackersMutex.Unlock()

	// Create a single update object so we don't make one for each tracker.
	update := &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(agentID),
		Update: &metadata_servicepb.AgentUpdate_DataInfo{
			DataInfo: agentDataInfo,
		},
	}

	// Mark this change across all of the agent update trackers.
	for _, tracker := range m.agentUpdateTrackers {
		tracker.updates = append(tracker.updates, update)
	}

	return nil
}

// ApplyAgentUpdate updates the metadata store with the information from the agent update.
func (m *AgentManagerImpl) ApplyAgentUpdate(update *AgentUpdate) error {
	resp, err := m.agtStore.GetAgent(update.AgentID)
	if err != nil {
		log.WithError(err).Error("Failed to get agent")
		return err
	} else if resp == nil {
		log.Info("Ignoring update for agent that has already been deleted")
		return nil
	}

	err = m.handleCreatedProcesses(update.UpdateInfo.ProcessCreated)
	if err != nil {
		log.WithError(err).Error("Error when creating new processes")
	}

	err = m.handleTerminatedProcesses(update.UpdateInfo.ProcessTerminated)
	if err != nil {
		log.WithError(err).Error("Error when updating terminated processes")
	}
	if update.UpdateInfo.Data != nil {
		err = m.updateAgentDataInfoWrapper(update.AgentID, update.UpdateInfo.Data)
		if err != nil {
			return err
		}
	}
	if !update.UpdateInfo.DoesUpdateSchema {
		return nil
	}
	return m.updateAgentSchemaWrapper(update.AgentID, update.UpdateInfo.Schema)
}

func (m *AgentManagerImpl) handleCreatedProcesses(processes []*metadatapb.ProcessCreated) error {
	if processes == nil || len(processes) == 0 {
		return nil
	}

	processInfos := make([]*metadatapb.ProcessInfo, len(processes))
	for i, p := range processes {
		pPb := &metadatapb.ProcessInfo{
			UPID:             p.UPID,
			PID:              p.PID,
			StartTimestampNS: p.StartTimestampNS,
			ProcessArgs:      p.Cmdline,
			CID:              p.CID,
		}
		processInfos[i] = pPb
	}

	return m.agtStore.UpdateProcesses(processInfos)
}

func (m *AgentManagerImpl) handleTerminatedProcesses(processes []*metadatapb.ProcessTerminated) error {
	if processes == nil || len(processes) == 0 {
		return nil
	}

	upids := make([]*types.UInt128, len(processes))
	for i, p := range processes {
		upids[i] = types.UInt128FromProto(p.UPID)
	}

	pInfos, err := m.agtStore.GetProcesses(upids)
	if err != nil {
		log.WithError(err).Error("Could not get processes when trying to update terminated processes")
		return err
	}

	var updatedProcesses []*metadatapb.ProcessInfo
	for i, p := range pInfos {
		if p != (*metadatapb.ProcessInfo)(nil) {
			p.StopTimestampNS = processes[i].StopTimestampNS
			updatedProcesses = append(updatedProcesses, p)
		}
	}

	return m.agtStore.UpdateProcesses(updatedProcesses)
}

// NewAgentManager creates a new agent manager.
// TODO (vihang/michelle): Figure out a better solution than passing in the k8s controller.
// We need the k8sMDh to get CIDR info right now.
func NewAgentManager(agtStore AgentStore, k8sMdh *K8sMetadataHandler, conn *nats.Conn) *AgentManagerImpl {
	clock := utils.SystemClock{}
	return NewAgentManagerWithClock(agtStore, k8sMdh, conn, clock)
}

// RegisterAgent creates a new agent.
func (m *AgentManagerImpl) RegisterAgent(agent *agentpb.Agent) (asid uint32, err error) {
	info := agent.Info

	// Check if agent already exists.
	aUUID := utils.UUIDFromProtoOrNil(info.AgentID)

	resp, err := m.agtStore.GetAgent(aUUID)
	if err != nil {
		log.WithError(err).Fatal("Failed to get agent")
	} else if resp != nil {
		return 0, errors.New("Agent already exists")
	}

	// Get ASID for the new agent.
	asid, err = m.agtStore.GetASID()
	if err != nil {
		return 0, err
	}

	infoPb := &agentpb.Agent{
		Info:            info,
		CreateTimeNS:    m.clock.Now().UnixNano(),
		LastHeartbeatNS: m.clock.Now().UnixNano(),
		ASID:            asid,
	}

	// Add this agent to the updated agents list.
	err = m.createAgentWrapper(aUUID, infoPb)
	if err != nil {
		return 0, err
	}

	return asid, nil
}

// DeleteAgent deletes the agent with the given ID.
func (m *AgentManagerImpl) DeleteAgent(agentID uuid.UUID) error {
	err := m.deleteAgentWrapper(agentID)
	if err != nil {
		log.WithError(err).Fatal("Failed to delete agent from etcd")
	}

	return err
}

// UpdateHeartbeat updates the agent heartbeat with the current time.
func (m *AgentManagerImpl) UpdateHeartbeat(agentID uuid.UUID) error {
	// Get current AgentData.
	agent, err := m.agtStore.GetAgent(agentID)
	if err != nil {
		return err
	}
	if agent == nil {
		return errors.New("Agent does not exist")
	}

	// Update LastHeartbeatNS in AgentData.
	agent.LastHeartbeatNS = m.clock.Now().UnixNano()

	err = m.updateAgentWrapper(agentID, agent)
	if err != nil {
		return err
	}

	return nil
}

// UpdateAgent updates agent info, such as schema.
func (m *AgentManagerImpl) UpdateAgent(info *agentpb.Agent) error {
	// TODO(michelle): Implement once we figure out how the agent info (schemas, etc) looks.
	// Make sure to add any updates to the agent updates list.
	return nil
}

// GetActiveAgents gets all of the current active agents.
func (m *AgentManagerImpl) GetActiveAgents() ([]*agentpb.Agent, error) {
	var agents []*agentpb.Agent

	agentPbs, err := m.agtStore.GetAgents()
	if err != nil {
		return agents, err
	}

	return agentPbs, nil
}

// MessageAgents sends the message to the given agentIDs.
func (m *AgentManagerImpl) MessageAgents(agentIDs []uuid.UUID, msg []byte) error {
	// Send request to all agents.
	var errs []error
	for _, agentID := range agentIDs {
		topic := GetAgentTopicFromUUID(agentID)

		err := m.conn.Publish(topic, msg)
		if err != nil {
			// Don't fail on first error, just collect all errors and keep sending.
			errs = append(errs, err)
		}
	}

	// Pick the first err if any to return.
	if len(errs) > 0 {
		return errs[0]
	}
	return nil
}

// MessageActiveAgents sends the message to all active agents.
func (m *AgentManagerImpl) MessageActiveAgents(msg []byte) error {
	agents, err := m.GetActiveAgents()
	if err != nil {
		return err
	}

	agentIDs := make([]uuid.UUID, len(agents))

	for i, a := range agents {
		agentIDs[i] = utils.UUIDFromProtoOrNil(a.Info.AgentID)
	}
	return m.MessageAgents(agentIDs, msg)
}

// UpdateConfig updates the config key and value for the specified agent.
func (m *AgentManagerImpl) UpdateConfig(ns string, podName string, key string, value string) error {
	// Find the agent ID for the agent with the given name.
	agentID, err := m.agtStore.GetAgentIDFromPodName(podName)
	if err != nil || agentID == "" {
		return errors.New("Could not find agent with the given name")
	}

	// Send the config update to the agent over NATS.
	updateReq := messagespb.VizierMessage{
		Msg: &messagespb.VizierMessage_ConfigUpdateMessage{
			ConfigUpdateMessage: &messagespb.ConfigUpdateMessage{
				Msg: &messagespb.ConfigUpdateMessage_ConfigUpdateRequest{
					ConfigUpdateRequest: &messagespb.ConfigUpdateRequest{
						Key:   key,
						Value: value,
					},
				},
			},
		},
	}
	msg, err := updateReq.Marshal()
	if err != nil {
		return err
	}
	topic := GetAgentTopicFromUUID(uuid.FromStringOrNil(agentID))
	err = m.conn.Publish(topic, msg)
	if err != nil {
		return err
	}
	return nil
}

// GetAgentUpdates returns the latest agent status since the last call to GetAgentUpdates().
// if the input cursor has never read the initial state before, the full initial agent state is read out.
// Afterwards, the changes to the agent state are read out as a delta to the previous state.
func (m *AgentManagerImpl) GetAgentUpdates(cursorID uuid.UUID) ([]*metadata_servicepb.AgentUpdate,
	*storepb.ComputedSchema, error) {

	schemaUpdated := false
	var updatedAgentsUpdates []*metadata_servicepb.AgentUpdate

	var err error
	var hasReadInitialState bool
	func() {
		// Note: Due to the fact that we do not lock the entirety of this GetAgentUpdates function (and the various
		// wrapper functions updating the metadata store with new agent state), there may be inconsistency in the
		// results outputted by GetAgentUpdates. By locking and clearing the state first, then releasing the lock before
		// reading the metadata store state, a given update may be send twice by this function, so downstream consumers of
		// GetAgentUpdates need to be resilient to getting the same update twice. If we want perfect consistency here, we
		// will either need to lock the entire function (which includes network calls which we don't want to lock across)
		// or change the way a lot of the metadata store works.
		m.agentUpdateTrackersMutex.Lock()
		defer m.agentUpdateTrackersMutex.Unlock()

		tracker, present := m.agentUpdateTrackers[cursorID]
		if !present {
			err = fmt.Errorf("Agent update cursor %s is not present in AgentManager", cursorID.String())
			return
		}
		if tracker == nil {
			err = fmt.Errorf("Agent update cursor %s is nil in AgentManager", cursorID.String())
			return
		}

		schemaUpdated = tracker.schemaUpdated
		updatedAgentsUpdates = tracker.updates

		// Reset the state now that we have popped off the latest updates.
		tracker.clearUpdates()
		hasReadInitialState = tracker.hasReadInitialState

		if !tracker.hasReadInitialState {
			tracker.hasReadInitialState = true
		}
	}()

	if err != nil {
		return nil, nil, err
	}

	var agentUpdates []*metadata_servicepb.AgentUpdate
	var computedSchema *storepb.ComputedSchema

	if !hasReadInitialState || schemaUpdated {
		computedSchema, err = m.agtStore.GetComputedSchema()
		if err != nil {
			return nil, nil, err
		}
	}

	if hasReadInitialState {
		agentUpdates = updatedAgentsUpdates
	} else {
		updatedAgents, err := m.agtStore.GetAgents()
		if err != nil {
			return nil, nil, err
		}
		for _, agentInfo := range updatedAgents {
			agentUpdates = append(agentUpdates, &metadata_servicepb.AgentUpdate{
				AgentID: agentInfo.Info.AgentID,
				Update: &metadata_servicepb.AgentUpdate_Agent{
					Agent: agentInfo,
				},
			})
		}
		updatedAgentsDataInfo, err := m.agtStore.GetAgentsDataInfo()
		if err != nil {
			return nil, nil, err
		}
		for agentID, agentDataInfo := range updatedAgentsDataInfo {
			agentUpdates = append(agentUpdates, &metadata_servicepb.AgentUpdate{
				AgentID: utils.ProtoFromUUID(agentID),
				Update: &metadata_servicepb.AgentUpdate_DataInfo{
					DataInfo: agentDataInfo,
				},
			})
		}
	}

	return agentUpdates, computedSchema, nil
}

// GetComputedSchema gets the computed schemas
func (m *AgentManagerImpl) GetComputedSchema() (*storepb.ComputedSchema, error) {
	return m.agtStore.GetComputedSchema()
}

// GetAgentIDForHostnamePair gets the agent for the given hostnamePair, if it exists.
func (m *AgentManagerImpl) GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error) {
	return m.agtStore.GetAgentIDForHostnamePair(hnPair)
}

// GetServiceCIDR returns the service CIDR for the current cluster.
func (m *AgentManagerImpl) GetServiceCIDR() string {
	return m.k8sMdh.GetServiceCIDR()
}

// GetPodCIDRs returns the PodCIDRs for the cluster.
func (m *AgentManagerImpl) GetPodCIDRs() []string {
	return m.k8sMdh.GetPodCIDRs()
}
