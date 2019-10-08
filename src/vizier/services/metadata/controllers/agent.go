package controllers

import (
	"context"
	"errors"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/clientv3util"
	"github.com/coreos/etcd/clientv3/concurrency"
	"github.com/gogo/protobuf/proto"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	data "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// AgentExpirationTimeout is the amount of time that we should wait to receive a heartbeat
// from an agent before marking it as unhealthy.
const AgentExpirationTimeout int64 = 1e9 * 60 // 60 seconds in nano-seconds.

// AgentInfo describes information about an agent.
type AgentInfo struct {
	// Unix time of the last heart beat (current system clock)
	LastHeartbeatNS int64
	CreateTimeNS    int64
	AgentID         uuid.UUID
	Hostname        string
}

// AgentUpdate describes the update info for a given agent.
type AgentUpdate struct {
	UpdateInfo *messagespb.AgentUpdateInfo
	AgentID    uuid.UUID
}

// AgentManager handles any agent updates and requests.
type AgentManager interface {
	// RegisterAgent registers a new agent.
	RegisterAgent(info *AgentInfo) (uint32, error)

	// UpdateHeartbeat updates the agent heartbeat with the current time.
	UpdateHeartbeat(agentID uuid.UUID) error

	// UpdateAgent updates agent info, such as schema.
	UpdateAgent(info *AgentInfo) error

	// UpdateAgentState will run through all agents and delete those
	// that are dead.
	UpdateAgentState() error

	// GetActiveAgents gets all of the current active agents.
	GetActiveAgents() ([]AgentInfo, error)

	AddToFrontOfAgentQueue(string, *metadatapb.ResourceUpdate) error
	GetFromAgentQueue(string) ([]*metadatapb.ResourceUpdate, error)

	AddToUpdateQueue(uuid.UUID, *messagespb.AgentUpdateInfo)

	GetMetadataUpdates(hostname string) ([]*metadatapb.ResourceUpdate, error)

	AddUpdatesToAgentQueue(uuid.UUID, []*metadatapb.ResourceUpdate) error
}

// AgentManagerImpl is an implementation for AgentManager which talks to etcd.
type AgentManagerImpl struct {
	IsLeader bool
	client   *clientv3.Client
	clock    utils.Clock
	mds      MetadataStore
	updateCh chan *AgentUpdate
	sess     *concurrency.Session
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(client *clientv3.Client, mds MetadataStore, isLeader bool, clock utils.Clock) *AgentManagerImpl {
	c := make(chan *AgentUpdate)

	sess, err := concurrency.NewSession(client, concurrency.WithContext(context.Background()))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session for etcd")
	}

	agentManager := &AgentManagerImpl{
		client:   client,
		IsLeader: isLeader,
		clock:    clock,
		mds:      mds,
		updateCh: c,
		sess:     sess,
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

		err := m.applyAgentUpdate(msg)
		if err != nil {
			// Add update back to the queue to retry.
			m.updateCh <- msg
		}
	}
}

func (m *AgentManagerImpl) applyAgentUpdate(update *AgentUpdate) error {
	err := m.handleCreatedProcesses(update.UpdateInfo.ProcessCreated)
	if err != nil {
		log.WithError(err).Error("Error when creating new processes")
	}

	err = m.handleTerminatedProcesses(update.UpdateInfo.ProcessTerminated)
	if err != nil {
		log.WithError(err).Error("Error when updating terminated processes")
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
func NewAgentManager(client *clientv3.Client, mds MetadataStore, isLeader bool) *AgentManagerImpl {
	clock := utils.SystemClock{}
	return NewAgentManagerWithClock(client, mds, isLeader, clock)
}

func updateAgentData(agentID uuid.UUID, data *data.AgentData, client *clientv3.Client) error {
	i, err := data.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agentData pb")
	}

	// Update agentData in etcd.
	_, err = client.Put(context.Background(), GetAgentKeyFromUUID(agentID), string(i))
	if err != nil {
		return errors.New("Unable to update etcd")
	}
	return nil
}

// RegisterAgent creates a new agent.
func (m *AgentManagerImpl) RegisterAgent(info *AgentInfo) (asid uint32, err error) {
	if !m.IsLeader { // Only write to etcd if current service is a leader.
		return 0, errors.New("Non-leader can't create agent")
	}
	ctx := context.Background()

	// Check if agent already exists.
	resp, err := m.client.Get(ctx, GetAgentKeyFromUUID(info.AgentID))
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
	} else if len(resp.Kvs) != 0 {
		return 0, errors.New("Agent already exists")
	}

	// Check there's an existing agent for the hostname.
	resp, err = m.client.Get(ctx, GetHostnameAgentKey(info.Hostname))
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
	} else if len(resp.Kvs) != 0 {
		// Another agent already exists for this hostname. Delete it.
		m.deleteAgent(ctx, string(resp.Kvs[0].Value), info.Hostname)
	}

	idPb := utils.ProtoFromUUID(&info.AgentID)
	if err != nil {
		log.WithError(err).Fatal("Failed to convert UUID to pb")
	}
	infoPb := &data.AgentData{
		AgentID: idPb,
		HostInfo: &data.HostInfo{
			Hostname: info.Hostname,
		},
		CreateTimeNS:    m.clock.Now().UnixNano(),
		LastHeartbeatNS: m.clock.Now().UnixNano(),
	}
	i, err := infoPb.Marshal()
	if err != nil {
		return 0, errors.New("Unable to marshal agentData pb")
	}

	mu := concurrency.NewMutex(m.sess, GetUpdateKey())
	mu.Lock(ctx)
	defer mu.Unlock(context.Background())

	hostnameDNE := clientv3util.KeyMissing(GetHostnameAgentKey(info.Hostname))
	createHostname := clientv3.OpPut(GetHostnameAgentKey(info.Hostname), info.AgentID.String())
	createAgent := clientv3.OpPut(GetAgentKeyFromUUID(info.AgentID), string(i))

	_, err = m.client.Txn(ctx).If(hostnameDNE).Then(createHostname, createAgent).Commit()
	if err != nil {
		log.WithError(err).Fatal("Could not update agent data in etcd")
	}

	// Get ASID for the new agent.
	asid, err = m.mds.GetASID()
	if err != nil {
		return 0, err
	}

	return asid, nil
}

// UpdateHeartbeat updates the agent heartbeat with the current time.
func (m *AgentManagerImpl) UpdateHeartbeat(agentID uuid.UUID) error {
	if !m.IsLeader {
		return nil
	}
	ctx := context.Background()

	// Get current AgentData.
	resp, err := m.client.Get(ctx, GetAgentKeyFromUUID(agentID))
	if err != nil {
		log.WithError(err).Fatal("Failed to get existing agentData")
		return err
	}
	if len(resp.Kvs) != 1 {
		return errors.New("Agent does not exist")
	}

	// Update LastHeartbeatNS in AgentData.
	pb := &data.AgentData{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)
	pb.LastHeartbeatNS = m.clock.Now().UnixNano()

	mu := concurrency.NewMutex(m.sess, GetUpdateKey())
	mu.Lock(ctx)
	defer mu.Unlock(context.Background())
	err = updateAgentData(agentID, pb, m.client)
	if err != nil {
		log.WithError(err).Fatal("Could not update agent data in etcd")
	}

	return nil
}

// UpdateAgent updates agent info, such as schema.
func (m *AgentManagerImpl) UpdateAgent(info *AgentInfo) error {
	// TODO(michelle): Implement once we figure out how the agent info (schemas, etc) looks.
	return nil
}

func (m *AgentManagerImpl) deleteAgent(ctx context.Context, agentID string, hostname string) error {
	mu := concurrency.NewMutex(m.sess, GetUpdateKey())
	mu.Lock(ctx)
	defer mu.Unlock(context.Background())

	_, err := m.client.Delete(ctx, GetAgentKey(agentID))
	if err != nil {
		return err
	}
	hostnameAgentMap := clientv3.Compare(clientv3.Value(GetHostnameAgentKey(hostname)), "=", agentID)

	ops := make([]clientv3.Op, 2)
	ops[0] = clientv3.OpDelete(GetHostnameAgentKey(hostname))
	ops[1] = clientv3.OpDelete(GetAgentSchemasKey(agentID), clientv3.WithPrefix())

	_, err = m.client.Txn(ctx).If(hostnameAgentMap).Then(ops...).Commit()

	return nil
}

// UpdateAgentState will run through all agents and delete those
// that are dead.
func (m *AgentManagerImpl) UpdateAgentState() error {
	if !m.IsLeader {
		return nil
	}

	// TODO(michelle): PL-665 Move all etcd-specific functionality into etcd_metadata_store, so that the agent manager itself
	// is not directly interfacing with etcd.
	ctx := context.Background()

	currentTime := m.clock.Now().UnixNano()

	agentPbs, err := m.mds.GetAgents()
	if err != nil {
		return err
	}

	for _, agentPb := range *agentPbs {
		if currentTime-agentPb.LastHeartbeatNS > AgentExpirationTimeout {
			uid, err := utils.UUIDFromProto(agentPb.AgentID)
			if err != nil {
				log.WithError(err).Fatal("Could not convert UUID to proto")
			}
			err = m.deleteAgent(ctx, uid.String(), agentPb.HostInfo.Hostname)
			if err != nil {
				log.WithError(err).Fatal("Failed to delete agent from etcd")
			}
		}
	}

	return nil
}

// GetActiveAgents gets all of the current active agents.
func (m *AgentManagerImpl) GetActiveAgents() ([]AgentInfo, error) {
	var agents []AgentInfo

	agentPbs, err := m.mds.GetAgents()
	if err != nil {
		return agents, err
	}

	for _, agentPb := range *agentPbs {
		uid, err := utils.UUIDFromProto(agentPb.AgentID)
		if err != nil {
			log.WithError(err).Fatal("Could not convert UUID to proto")
		}
		info := &AgentInfo{
			LastHeartbeatNS: agentPb.LastHeartbeatNS,
			CreateTimeNS:    agentPb.CreateTimeNS,
			AgentID:         uid,
			Hostname:        agentPb.HostInfo.Hostname,
		}
		agents = append(agents, *info)
	}

	return agents, nil
}

// AddToFrontOfAgentQueue adds the given value to the front of the agent's update queue.
func (m *AgentManagerImpl) AddToFrontOfAgentQueue(agentID string, value *metadatapb.ResourceUpdate) error {
	return m.mds.AddToFrontOfAgentQueue(agentID, value)
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (m *AgentManagerImpl) GetFromAgentQueue(agentID string) ([]*metadatapb.ResourceUpdate, error) {
	return m.mds.GetFromAgentQueue(agentID)
}

// AddUpdatesToAgentQueue adds the given updates in order to the agent's update queue.
func (m *AgentManagerImpl) AddUpdatesToAgentQueue(agentID uuid.UUID, updates []*metadatapb.ResourceUpdate) error {
	return m.mds.AddUpdatesToAgentQueue(agentID.String(), updates)
}

// GetMetadataUpdates gets all updates from the metadata store.
func (m *AgentManagerImpl) GetMetadataUpdates(hostname string) ([]*metadatapb.ResourceUpdate, error) {
	var updates []*metadatapb.ResourceUpdate

	pods, err := m.mds.GetNodePods(hostname)
	if err != nil {
		return nil, err
	}

	endpoints, err := m.mds.GetNodeEndpoints(hostname)
	if err != nil {
		return nil, err
	}

	for _, pod := range pods {
		containerUpdates := GetContainerResourceUpdatesFromPod(pod)
		updates = append(updates, containerUpdates...)

		podUpdate := GetResourceUpdateFromPod(pod)
		updates = append(updates, podUpdate)
	}

	for _, endpoint := range endpoints {
		epUpdate := GetNodeResourceUpdateFromEndpoints(endpoint, hostname)
		updates = append(updates, epUpdate)
	}

	return updates, nil
}
