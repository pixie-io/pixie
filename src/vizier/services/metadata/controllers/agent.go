package controllers

import (
	"errors"
	"sync"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	metadata_servicepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// AgentExpirationTimeout is the amount of time that we should wait to receive a heartbeat
// from an agent before marking it as unhealthy.
const AgentExpirationTimeout int64 = 1e9 * 60 // 60 seconds in nano-seconds.
// MaxAgentUpdates is the total number of updates each agent can have on its queue.
const MaxAgentUpdates int = 10000

// MaxUpdatesToDequeue is the maximum number of updates we should dequeue at a time.
const MaxUpdatesToDequeue int = 100

// MaxBytesToDequeue is the maximum number of bytes we should dequeue at a time. We can dequeue .9MB.
const MaxBytesToDequeue int = 900000

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

	// UpdateAgentState will run through all agents and delete those
	// that are dead.
	UpdateAgentState() error

	// GetActiveAgents gets all of the current active agents.
	GetActiveAgents() ([]*agentpb.Agent, error)

	AddToFrontOfAgentQueue(string, *metadatapb.ResourceUpdate) error
	GetFromAgentQueue(string) ([]*metadatapb.ResourceUpdate, error)

	AddToUpdateQueue(uuid.UUID, *messagespb.AgentUpdateInfo)

	GetMetadataUpdates(hostname *HostnameIPPair) ([]*metadatapb.ResourceUpdate, error)

	AddUpdatesToAgentQueue(string, []*metadatapb.ResourceUpdate) error

	ApplyAgentUpdate(update *AgentUpdate) error
	HandleUpdate(*UpdateMessage)

	// GetAgentUpdates returns all of the updates that have occurred for agents since
	// the last invocation of GetAgentUpdates. if readInitialState is set to true, it
	// reads out the entire current state first.
	GetAgentUpdates(readFullState bool) ([]*metadata_servicepb.AgentUpdate, *storepb.ComputedSchema, error)
}

// AgentUpdateTracker stores the updates (in order) for agents for GetAgentUpdates.
type AgentUpdateTracker struct {
	updates       []*metadata_servicepb.AgentUpdate
	schemaUpdated bool
}

// EmptyAgentUpdateTracker creates an AgentUpdateTracker in the default state.
func EmptyAgentUpdateTracker() AgentUpdateTracker {
	return AgentUpdateTracker{
		updates:       []*metadata_servicepb.AgentUpdate{},
		schemaUpdated: false,
	}
}

// AgentManagerImpl is an implementation for AgentManager which talks to the metadata store.
type AgentManagerImpl struct {
	clock              utils.Clock
	mds                MetadataStore
	updateCh           chan *AgentUpdate
	agentQueues        map[string]AgentQueue
	queueMu            sync.Mutex
	updatedAgentsMutex sync.Mutex
	updatedAgents      AgentUpdateTracker
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(mds MetadataStore, clock utils.Clock) *AgentManagerImpl {
	c := make(chan *AgentUpdate)

	agentManager := &AgentManagerImpl{
		clock:         clock,
		mds:           mds,
		updateCh:      c,
		agentQueues:   make(map[string]AgentQueue),
		updatedAgents: EmptyAgentUpdateTracker(),
	}

	go agentManager.processAgentUpdates()

	return agentManager
}

func (m *AgentManagerImpl) processAgentUpdates() {
	for {
		msg, more := <-m.updateCh
		if !more {
			return
		}

		err := m.ApplyAgentUpdate(msg)
		if err != nil {
			// Add update back to the queue to retry.
			m.updateCh <- msg
		}
	}
}

// A helper function for all cases where we call m.mds.UpdateSchemas
// This should be called instead of m.mds.UpdateSchemas in order to make sure that the agent
// schema update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentSchemaWrapper(agentID uuid.UUID, schema []*storepb.TableInfo) error {
	m.updatedAgentsMutex.Lock()
	defer m.updatedAgentsMutex.Unlock()

	m.updatedAgents.schemaUpdated = true
	return m.mds.UpdateSchemas(agentID, schema)
}

// A helper function for all cases where we call m.mds.DeleteAgent.
// This should be called instead of mds.DeleteAgent in order to make sure that the agent
// deletion is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) deleteAgentWrapper(agentID uuid.UUID) error {
	m.updatedAgentsMutex.Lock()
	defer m.updatedAgentsMutex.Unlock()

	// Track the deletion of this agent for the agent updates list.
	m.updatedAgents.updates = append(m.updatedAgents.updates, &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(&agentID),
		Update: &metadata_servicepb.AgentUpdate_Deleted{
			Deleted: true,
		},
	})
	return m.mds.DeleteAgent(agentID)
}

// A helper function for all cases where we call m.mds.CreateAgent.
// This should be called instead of mds.CreateAgent in order to make sure that the agent
// creation is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) createAgentWrapper(agentID uuid.UUID, agentInfo *agentpb.Agent) error {
	m.updatedAgentsMutex.Lock()
	defer m.updatedAgentsMutex.Unlock()

	m.updatedAgents.updates = append(m.updatedAgents.updates, &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(&agentID),
		Update: &metadata_servicepb.AgentUpdate_Agent{
			Agent: agentInfo,
		},
	})
	return m.mds.CreateAgent(agentID, agentInfo)
}

// A helper function for all cases where we call m.mds.CreateAgent.
// This should be called instead of mds.CreateAgent in order to make sure that the agent
// update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentWrapper(agentID uuid.UUID, agentInfo *agentpb.Agent) error {
	m.updatedAgentsMutex.Lock()
	defer m.updatedAgentsMutex.Unlock()

	m.updatedAgents.updates = append(m.updatedAgents.updates, &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(&agentID),
		Update: &metadata_servicepb.AgentUpdate_Agent{
			Agent: agentInfo,
		},
	})
	return m.mds.UpdateAgent(agentID, agentInfo)
}

// A helper function for all cases where we call m.mds.UpdateAgentDataInfo.
// This should be called instead of mds.UpdateAgentDataInfo in order to make sure that the agent
// update is tracked in the our agent state change tracker (updatedAgents).
func (m *AgentManagerImpl) updateAgentDataInfoWrapper(agentID uuid.UUID, agentDataInfo *messagespb.AgentDataInfo) error {
	m.updatedAgentsMutex.Lock()
	defer m.updatedAgentsMutex.Unlock()

	m.updatedAgents.updates = append(m.updatedAgents.updates, &metadata_servicepb.AgentUpdate{
		AgentID: utils.ProtoFromUUID(&agentID),
		Update: &metadata_servicepb.AgentUpdate_DataInfo{
			DataInfo: agentDataInfo,
		},
	})
	return m.mds.UpdateAgentDataInfo(agentID, agentDataInfo)
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

// AddToUpdateQueue adds the container/schema update to a queue for updates to the metadata store.
func (m *AgentManagerImpl) AddToUpdateQueue(agentID uuid.UUID, update *messagespb.AgentUpdateInfo) {
	agentUpdate := &AgentUpdate{
		UpdateInfo: update,
		AgentID:    agentID,
	}
	m.updateCh <- agentUpdate
}

// NewAgentManager creates a new agent manager.
func NewAgentManager(mds MetadataStore) *AgentManagerImpl {
	clock := utils.SystemClock{}
	return NewAgentManagerWithClock(mds, clock)
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

	// Check there's an existing agent for the hostname.
	hostname := ""
	if !info.Capabilities.CollectsData {
		hostname = info.HostInfo.Hostname
	}
	hostnameAgID, err := m.mds.GetAgentIDForHostnamePair(&HostnameIPPair{hostname, info.HostInfo.HostIP})
	if err != nil {
		log.WithError(err).Fatal("Failed to get agent hostname")
	} else if hostnameAgID != "" {
		delAgID, err := uuid.FromString(hostnameAgID)
		if err != nil {
			log.WithError(err).Fatal("Could not parse agent ID")
		}
		err = m.deleteAgentWrapper(delAgID)
		if err != nil {
			log.WithError(err).Fatal("Could not delete agent for hostname")
		}
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

// UpdateAgentState will run through all agents and delete those
// that are dead.
func (m *AgentManagerImpl) UpdateAgentState() error {
	currentTime := m.clock.Now().UnixNano()

	agentPbs, err := m.mds.GetAgents()
	if err != nil {
		return err
	}

	for _, agentPb := range agentPbs {
		if currentTime-agentPb.LastHeartbeatNS > AgentExpirationTimeout {
			uid, err := utils.UUIDFromProto(agentPb.Info.AgentID)
			if err != nil {
				log.WithError(err).Fatal("Could not convert UUID to proto")
			}
			err = m.deleteAgentWrapper(uid)
			if err != nil {
				log.WithError(err).Fatal("Failed to delete agent from etcd")
			}
			m.closeAgentQueue(uid)
		}
	}

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
		return errors.New("Agent queue full, could not add to queue")
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
		if err != nil {
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
		return errors.New("Agent queue full, could not add to queue")
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

// GetAgentUpdates returns the latest agent status since the last call to GetAgentUpdates().
// When readInitialState is set to true, the full agent state is read out.
// Afterwards, the changes to the agent state are read.
// It is assumed that there will only be one subscriber to the agent updates because calls to this
// function will reset the state across the agent manager.
// This should be okay because only the query broker needs to subscribe to these state changes for now,
// but it will need to be scaled if we have multiple query brokers talking to a single metadata service.
func (m *AgentManagerImpl) GetAgentUpdates(readInitialState bool) ([]*metadata_servicepb.AgentUpdate,
	*storepb.ComputedSchema, error) {
	m.updatedAgentsMutex.Lock()
	defer m.updatedAgentsMutex.Unlock()

	var agentUpdates []*metadata_servicepb.AgentUpdate
	var computedSchema *storepb.ComputedSchema
	var err error

	if readInitialState || m.updatedAgents.schemaUpdated {
		computedSchema, err = m.mds.GetComputedSchema()
		if err != nil {
			return nil, nil, err
		}
	}

	if !readInitialState {
		agentUpdates = m.updatedAgents.updates
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
				AgentID: utils.ProtoFromUUID(&agentID),
				Update: &metadata_servicepb.AgentUpdate_DataInfo{
					DataInfo: agentDataInfo,
				},
			})
		}
	}

	// Reset the state now that we have popped off the latest updates.
	m.updatedAgents = EmptyAgentUpdateTracker()
	return agentUpdates, computedSchema, nil
}
