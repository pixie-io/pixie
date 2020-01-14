package controllers

import (
	"errors"
	"fmt"
	"net"
	"path"
	"strconv"
	"strings"
	"time"

	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
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

func getAsidKey() string {
	return "/asid"
}

func getAgentKeyPrefix() string {
	return "/agent/"
}

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

func getAgentSchemaKey(agentID uuid.UUID, schemaName string) string {
	return path.Join(getAgentSchemasKey(agentID), schemaName)
}

func getComputedSchemasKey() string {
	return path.Join("/", "computedSchema")
}

func getComputedSchemaKey(schemaName string) string {
	return path.Join(getComputedSchemasKey(), schemaName)
}

func getProcessKey(upid string) string {
	return path.Join("/", "processes", upid)
}

func getPodsKey() string {
	return path.Join("/", "pod") + "/"
}

func getEndpointsKey() string {
	return path.Join("/", "endpoints") + "/"
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

// GetAgents gets all of the current active agents.
func (mds *KVMetadataStore) GetAgents() ([]*agentpb.Agent, error) {
	var agents []*agentpb.Agent

	keys, vals, err := mds.cache.GetWithPrefix(getAgentKeyPrefix())
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
		proto.Unmarshal(vals[i], pb)
		if len(pb.Info.AgentID.Data) > 0 {
			agents = append(agents, pb)
		}
	}

	return agents, nil
}

// GetASID gets the next assignable ASID.
func (mds *KVMetadataStore) GetASID() (uint32, error) {
	asid := "1" // Starting ASID.

	resp, err := mds.cache.Get(getAsidKey())
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
	mds.cache.Set(getAsidKey(), fmt.Sprint(updatedAsid))

	return uint32(asidInt), nil
}

/* =============== Schema Operations ============== */

// UpdateSchemas updates the given schemas in the metadata store.
func (mds *KVMetadataStore) UpdateSchemas(agentID uuid.UUID, schemas []*metadatapb.SchemaInfo) error {
	computedSchemaPb := metadatapb.ComputedSchema{
		Tables: schemas,
	}
	computedSchema, err := computedSchemaPb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not computed schema update message.")
		return err
	}

	for _, schemaPb := range schemas {
		schema, err := schemaPb.Marshal()
		if err != nil {
			log.WithError(err).Error("Could not marshall schema update message.")
			continue
		}
		mds.cache.Set(getAgentSchemaKey(agentID, schemaPb.Name), string(schema))
	}
	// TODO(michelle): PL-695 This currently assumes that if a schema is available on one agent,
	// then it is available on all agents. This should be updated so that we handle situations where that is not the case.
	mds.cache.Set(getComputedSchemasKey(), string(computedSchema))

	return nil
}

// GetComputedSchemas gets all computed schemas in the metadata store.
func (mds *KVMetadataStore) GetComputedSchemas() ([]*metadatapb.SchemaInfo, error) {
	cSchemas, err := mds.cache.Get(getComputedSchemasKey())
	if err != nil {
		return nil, err
	}
	if cSchemas == nil {
		return nil, fmt.Errorf("Could not find any computed schemas")
	}

	computedSchemaPb := &metadatapb.ComputedSchema{}
	err = proto.Unmarshal(cSchemas, computedSchemaPb)
	if err != nil {
		return nil, err
	}

	return computedSchemaPb.Tables, nil
}

/* =============== Process Operations ============== */

// UpdateProcesses updates the given processes in the metadata store.
func (mds *KVMetadataStore) UpdateProcesses(processes []*metadatapb.ProcessInfo) error {
	for _, processPb := range processes {
		process, err := processPb.Marshal()
		if err != nil {
			log.WithError(err).Error("Could not marshall processInfo.")
			continue
		}
		upid := types.UInt128FromProto(processPb.UPID)
		processKey := getProcessKey(k8s.StringFromUPID(upid))

		if processPb.StopTimestampNS > 0 {
			mds.cache.SetWithTTL(processKey, string(process), mds.expiryDuration)
		} else {
			mds.cache.Set(processKey, string(process))
		}
	}
	return nil
}

// GetProcesses gets the process infos for the given process upids.
func (mds *KVMetadataStore) GetProcesses(upids []*types.UInt128) ([]*metadatapb.ProcessInfo, error) {
	processes := make([]*metadatapb.ProcessInfo, len(upids))

	for i, upid := range upids {
		process, err := mds.cache.Get(getProcessKey(k8s.StringFromUPID(upid)))
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

/* =============== Pod Operations ============== */

// GetNodePods gets all pods belonging to a node in the metadata store.
func (mds *KVMetadataStore) GetNodePods(hostname string) ([]*metadatapb.Pod, error) {
	_, vals, err := mds.cache.GetWithPrefix(getPodsKey())
	if err != nil {
		return nil, err
	}

	ip, _ := net.LookupIP(hostname)
	ipStr := ""

	if len(ip) > 0 {
		ipStr = ip[0].String()
	}

	var pods []*metadatapb.Pod
	for _, val := range vals {
		pb := &metadatapb.Pod{}
		proto.Unmarshal(val, pb)
		if (pb.Status.HostIP == ipStr || hostname == "") && pb.Metadata.DeletionTimestampNS == 0 {
			pods = append(pods, pb)
		}
	}
	return pods, nil
}

/* =============== Endpoints Operations ============== */

// GetNodeEndpoints gets all endpoints in the metadata store that belong to a particular hostname.
func (mds *KVMetadataStore) GetNodeEndpoints(hostname string) ([]*metadatapb.Endpoints, error) {
	_, vals, err := mds.cache.GetWithPrefix(getEndpointsKey())
	if err != nil {
		return nil, err
	}

	var endpoints []*metadatapb.Endpoints
	for _, val := range vals {
		pb := &metadatapb.Endpoints{}
		proto.Unmarshal(val, pb)
		for _, subset := range pb.Subsets {
			for _, address := range subset.Addresses {
				if (address.NodeName == hostname || hostname == "") && pb.Metadata.DeletionTimestampNS == 0 {
					endpoints = append(endpoints, pb)
				}
			}
		}
	}
	return endpoints, nil
}
