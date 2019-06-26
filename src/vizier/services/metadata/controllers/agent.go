package controllers

import (
	"context"
	"errors"
	"strings"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/clientv3util"
	"github.com/coreos/etcd/clientv3/concurrency"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	data "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// AgentExpirationTimeout is the amount of time that we should wait to receive a heartbeat
// from an agent before marking it as unhealthy.
const AgentExpirationTimeout int64 = 1E9 * 60 // 60 seconds in nano-seconds.

// AgentInfo describes information about an agent.
type AgentInfo struct {
	// Unix time of the last heart beat (current system clock)
	LastHeartbeatNS int64
	CreateTimeNS    int64
	AgentID         uuid.UUID
	Hostname        string
}

// AgentManager handles any agent updates and requests.
type AgentManager interface {
	// CreateAgent creates a new agent.
	CreateAgent(info *AgentInfo) error

	// UpdateHeartbeat updates the agent heartbeat with the current time.
	UpdateHeartbeat(agentID uuid.UUID) error

	// UpdateAgent updates agent info, such as schema.
	UpdateAgent(info *AgentInfo) error

	// UpdateAgentState will run through all agents and delete those
	// that are dead.
	UpdateAgentState() error

	// GetActiveAgents gets all of the current active agents.
	GetActiveAgents() ([]AgentInfo, error)

	AddToFrontOfAgentQueue(string, *messagespb.MetadataUpdateInfo_ResourceUpdate) error
	GetFromAgentQueue(string) (*[]messagespb.MetadataUpdateInfo_ResourceUpdate, error)
}

// AgentManagerImpl is an implementation for AgentManager which talks to etcd.
type AgentManagerImpl struct {
	IsLeader bool
	client   *clientv3.Client
	clock    utils.Clock
	mds      MetadataStore
}

// NewAgentManagerWithClock creates a new agent manager with a clock.
func NewAgentManagerWithClock(client *clientv3.Client, mds MetadataStore, isLeader bool, clock utils.Clock) *AgentManagerImpl {
	agentManager := &AgentManagerImpl{
		client:   client,
		IsLeader: isLeader,
		clock:    clock,
		mds:      mds,
	}

	return agentManager
}

// NewAgentManager creates a new agent manager.
func NewAgentManager(client *clientv3.Client, mds MetadataStore, isLeader bool) *AgentManagerImpl {
	clock := utils.SystemClock{}
	return NewAgentManagerWithClock(client, mds, isLeader, clock)
}

// GetAgentKeyFromUUID gets the etcd key for the agent, given the id in a UUID format.
func GetAgentKeyFromUUID(agentID uuid.UUID) string {
	return GetAgentKey(agentID.String())
}

// GetAgentKey gets the etcd key for the agent, given the id in a string format.
func GetAgentKey(agentID string) string {
	return "/agent/" + agentID
}

// GetHostnameAgentKey gets the etcd key for the hostname's agent.
func GetHostnameAgentKey(hostname string) string {
	return "/hostname/" + hostname + "/agent"
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

// CreateAgent creates a new agent.
func (m *AgentManagerImpl) CreateAgent(info *AgentInfo) error {
	if !m.IsLeader { // Only write to etcd if current service is a leader.
		return nil
	}
	ctx := context.Background()
	ss, err := concurrency.NewSession(m.client, concurrency.WithContext(ctx))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session")
	}

	defer ss.Close()

	// Check if agent already exists.
	resp, err := m.client.Get(ctx, GetAgentKeyFromUUID(info.AgentID))
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
	} else if len(resp.Kvs) != 0 {
		return errors.New("Agent already exists")
	}

	// Check there's an existing agent for the hostname.
	resp, err = m.client.Get(ctx, GetHostnameAgentKey(info.Hostname))
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
	} else if len(resp.Kvs) != 0 {
		// Another agent already exists for this hostname. Delete it.
		m.deleteAgent(ctx, string(resp.Kvs[0].Value), info.Hostname, ss)
	}

	idPb, err := utils.ProtoFromUUID(&info.AgentID)
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
		return errors.New("Unable to marshal agentData pb")
	}

	mu := concurrency.NewMutex(ss, GetAgentKeyFromUUID(info.AgentID))
	mu.Lock(ctx)
	defer mu.Unlock(ctx)

	hostnameDNE := clientv3util.KeyMissing(GetHostnameAgentKey(info.Hostname))
	createHostname := clientv3.OpPut(GetHostnameAgentKey(info.Hostname), info.AgentID.String())
	createAgent := clientv3.OpPut(GetAgentKeyFromUUID(info.AgentID), string(i))

	_, err = m.client.Txn(ctx).If(hostnameDNE).Then(createHostname, createAgent).Commit()
	if err != nil {
		log.WithError(err).Fatal("Could not update agent data in etcd")
	}

	return nil
}

// UpdateHeartbeat updates the agent heartbeat with the current time.
func (m *AgentManagerImpl) UpdateHeartbeat(agentID uuid.UUID) error {
	if !m.IsLeader {
		return nil
	}
	ctx := context.Background()
	ss, err := concurrency.NewSession(m.client, concurrency.WithContext(ctx))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session")
	}
	defer ss.Close()

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

	mu := concurrency.NewMutex(ss, GetAgentKeyFromUUID(agentID))
	mu.Lock(ctx)
	defer mu.Unlock(ctx)
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

func (m *AgentManagerImpl) deleteAgent(ctx context.Context, agentID string, hostname string, ss *concurrency.Session) error {
	mu := concurrency.NewMutex(ss, GetAgentKey(agentID))
	mu.Lock(ctx)

	defer mu.Unlock(ctx)

	_, err := m.client.Delete(ctx, GetAgentKey(agentID))
	if err != nil {
		return err
	}
	hostnameAgentMap := clientv3.Compare(clientv3.Value(GetHostnameAgentKey(hostname)), "=", agentID)
	delHostname := clientv3.OpDelete(GetHostnameAgentKey(hostname))

	_, err = m.client.Txn(ctx).If(hostnameAgentMap).Then(delHostname).Commit()

	return nil
}

// UpdateAgentState will run through all agents and delete those
// that are dead.
func (m *AgentManagerImpl) UpdateAgentState() error {
	if !m.IsLeader {
		return nil
	}
	ctx := context.Background()
	ss, err := concurrency.NewSession(m.client, concurrency.WithContext(ctx))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session")
	}
	defer ss.Close()

	currentTime := m.clock.Now().UnixNano()

	// Get all agents.
	resp, err := m.client.Get(ctx, GetAgentKey(""), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
		return err
	}
	// Loop through all the agents, and then remove agents that are too old.
	for _, kv := range resp.Kvs {
		pb := &data.AgentData{}
		proto.Unmarshal(kv.Value, pb)

		if currentTime-pb.LastHeartbeatNS > AgentExpirationTimeout {
			uid, err := utils.UUIDFromProto(pb.AgentID)
			if err != nil {
				log.WithError(err).Fatal("Could not convert UUID to proto")
			}
			err = m.deleteAgent(ctx, uid.String(), pb.HostInfo.Hostname, ss)
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
	ctx := context.Background()

	// Get all agents.
	resp, err := m.client.Get(ctx, GetAgentKey(""), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
		return agents, err
	}
	for _, kv := range resp.Kvs {

		// Filter out keys that aren't of the form /agent/<uuid>.
		splitKey := strings.Split(string(kv.Key), "/")
		if len(splitKey) != 3 {
			continue
		}

		pb := &data.AgentData{}
		proto.Unmarshal(kv.Value, pb)

		uid, err := utils.UUIDFromProto(pb.AgentID)
		if err != nil {
			log.WithError(err).Fatal("Could not convert UUID to proto")
		}
		info := &AgentInfo{
			LastHeartbeatNS: pb.LastHeartbeatNS,
			CreateTimeNS:    pb.CreateTimeNS,
			AgentID:         uid,
			Hostname:        pb.HostInfo.Hostname,
		}
		agents = append(agents, *info)
	}

	return agents, nil
}

// AddToFrontOfAgentQueue adds the given value to the front of the agent's update queue.
func (m *AgentManagerImpl) AddToFrontOfAgentQueue(agentID string, value *messagespb.MetadataUpdateInfo_ResourceUpdate) error {
	return m.mds.AddToFrontOfAgentQueue(agentID, value)
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (m *AgentManagerImpl) GetFromAgentQueue(agentID string) (*[]messagespb.MetadataUpdateInfo_ResourceUpdate, error) {
	return m.mds.GetFromAgentQueue(agentID)
}
