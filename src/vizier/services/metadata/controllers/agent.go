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
}

// AgentManagerImpl is an implementation for AgentManager which talks to the metadata store.
type AgentManagerImpl struct {
	clock       utils.Clock
	mds         MetadataStore
	updateCh    chan *AgentUpdate
	agentQueues map[string]AgentQueue
	queueMu     sync.Mutex
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(mds MetadataStore, clock utils.Clock) *AgentManagerImpl {
	c := make(chan *AgentUpdate)

	agentManager := &AgentManagerImpl{
		clock:       clock,
		mds:         mds,
		updateCh:    c,
		agentQueues: make(map[string]AgentQueue),
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

// ApplyAgentUpdate updates the metadata store with the information from the agent update.
func (m *AgentManagerImpl) ApplyAgentUpdate(update *AgentUpdate) error {
	err := m.handleCreatedProcesses(update.UpdateInfo.ProcessCreated)
	if err != nil {
		log.WithError(err).Error("Error when creating new processes")
	}

	err = m.handleTerminatedProcesses(update.UpdateInfo.ProcessTerminated)
	if err != nil {
		log.WithError(err).Error("Error when updating terminated processes")
	}
	if update.UpdateInfo.Data != nil {
		err = m.mds.UpdateAgentDataInfo(update.AgentID, update.UpdateInfo.Data)
		if err != nil {
			return err
		}
	}
	if !update.UpdateInfo.DoesUpdateSchema {
		return nil
	}
	return m.mds.UpdateSchemas(update.AgentID, update.UpdateInfo.Schema)
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
		err = m.mds.DeleteAgent(delAgID)
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

	err = m.mds.CreateAgent(aUUID, infoPb)
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

	err = m.mds.UpdateAgent(agentID, agent)
	if err != nil {
		return err
	}

	return nil
}

// UpdateAgent updates agent info, such as schema.
func (m *AgentManagerImpl) UpdateAgent(info *agentpb.Agent) error {
	// TODO(michelle): Implement once we figure out how the agent info (schemas, etc) looks.
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
			err = m.mds.DeleteAgent(uid)
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
