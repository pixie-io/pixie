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
	"pixielabs.ai/pixielabs/src/shared/types/go"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	metadata_servicepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// AgentExpirationTimeout is the amount of time that we should wait to receive a heartbeat
// from an agent before marking it as unhealthy.
const AgentExpirationTimeout time.Duration = 1 * time.Minute

// MaxAgentUpdates is the total number of updates each agent can have on its queue.
const MaxAgentUpdates int = 10000

// MaxUpdatesToDequeue is the maximum number of updates we should dequeue at a time.
const MaxUpdatesToDequeue int = 100

// MaxBytesToDequeue is the maximum number of bytes we should dequeue at a time. We can dequeue .9MB.
const MaxBytesToDequeue int = 900000

// ErrAgentQueueFull is an error that occurs when the agent update queue is full and no more updates can be queued.
var ErrAgentQueueFull = errors.New("Agent queue full, could not add to queue")

// AgentUpdate describes the update info for a given agent.
type AgentUpdate struct {
	UpdateInfo *messagespb.AgentUpdateInfo
	AgentID    uuid.UUID
}

// AgentQueue represents the queue of resource updates that should be sent to an agent.
type AgentQueue struct {
	FailedQueue chan *metadatapb.ResourceUpdate
	Queue       chan *metadatapb.ResourceUpdate
	Mu          sync.Mutex
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

	AddToFrontOfAgentQueue(string, *metadatapb.ResourceUpdate) error
	GetFromAgentQueue(string) ([]*metadatapb.ResourceUpdate, error)

	GetMetadataUpdates(hostname *HostnameIPPair) ([]*metadatapb.ResourceUpdate, error)

	AddUpdatesToAgentQueue(string, []*metadatapb.ResourceUpdate) error

	ApplyAgentUpdate(update *AgentUpdate) error
	HandleUpdate(*UpdateMessage)

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
	clock       utils.Clock
	mds         MetadataStore
	agentQueues map[string]AgentQueue
	queueMu     sync.Mutex
	conn        *nats.Conn

	// The agent manager may have multiple clients requesting updates to the current agent state
	// compared to the state they last saw. This map keeps all of the various trackers (per client)
	agentUpdateTrackers map[uuid.UUID]*AgentUpdateTracker
	// Protects agentUpdateTrackers.
	agentUpdateTrackersMutex sync.Mutex
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(mds MetadataStore, conn *nats.Conn, clock utils.Clock) *AgentManagerImpl {
	agentManager := &AgentManagerImpl{
		clock:               clock,
		mds:                 mds,
		conn:                conn,
		agentQueues:         make(map[string]AgentQueue),
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

// A helper function for all cases where we call m.mds.UpdateSchemas
// This should be called instead of m.mds.UpdateSchemas in order to make sure that the agent
// schema update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentSchemaWrapper(agentID uuid.UUID, schema []*storepb.TableInfo) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `updateAgentSchemaWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.mds.UpdateSchemas(agentID, schema)
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

// A helper function for all cases where we call m.mds.DeleteAgent.
// This should be called instead of mds.DeleteAgent in order to make sure that the agent
// deletion is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) deleteAgentWrapper(agentID uuid.UUID) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `deleteAgentWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.mds.DeleteAgent(agentID)

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

// A helper function for all cases where we call m.mds.CreateAgent.
// This should be called instead of mds.CreateAgent in order to make sure that the agent
// creation is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) createAgentWrapper(agentID uuid.UUID, agentInfo *agentpb.Agent) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `createAgentWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.mds.CreateAgent(agentID, agentInfo)

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

// A helper function for all cases where we call m.mds.CreateAgent.
// This should be called instead of mds.CreateAgent in order to make sure that the agent
// update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentWrapper(agentID uuid.UUID, agentInfo *agentpb.Agent) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `updateAgentWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.mds.UpdateAgent(agentID, agentInfo)

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

// A helper function for all cases where we call m.mds.UpdateAgentDataInfo.
// This should be called instead of mds.UpdateAgentDataInfo in order to make sure that the agent
// update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentDataInfoWrapper(agentID uuid.UUID, agentDataInfo *messagespb.AgentDataInfo) error {
	// Note: Metadata store state must be updated before the agent tracker state is updated, otherwise the
	// update may be missed by the agent tracker when reading the initial agent state.
	// We cannot lock the entire call to `updateAgentDataInfoWrapper`, which would allow for perfect consistency,
	// since the update to the metadata store may hit the network.
	err := m.mds.UpdateAgentDataInfo(agentID, agentDataInfo)

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
	resp, err := m.mds.GetAgent(update.AgentID)
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

	return m.mds.UpdateProcesses(processInfos)
}

func (m *AgentManagerImpl) handleTerminatedProcesses(processes []*metadatapb.ProcessTerminated) error {
	if processes == nil || len(processes) == 0 {
		return nil
	}

	upids := make([]*types.UInt128, len(processes))
	for i, p := range processes {
		upids[i] = types.UInt128FromProto(p.UPID)
	}

	pInfos, err := m.mds.GetProcesses(upids)
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

	return m.mds.UpdateProcesses(updatedProcesses)
}

// NewAgentManager creates a new agent manager.
func NewAgentManager(mds MetadataStore, conn *nats.Conn) *AgentManagerImpl {
	clock := utils.SystemClock{}
	return NewAgentManagerWithClock(mds, conn, clock)
}

// RegisterAgent creates a new agent.
func (m *AgentManagerImpl) RegisterAgent(agent *agentpb.Agent) (asid uint32, err error) {
	info := agent.Info

	// Check if agent already exists.
	aUUID := utils.UUIDFromProtoOrNil(info.AgentID)

	resp, err := m.mds.GetAgent(aUUID)
	if err != nil {
		log.WithError(err).Fatal("Failed to get agent")
	} else if resp != nil {
		return 0, errors.New("Agent already exists")
	}

	// Get ASID for the new agent.
	asid, err = m.mds.GetASID()
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
	m.closeAgentQueue(agentID)

	return err
}

// UpdateHeartbeat updates the agent heartbeat with the current time.
func (m *AgentManagerImpl) UpdateHeartbeat(agentID uuid.UUID) error {
	// Get current AgentData.
	agent, err := m.mds.GetAgent(agentID)
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

func (m *AgentManagerImpl) closeAgentQueue(uid uuid.UUID) {
	m.queueMu.Lock()
	defer m.queueMu.Unlock()

	if queue, ok := m.agentQueues[uid.String()]; ok {
		close(queue.Queue)
		close(queue.FailedQueue)
		delete(m.agentQueues, uid.String())
	}
}

// GetActiveAgents gets all of the current active agents.
func (m *AgentManagerImpl) GetActiveAgents() ([]*agentpb.Agent, error) {
	var agents []*agentpb.Agent

	agentPbs, err := m.mds.GetAgents()
	if err != nil {
		return agents, err
	}

	return agentPbs, nil
}

func (m *AgentManagerImpl) getOrCreateAgentQueue(agentID string) *AgentQueue {
	m.queueMu.Lock()
	defer m.queueMu.Unlock()

	if currQueue, ok := m.agentQueues[agentID]; ok {
		return &currQueue
	}

	// Create update queue for agent.
	queue := AgentQueue{
		FailedQueue: make(chan *metadatapb.ResourceUpdate, MaxAgentUpdates),
		Queue:       make(chan *metadatapb.ResourceUpdate, MaxAgentUpdates),
	}
	m.agentQueues[agentID] = queue
	return &queue
}

// AddToFrontOfAgentQueue adds the given value to the front of the agent's update queue.
func (m *AgentManagerImpl) AddToFrontOfAgentQueue(agentID string, value *metadatapb.ResourceUpdate) error {
	currQueue := m.getOrCreateAgentQueue(agentID)
	currQueue.Mu.Lock()
	defer currQueue.Mu.Unlock()

	if len(currQueue.FailedQueue) >= MaxAgentUpdates {
		return ErrAgentQueueFull
	}
	currQueue.FailedQueue <- value
	return nil
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (m *AgentManagerImpl) GetFromAgentQueue(agentID string) ([]*metadatapb.ResourceUpdate, error) {
	m.queueMu.Lock()
	currQueue, ok := m.agentQueues[agentID]
	m.queueMu.Unlock()

	if !ok {
		return nil, nil
	}

	currQueue.Mu.Lock()
	defer currQueue.Mu.Unlock()

	var updates []*metadatapb.ResourceUpdate
	dequeuedSize := 0

	fQLen := len(currQueue.FailedQueue)
	qLen := len(currQueue.Queue)

	for i := 0; i < fQLen; i++ {
		update := <-currQueue.FailedQueue

		if update.Size()+dequeuedSize > MaxBytesToDequeue {
			// We've dequeued the max # of bytes. Put this back on the agent queue.
			log.Info("Dequeued max number of bytes from agent queue")
			if update.Size() > MaxBytesToDequeue {
				log.WithField("update", update).Info("Single update larger than MaxBytesToDequeue... Dropping")
				return updates, nil
			}
			currQueue.FailedQueue <- update
			return updates, nil
		}
		updates = append(updates, update)
		dequeuedSize += update.Size()
	}

	for i := 0; i < qLen; i++ {
		update := <-currQueue.Queue

		if update.Size()+dequeuedSize > MaxBytesToDequeue {
			// We've dequeued the max # of bytes. Put this back on the agent queue.
			log.Info("Dequeued max number of bytes from agent queue")
			if update.Size() > MaxBytesToDequeue {
				log.WithField("update", update).Info("Single update larger than MaxBytesToDequeue... Dropping")
				return updates, nil
			}
			currQueue.FailedQueue <- update
			return updates, nil
		}
		updates = append(updates, update)
		dequeuedSize += update.Size()
	}
	return updates, nil
}

// HandleUpdate processes a metadata update and adds it to the appropriate agent queues.
func (m *AgentManagerImpl) HandleUpdate(update *UpdateMessage) {
	updatePb := update.Message
	hostnames := update.Hostnames
	nodeSpecific := update.NodeSpecific

	agents, err := m.mds.GetAgentsForHostnamePairs(&hostnames)
	if err != nil {
		return
	}

	log.WithField("agents", agents).WithField("hostnames", hostnames).
		WithField("update", updatePb).Trace("Adding update to agent queue for agents")

	allAgents := make([]string, len(agents))
	copy(allAgents, agents)

	if !nodeSpecific {
		// This update is not for a specific node. Send to Kelvins as well.
		kelvinIDs, err := m.mds.GetKelvinIDs()
		if err != nil {
			log.WithError(err).Error("Could not get kelvin IDs")
		} else {
			allAgents = append(allAgents, kelvinIDs...)
		}
		log.WithField("kelvins", kelvinIDs).WithField("update", updatePb).Trace("Adding update to agent queue for kelvins")
	}

	for _, agent := range allAgents {
		err = m.AddUpdatesToAgentQueue(agent, []*metadatapb.ResourceUpdate{updatePb})
		if err != nil && err != ErrAgentQueueFull {
			log.WithError(err).Error("Could not write service update to agent update queue.")
		}
	}
}

// AddUpdatesToAgentQueue adds the given updates in order to the agent's update queue.
func (m *AgentManagerImpl) AddUpdatesToAgentQueue(agentID string, updates []*metadatapb.ResourceUpdate) error {
	currQueue := m.getOrCreateAgentQueue(agentID)
	currQueue.Mu.Lock()
	defer currQueue.Mu.Unlock()

	if len(currQueue.Queue)+len(updates) >= MaxAgentUpdates {
		return ErrAgentQueueFull
	}
	for _, update := range updates {
		currQueue.Queue <- update
	}
	return nil
}

// GetMetadataUpdates gets all updates from the metadata store. If no hostname is specified, it fetches all updates
// regardless of hostname.
func (m *AgentManagerImpl) GetMetadataUpdates(hostname *HostnameIPPair) ([]*metadatapb.ResourceUpdate, error) {
	return m.mds.GetMetadataUpdates(hostname)
}

// UpdateConfig updates the config key and value for the specified agent.
func (m *AgentManagerImpl) UpdateConfig(ns string, podName string, key string, value string) error {
	// Find the agent ID for the agent with the given name.
	agentID, err := m.mds.GetAgentIDFromPodName(podName)
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
		computedSchema, err = m.mds.GetComputedSchema()
		if err != nil {
			return nil, nil, err
		}
	}

	if hasReadInitialState {
		agentUpdates = updatedAgentsUpdates
	} else {
		updatedAgents, err := m.mds.GetAgents()
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
		updatedAgentsDataInfo, err := m.mds.GetAgentsDataInfo()
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
