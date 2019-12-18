package controllers

import (
	"context"
	"errors"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/clientv3util"
	"github.com/gogo/protobuf/proto"

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

	// UpdateAgentState will run through all agents and delete those
	// that are dead.
	UpdateAgentState() error

	// GetActiveAgents gets all of the current active agents.
	GetActiveAgents() ([]*agentpb.Agent, error)

	AddToFrontOfAgentQueue(string, *metadatapb.ResourceUpdate) error
	GetFromAgentQueue(string) ([]*metadatapb.ResourceUpdate, error)

	AddToUpdateQueue(uuid.UUID, *messagespb.AgentUpdateInfo)

	GetMetadataUpdates(hostname string) ([]*metadatapb.ResourceUpdate, error)

	AddUpdatesToAgentQueue(uuid.UUID, []*metadatapb.ResourceUpdate) error
}

// AgentManagerImpl is an implementation for AgentManager which talks to etcd.
type AgentManagerImpl struct {
	client   *clientv3.Client
	clock    utils.Clock
	mds      MetadataStore
	updateCh chan *AgentUpdate
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(client *clientv3.Client, mds MetadataStore, clock utils.Clock) *AgentManagerImpl {
	c := make(chan *AgentUpdate)

	agentManager := &AgentManagerImpl{
		client:   client,
		clock:    clock,
		mds:      mds,
		updateCh: c,
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
func NewAgentManager(client *clientv3.Client, mds MetadataStore) *AgentManagerImpl {
	clock := utils.SystemClock{}
	return NewAgentManagerWithClock(client, mds, clock)
}

func updateAgentData(agentID uuid.UUID, data *agentpb.Agent, client *clientv3.Client) error {
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
func (m *AgentManagerImpl) RegisterAgent(agent *agentpb.Agent) (asid uint32, err error) {
	ctx := context.Background()
	info := agent.Info

	// Check if agent already exists.
	aUUID := utils.UUIDFromProtoOrNil(info.AgentID)
	resp, err := m.client.Get(ctx, GetAgentKeyFromUUID(aUUID))
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
	} else if len(resp.Kvs) != 0 {
		return 0, errors.New("Agent already exists")
	}

	collectsData := info.Capabilities == nil || info.Capabilities.CollectsData

	// Check there's an existing agent for the hostname.
	resp, err = m.client.Get(ctx, GetHostnameAgentKey(info.HostInfo.Hostname))
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
	} else if len(resp.Kvs) != 0 {
		// Another agent already exists for this hostname. Delete it.
		m.deleteAgent(ctx, string(resp.Kvs[0].Value), info.HostInfo.Hostname, collectsData)
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
	i, err := infoPb.Marshal()
	if err != nil {
		return 0, errors.New("Unable to marshal agentData pb")
	}

	hostnameDNE := clientv3util.KeyMissing(GetHostnameAgentKey(info.HostInfo.Hostname))

	ops := make([]clientv3.Op, 2)
	ops[0] = clientv3.OpPut(GetHostnameAgentKey(info.HostInfo.Hostname), aUUID.String())
	ops[1] = clientv3.OpPut(GetAgentKeyFromUUID(aUUID), string(i))

	if !collectsData {
		ops = append(ops, clientv3.OpPut(GetKelvinAgentKey(aUUID.String()), aUUID.String()))
	}

	_, err = m.client.Txn(ctx).If(hostnameDNE).Then(ops...).Commit()
	if err != nil {
		log.WithError(err).Fatal("Could not update agent data in etcd")
	}

	return asid, nil
}

// UpdateHeartbeat updates the agent heartbeat with the current time.
func (m *AgentManagerImpl) UpdateHeartbeat(agentID uuid.UUID) error {
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
	pb := &agentpb.Agent{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)
	pb.LastHeartbeatNS = m.clock.Now().UnixNano()

	err = updateAgentData(agentID, pb, m.client)
	if err != nil {
		log.WithError(err).Fatal("Could not update agent data in etcd")
	}

	return nil
}

// UpdateAgent updates agent info, such as schema.
func (m *AgentManagerImpl) UpdateAgent(info *agentpb.Agent) error {
	// TODO(michelle): Implement once we figure out how the agent info (schemas, etc) looks.
	return nil
}

func (m *AgentManagerImpl) deleteAgent(ctx context.Context, agentID string, hostname string, collectsData bool) error {
	_, err := m.client.Delete(ctx, GetAgentKey(agentID))
	if err != nil {
		return err
	}
	hostnameAgentMap := clientv3.Compare(clientv3.Value(GetHostnameAgentKey(hostname)), "=", agentID)

	ops := make([]clientv3.Op, 2)
	ops[0] = clientv3.OpDelete(GetHostnameAgentKey(hostname))
	ops[1] = clientv3.OpDelete(GetAgentSchemasKey(agentID), clientv3.WithPrefix())

	if !collectsData {
		ops = append(ops, clientv3.OpDelete(GetKelvinAgentKey(agentID)))
	}

	_, err = m.client.Txn(ctx).If(hostnameAgentMap).Then(ops...).Commit()

	return nil
}

// UpdateAgentState will run through all agents and delete those
// that are dead.
func (m *AgentManagerImpl) UpdateAgentState() error {
	// TODO(michelle): PL-665 Move all etcd-specific functionality into etcd_metadata_store, so that the agent manager itself
	// is not directly interfacing with etcd.
	ctx := context.Background()

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
			collectsData := agentPb.Info.Capabilities == nil || agentPb.Info.Capabilities.CollectsData
			err = m.deleteAgent(ctx, uid.String(), agentPb.Info.HostInfo.Hostname, collectsData)
			if err != nil {
				log.WithError(err).Fatal("Failed to delete agent from etcd")
			}
		}
	}

	return nil
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

// GetMetadataUpdates gets all updates from the metadata store. If no hostname is specified, it fetches all updates
// regardless of hostname.
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
