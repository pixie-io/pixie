package controllers

import (
	"errors"
	"path"
	"time"

	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// KVMetadataStore stores metadata using keys and values.
type KVMetadataStore struct {
	cache          *kvstore.Cache
	expiryDuration time.Duration
	clusterInfo    ClusterInfo
}

// TODO(michelle): Uncomment this when we remove etcd_metadata_store.go
// // ClusterInfo contains static information about the cluster, stored in memory.
// type ClusterInfo struct {
// 	CIDR string // This is the cluster CIDR block.
// }

// NewKVMetadataStore creates a new key-value metadata store.
func NewKVMetadataStore(cache *kvstore.Cache) (*KVMetadataStore, error) {
	return NewKVMetadataStoreWithExpiryTime(cache, 24*time.Hour)
}

// NewKVMetadataStoreWithExpiryTime creates a new metadata store with the given expiry time. A KVMetadataStore
// stores metadata using keys and values.
func NewKVMetadataStoreWithExpiryTime(cache *kvstore.Cache, expiryDuration time.Duration) (*KVMetadataStore, error) {
	mds := &KVMetadataStore{
		cache:          cache,
		expiryDuration: expiryDuration,
		clusterInfo:    ClusterInfo{CIDR: ""},
	}

	return mds, nil
}

// GetClusterCIDR returns the CIDR for the current cluster.
func (mds *KVMetadataStore) GetClusterCIDR() string {
	return mds.clusterInfo.CIDR
}

// SetClusterInfo sets static information about the current cluster.
func (mds *KVMetadataStore) SetClusterInfo(clusterInfo ClusterInfo) {
	mds.clusterInfo = clusterInfo
}

/* ================= Keys =================*/

// GetAgentKey gets the key for the agent.
func getAgentKey(agentID uuid.UUID) string {
	return path.Join("/", "agent", agentID.String())
}

// GetHostnameAgentKey gets the key for the hostname's agent.
func getHostnameAgentKey(hostname string) string {
	return path.Join("/", "hostname", hostname, "agent")
}

// GetKelvinAgentKey gets the key for a kelvin node.
func getKelvinAgentKey(agentID uuid.UUID) string {
	return path.Join("/", "kelvin", agentID.String())
}

// GetAgentSchemasKey gets all schemas belonging to an agent.
func getAgentSchemasKey(agentID uuid.UUID) string {
	return path.Join("/", "agents", agentID.String(), "schema")
}

/* =============== Agent Operations ============== */

// GetAgent gets the agent info for the agent with the given id.
func (mds *KVMetadataStore) GetAgent(agentID uuid.UUID) (*agentpb.Agent, error) {
	resp, err := mds.cache.Get(getAgentKey(agentID))
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

// GetAgentIDForHostname gets the agent for the given hostname, if it exists.
func (mds *KVMetadataStore) GetAgentIDForHostname(hostname string) (string, error) {
	id, err := mds.cache.Get(getHostnameAgentKey(hostname))
	if err != nil {
		return "", err
	}
	if id == nil {
		return "", nil
	}

	return string(id), err
}

// DeleteAgent deletes the agent with the given ID.
func (mds *KVMetadataStore) DeleteAgent(agentID uuid.UUID) error {
	resp, err := mds.cache.Get(getAgentKey(agentID))
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

	hostname := aPb.Info.HostInfo.Hostname
	delKeys := []string{getAgentKey(agentID), getHostnameAgentKey(hostname)}

	// Info.Capabiltiies should never be nil with our new PEMs/Kelvin. If it is nil,
	// this means that the protobuf we retrieved from etcd belongs to an older agent.
	collectsData := aPb.Info.Capabilities == nil || aPb.Info.Capabilities.CollectsData
	if !collectsData {
		delKeys = append(delKeys, getKelvinAgentKey(agentID))
	}

	mds.cache.DeleteAll(delKeys)

	return mds.cache.DeleteWithPrefix(getAgentSchemasKey(agentID))
}

// CreateAgent creates a new agent.
func (mds *KVMetadataStore) CreateAgent(agentID uuid.UUID, a *agentpb.Agent) error {
	i, err := a.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent protobuf: " + err.Error())
	}

	mds.cache.Set(getHostnameAgentKey(a.Info.HostInfo.Hostname), agentID.String())
	mds.cache.Set(getAgentKey(agentID), string(i))

	collectsData := a.Info.Capabilities == nil || a.Info.Capabilities.CollectsData
	if !collectsData {
		mds.cache.Set(getKelvinAgentKey(agentID), agentID.String())
	}

	return nil
}

// UpdateAgent updates the agent info for the agent with the given ID.
func (mds *KVMetadataStore) UpdateAgent(agentID uuid.UUID, a *agentpb.Agent) error {
	i, err := a.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent protobuf: " + err.Error())
	}

	mds.cache.Set(getAgentKey(agentID), string(i))
	return nil
}
