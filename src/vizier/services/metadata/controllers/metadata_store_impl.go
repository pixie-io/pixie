package controllers

import (
	"errors"
	"fmt"
	"net"
	"path"
	"strconv"
	"strings"
	"time"

	"github.com/EvilSuperstars/go-cidrman"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	"pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	types "pixielabs.ai/pixielabs/src/shared/types/go"
	"pixielabs.ai/pixielabs/src/utils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// KVMetadataStore stores metadata using keys and values.
type KVMetadataStore struct {
	cache          *kvstore.Cache
	expiryDuration time.Duration
	clusterInfo    ClusterInfo
}

// ClusterInfo contains static information about the cluster, stored in memory.
type ClusterInfo struct {
	ServiceCIDR *net.IPNet // This is the service CIDR block; it is inferred from all observed service IPs.
	PodCIDRs    []string   // The pod CIDRs in the cluster, inferred from each node's reported pod CIDR.
}

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
		clusterInfo:    ClusterInfo{ServiceCIDR: nil, PodCIDRs: make([]string, 0)},
	}

	return mds, nil
}

// HostnameIPPair is a unique identifies for a K8s node.
type HostnameIPPair struct {
	Hostname string
	IP       string
}

// GetServiceCIDR returns the service CIDR for the current cluster.
func (mds *KVMetadataStore) GetServiceCIDR() string {
	if mds.clusterInfo.ServiceCIDR == nil {
		return ""
	}
	return mds.clusterInfo.ServiceCIDR.String()
}

// UpdatePodCIDR updates list of pod CIDRs.
func (mds *KVMetadataStore) UpdatePodCIDR(cidrs []string) error {
	mergedCIDRs, err := cidrman.MergeCIDRs(append(mds.clusterInfo.PodCIDRs, cidrs...))
	if err != nil {
		return err
	}
	mds.clusterInfo.PodCIDRs = mergedCIDRs
	return nil
}

// GetPodCIDRs returns the PodCIDRs for the cluster.
func (mds *KVMetadataStore) GetPodCIDRs() []string {
	return mds.clusterInfo.PodCIDRs
}

/* ================= Keys =================*/

func getNamespaceFromMetadata(md *metadatapb.ObjectMetadata) string {
	return getNamespaceFromString(md.Namespace)
}

func getNamespaceFromString(ns string) string {
	if ns == "" {
		ns = "default"
	}
	return ns
}

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

func getPodNameToAgentIDKey(podName string) string {
	return path.Join("/", "podToAgentID", podName)
}

// GetHostnamePairAgentKey gets the key for the hostname pair's agent.
func GetHostnamePairAgentKey(pair *HostnameIPPair) string {
	return path.Join("/", "hostnameIP", fmt.Sprintf("%s-%s", pair.Hostname, pair.IP), "agent")
}

// GetKelvinAgentKey gets the key for a kelvin node.
func getKelvinAgentKeyPrefix() string {
	return "/kelvin/"
}

// GetKelvinAgentKey gets the key for a kelvin node.
func getKelvinAgentKey(agentID uuid.UUID) string {
	return path.Join("/", "kelvin", agentID.String())
}

func getAgentDataInfoPrefix() string {
	return path.Join("/", "agentDataInfo")
}

func getAgentDataInfoKey(agentID uuid.UUID) string {
	return path.Join(getAgentDataInfoPrefix(), agentID.String())
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

func getPodKey(e *metadatapb.Pod) string {
	return getPodKeyFromStrings(e.Metadata.UID, getNamespaceFromMetadata(e.Metadata))
}

func getPodToHostnamePairKey(podName string, namespace string) string {
	return path.Join("/podHostnamePair/", fmt.Sprintf("%s-%s", namespace, podName))
}

func getPodKeyFromStrings(uid string, namespace string) string {
	return path.Join("/", "pod", namespace, uid)
}

func getContainersKey() string {
	return path.Join("/", "containers") + "/"
}

func getContainerKey(c *metadatapb.ContainerInfo) string {
	return getContainerKeyFromStrings(c.UID)
}

func getContainerKeyFromStrings(containerID string) string {
	return path.Join("/", "containers", containerID, "info")
}

func getEndpointsKey() string {
	return path.Join("/", "endpoints") + "/"
}

func getEndpointKey(e *metadatapb.Endpoints) string {
	return path.Join(getEndpointsKey(), getNamespaceFromMetadata(e.Metadata), e.Metadata.UID)
}

func getServicePodMapKey(e *metadatapb.Endpoints) string {
	return path.Join("/", "services", getNamespaceFromMetadata(e.Metadata), e.Metadata.Name, "pods")
}

func getServicesKey() string {
	return path.Join("/", "service") + "/"
}

func getServiceKey(e *metadatapb.Service) string {
	return path.Join("/", "service", getNamespaceFromMetadata(e.Metadata), e.Metadata.UID)
}

func getNamespacesKey() string {
	return path.Join("/", "namespace") + "/"
}

func getNamespaceKey(e *metadatapb.Namespace) string {
	return path.Join("/", "namespace", e.Metadata.UID)
}

func getResourceVersionMapKey(rv string) string {
	// The rv may end in "_#", for containers which share the same rv as pods.
	// This needs to be removed from the string that is about to be padded, and reappended after padding.
	splitRV := strings.Split(rv, "_")
	// Pad the rv so that ranges work as expected.
	paddedRV := fmt.Sprintf("%020s", splitRV[0])
	if len(splitRV) > 1 {
		// Reappend the suffix, if any.
		paddedRV = fmt.Sprintf("%s_%s", paddedRV, splitRV[1])
	}

	return path.Join("/", "paddedRVUpdate", paddedRV)
}

func getSubscriberResourceVersionKey(sub string) string {
	return path.Join("/", "subscriber", "resourceVersion", sub)
}

func getNodesKey() string {
	return path.Join("/", "node") + "/"
}

func getNodeKey(n *metadatapb.Node) string {
	return path.Join(getNodesKey(), n.Metadata.UID)
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

// GetKelvinIDs gets the IDs of the current active kelvins.
func (mds *KVMetadataStore) GetKelvinIDs() ([]string, error) {
	var ids []string

	// Get all kelvins.
	_, vals, err := mds.cache.GetWithPrefix(getKelvinAgentKeyPrefix())
	if err != nil {
		return nil, err
	}
	for _, v := range vals {
		ids = append(ids, string(v))
	}

	return ids, nil
}

// GetAgentIDFromPodName gets the agent ID for the agent with the given name.
func (mds *KVMetadataStore) GetAgentIDFromPodName(podName string) (string, error) {
	id, err := mds.cache.Get(getPodNameToAgentIDKey(podName))
	if err != nil {
		return "", err
	}

	if id == nil {
		return "", nil
	}

	return string(id), nil
}

// GetAgentIDForHostnamePair gets the agent for the given hostnamePair, if it exists.
func (mds *KVMetadataStore) GetAgentIDForHostnamePair(hnPair *HostnameIPPair) (string, error) {
	id, err := mds.cache.Get(GetHostnamePairAgentKey(hnPair))
	if err != nil {
		return "", err
	}
	if id == nil {
		return "", nil
	}

	return string(id), err
}

// GetAgentsForHostnamePairs gets the agents running on the given hostnames.
func (mds *KVMetadataStore) GetAgentsForHostnamePairs(hostnames *[]*HostnameIPPair) ([]string, error) {
	if len(*hostnames) == 0 {
		return nil, nil
	}

	agents := []string{}
	for _, hn := range *hostnames {
		if hn == nil {
			continue
		}
		resp, err := mds.cache.Get(GetHostnamePairAgentKey(hn))
		if err != nil {
			continue
		}
		if resp == nil {
			continue
		}
		agents = append(agents, string(resp))
	}

	return agents, nil
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

	hostname := ""
	if !aPb.Info.Capabilities.CollectsData {
		hostname = aPb.Info.HostInfo.Hostname
	}

	hnPair := &HostnameIPPair{
		Hostname: hostname,
		IP:       aPb.Info.HostInfo.HostIP,
	}
	delKeys := []string{getAgentKey(agentID), GetHostnamePairAgentKey(hnPair), getPodNameToAgentIDKey(aPb.Info.HostInfo.PodName)}

	// Info.Capabiltiies should never be nil with our new PEMs/Kelvin. If it is nil,
	// this means that the protobuf we retrieved from etcd belongs to an older agent.
	collectsData := aPb.Info.Capabilities == nil || aPb.Info.Capabilities.CollectsData
	if !collectsData {
		delKeys = append(delKeys, getKelvinAgentKey(agentID))
	}

	mds.cache.DeleteAll(delKeys)

	// Deletes from the computedSchema
	err = mds.UpdateSchemas(agentID, []*storepb.TableInfo{})
	if err != nil {
		return err
	}

	return mds.cache.DeleteWithPrefix(getAgentDataInfoKey(agentID))
}

// CreateAgent creates a new agent.
func (mds *KVMetadataStore) CreateAgent(agentID uuid.UUID, a *agentpb.Agent) error {
	i, err := a.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent protobuf: " + err.Error())
	}
	hostname := ""
	if !a.Info.Capabilities.CollectsData {
		hostname = a.Info.HostInfo.Hostname
	}
	hnPair := &HostnameIPPair{
		Hostname: hostname,
		IP:       a.Info.HostInfo.HostIP,
	}

	mds.cache.Set(GetHostnamePairAgentKey(hnPair), agentID.String())
	mds.cache.Set(getAgentKey(agentID), string(i))
	mds.cache.Set(getPodNameToAgentIDKey(a.Info.HostInfo.PodName), agentID.String())

	collectsData := a.Info.Capabilities == nil || a.Info.Capabilities.CollectsData
	if !collectsData {
		mds.cache.Set(getKelvinAgentKey(agentID), agentID.String())
	}

	log.WithField("hostname", hnPair.Hostname).WithField("HostIP", hnPair.IP).Info("Registering agent")
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
		if pb.Info != nil && len(pb.Info.AgentID.Data) > 0 {
			agents = append(agents, pb)
		}
	}

	return agents, nil
}

// GetAgentsDataInfo returns all of the information about data tables that each agent has.
func (mds *KVMetadataStore) GetAgentsDataInfo() (map[uuid.UUID]*messagespb.AgentDataInfo, error) {
	dataInfos := make(map[uuid.UUID]*messagespb.AgentDataInfo)

	keys, vals, err := mds.cache.GetWithPrefix(getAgentDataInfoPrefix())
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

// GetAgentDataInfo returns the data info for a particular agent.
func (mds *KVMetadataStore) GetAgentDataInfo(agentID uuid.UUID) (*messagespb.AgentDataInfo, error) {
	dataInfoStr, err := mds.cache.Get(getAgentDataInfoKey(agentID))
	if err != nil {
		return nil, err
	}

	pb := &messagespb.AgentDataInfo{}
	err = proto.Unmarshal(dataInfoStr, pb)
	if err != nil {
		return nil, err
	}
	return pb, nil
}

// UpdateAgentDataInfo updates the information about data tables that a particular agent has.
func (mds *KVMetadataStore) UpdateAgentDataInfo(agentID uuid.UUID, dataInfo *messagespb.AgentDataInfo) error {
	i, err := dataInfo.Marshal()
	if err != nil {
		return errors.New("Unable to marshal agent data info protobuf: " + err.Error())
	}

	mds.cache.Set(getAgentDataInfoKey(agentID), string(i))
	return nil
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

var errNoComputedSchemas = errors.New("Could not find any computed schemas")

// GetComputedSchema returns the raw CombinedComputedSchema.
func (mds *KVMetadataStore) GetComputedSchema() (*storepb.ComputedSchema, error) {
	cSchemas, err := mds.cache.Get(getComputedSchemasKey())
	if err != nil {
		return nil, err
	}
	if cSchemas == nil {
		return nil, errNoComputedSchemas
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
			return fmt.Errorf("Agent marked for deletion not found in list of agents for %s", agentIDPb, tableName)
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

func initializeComputedSchema(computedSchemaPb *storepb.ComputedSchema) *storepb.ComputedSchema {
	if computedSchemaPb == nil {
		computedSchemaPb = &storepb.ComputedSchema{}
	}
	if computedSchemaPb.Tables == nil {
		computedSchemaPb.Tables = make([]*storepb.TableInfo, 0)
	}
	if computedSchemaPb.TableNameToAgentIDs == nil {
		computedSchemaPb.TableNameToAgentIDs = make(map[string]*storepb.ComputedSchema_AgentIDs)
	}
	return computedSchemaPb
}

// UpdateSchemas updates the given schemas in the metadata store.
func (mds *KVMetadataStore) UpdateSchemas(agentID uuid.UUID, schemas []*storepb.TableInfo) error {
	computedSchemaPb, err := mds.GetComputedSchema()
	// If there are no computed schemas, that means we have yet to set one.
	if err == errNoComputedSchemas {
		// Reset error as this is not actually an error.
		err = nil
	}

	// Other errors are still errors.
	if err != nil {
		log.WithError(err).Error("Could not get old schema.")
		return err
	}

	// Make sure the computedSchema is non-nil and fields are non-nil.
	computedSchemaPb = initializeComputedSchema(computedSchemaPb)

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
		for _, a := range agents.AgentID {
			if a.Equal(agentIDPb) {
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

	mds.cache.Set(getComputedSchemasKey(), string(computedSchema))

	return nil
}

// PruneComputedSchema cleans any dead agents from the computed schema. This is a temporary fix, to address a larger
// consistency and race-condition problem that will be addressed by the upcoming extensive refactor of the metadata service.
func (mds *KVMetadataStore) PruneComputedSchema() error {
	// Fetch all existing agents.
	agents, err := mds.GetAgents()
	if err != nil {
		return err
	}
	// Make a map from the agent IDs.
	existingAgents := make(map[uuid.UUID]bool)
	for _, agent := range agents {
		existingAgents[utils.UUIDFromProtoOrNil(agent.Info.AgentID)] = true
	}

	// Fetch current computed schema.
	computedSchemaPb, err := mds.GetComputedSchema()
	// If there are no computed schemas, that means we don't have to do anything.
	if err == errNoComputedSchemas || computedSchemaPb == nil {
		return nil
	}
	if err != nil {
		return err
	}

	computedSchemaPb = initializeComputedSchema(computedSchemaPb)

	tableInfos := make([]*storepb.TableInfo, 0)
	tableToAgents := make(map[string]*storepb.ComputedSchema_AgentIDs)

	existingTables := make(map[string]bool)

	// Filter out any dead agents from the table -> agent mapping.
	for tableName, agentIDs := range computedSchemaPb.TableNameToAgentIDs {
		prunedIDs := []*uuidpb.UUID{}
		for i, agentID := range agentIDs.AgentID {
			if _, ok := existingAgents[utils.UUIDFromProtoOrNil(agentID)]; ok {
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
		if _, ok := existingTables[table.Name]; ok {
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

	mds.cache.Set(getComputedSchemasKey(), string(computedSchema))

	return nil
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
func (mds *KVMetadataStore) GetNodePods(hnPair *HostnameIPPair) ([]*metadatapb.Pod, error) {
	_, vals, err := mds.cache.GetWithPrefix(getPodsKey())
	if err != nil {
		return nil, err
	}

	var pods []*metadatapb.Pod
	for _, val := range vals {
		pb := &metadatapb.Pod{}
		proto.Unmarshal(val, pb)
		if pb.Status.HostIP == "" {
			log.WithField("pod_name", pb.Metadata.Name).Info("Pod has no hostIP")
		}
		if (hnPair == nil || pb.Status.HostIP == hnPair.IP) && pb.Metadata.DeletionTimestampNS == 0 {
			pods = append(pods, pb)
		}
	}
	return pods, nil
}

// GetPods gets all pods in the metadata store.
func (mds *KVMetadataStore) GetPods() ([]*metadatapb.Pod, error) {
	_, vals, err := mds.cache.GetWithPrefix(getPodsKey())
	if err != nil {
		return nil, err
	}

	pods := make([]*metadatapb.Pod, len(vals))
	for i, val := range vals {
		pb := &metadatapb.Pod{}
		proto.Unmarshal(val, pb)
		pods[i] = pb
	}
	return pods, nil
}

// UpdatePod adds or updates the given pod in the metadata store.
func (mds *KVMetadataStore) UpdatePod(p *metadatapb.Pod, deleted bool) error {
	cidrs := []string{p.Status.PodIP + "/32"}

	if deleted && p.Metadata.DeletionTimestampNS == 0 {
		p.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := p.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getPodKey(p)

	if p.Metadata.DeletionTimestampNS > 0 {
		mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
	} else {
		mds.cache.Set(key, string(val))
	}

	mds.cache.Set(getPodToHostnamePairKey(p.Metadata.Name, p.Metadata.Namespace), fmt.Sprintf("%s:%s", "", p.Status.HostIP))

	if p.Status.PodIP != "" {
		err = mds.UpdatePodCIDR(cidrs)
		if err != nil {
			log.WithField("cidrs", cidrs).WithError(err).Error("Error updating Pod CIDRs")
		}
	}

	// Add mapping from resource version -> pod.
	rvUpdate := &metadatapb.MetadataObject{
		Object: &metadatapb.MetadataObject_Pod{
			Pod: p,
		},
	}
	val, err = rvUpdate.Marshal()
	if err != nil {
		return errors.New("Unable to marshal rv pb")
	}
	mds.cache.Set(getResourceVersionMapKey(p.Metadata.ResourceVersion), string(val))

	return nil
}

// GetHostnameIPPairFromPodName gets the hostname IP pair from a given pod name.
func (mds *KVMetadataStore) GetHostnameIPPairFromPodName(podName string, namespace string) (*HostnameIPPair, error) {
	resp, err := mds.cache.Get(getPodToHostnamePairKey(podName, namespace))
	if err != nil {
		return nil, err
	}
	if resp == nil {
		return nil, nil
	}
	splitVal := strings.Split(string(resp), ":")
	if len(splitVal) != 2 {
		return nil, errors.New("malformed hostname IP pair")
	}
	return &HostnameIPPair{
		splitVal[0], splitVal[1]}, nil
}

/* =============== Container Operations ============== */

// GetContainers gets all containers in the metadata store.
func (mds *KVMetadataStore) GetContainers() ([]*metadatapb.ContainerInfo, error) {
	_, vals, err := mds.cache.GetWithPrefix(getContainersKey())
	if err != nil {
		return nil, err
	}

	containers := make([]*metadatapb.ContainerInfo, len(vals))
	for i, val := range vals {
		pb := &metadatapb.ContainerInfo{}
		proto.Unmarshal(val, pb)
		containers[i] = pb
	}
	return containers, nil
}

// UpdateContainer adds or updates the given container in the metadata store.
func (mds *KVMetadataStore) UpdateContainer(c *metadatapb.ContainerInfo) error {
	val, err := c.Marshal()
	if err != nil {
		return errors.New("Unable to marshal containerInfo pb")
	}

	key := getContainerKey(c)

	if c.StopTimestampNS > 0 {
		mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
	} else {
		mds.cache.Set(key, string(val))
	}
	return nil
}

// UpdateContainersFromPod updates the containers from the given pod in the metadata store.
func (mds *KVMetadataStore) UpdateContainersFromPod(pod *metadatapb.Pod, deleted bool) error {
	containers := make([]*metadatapb.ContainerStatus, 0)
	for _, status := range pod.Status.ContainerStatuses {
		if status.ContainerID != "" {
			containers = append(containers, status)
		}
	}

	for _, container := range containers {
		cid := formatContainerID(container.ContainerID)
		key := getContainerKeyFromStrings(cid)

		stopTime := container.StopTimestampNS
		if deleted && stopTime == 0 {
			stopTime = time.Now().UnixNano()
		}

		cInfo := metadatapb.ContainerInfo{
			Name:             container.Name,
			UID:              cid,
			StartTimestampNS: container.StartTimestampNS,
			StopTimestampNS:  stopTime,
			PodUID:           pod.Metadata.UID,
			Namespace:        getNamespaceFromMetadata(pod.Metadata),
		}
		val, err := cInfo.Marshal()
		if err != nil {
			return errors.New("Unable to marshal containerInfo pb")
		}

		if container.StopTimestampNS > 0 {
			mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
		} else {
			mds.cache.Set(key, string(val))
		}
	}

	return nil
}

/* =============== Endpoints Operations ============== */

// GetNodeEndpoints gets all endpoints in the metadata store that belong to a particular hostname.
func (mds *KVMetadataStore) GetNodeEndpoints(hnPair *HostnameIPPair) ([]*metadatapb.Endpoints, error) {
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
				if address.TargetRef == nil || address.TargetRef.Kind != "Pod" {
					continue
				}
				podPair, err := mds.GetHostnameIPPairFromPodName(address.TargetRef.Name, address.TargetRef.Namespace)
				if err != nil || podPair == nil {
					continue
				}
				if (hnPair == nil || (podPair.IP == hnPair.IP)) && pb.Metadata.DeletionTimestampNS == 0 {
					endpoints = append(endpoints, pb)
				}
			}
		}
	}
	return endpoints, nil
}

// GetEndpoints gets all endpoints in the metadata store.
func (mds *KVMetadataStore) GetEndpoints() ([]*metadatapb.Endpoints, error) {
	_, vals, err := mds.cache.GetWithPrefix(getEndpointsKey())
	if err != nil {
		return nil, err
	}

	endpoints := make([]*metadatapb.Endpoints, len(vals))
	for i, val := range vals {
		pb := &metadatapb.Endpoints{}
		proto.Unmarshal(val, pb)
		endpoints[i] = pb
	}
	return endpoints, nil
}

// UpdateEndpoints adds or updates the given endpoint in the metadata store.
func (mds *KVMetadataStore) UpdateEndpoints(e *metadatapb.Endpoints, deleted bool) error {
	if deleted && e.Metadata.DeletionTimestampNS == 0 {
		e.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := e.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getEndpointKey(e)

	// Update endpoints object.
	if e.Metadata.DeletionTimestampNS > 0 {
		mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
	} else {
		mds.cache.Set(key, string(val))
	}

	// Update service -> pod map.
	mapKey := getServicePodMapKey(e)
	var podIds []string
	for _, subset := range e.Subsets {
		for _, addr := range subset.Addresses {
			if addr.TargetRef != nil && addr.TargetRef.Kind == "Pod" {
				podIds = append(podIds, addr.TargetRef.UID)
			}
		}
	}
	mapVal := strings.Join(podIds, ",")

	if e.Metadata.DeletionTimestampNS > 0 {
		mds.cache.SetWithTTL(mapKey, mapVal, mds.expiryDuration)
	} else {
		mds.cache.Set(mapKey, mapVal)
	}

	// Add mapping from resource version -> endpoint.
	rvUpdate := &metadatapb.MetadataObject{
		Object: &metadatapb.MetadataObject_Endpoints{
			Endpoints: e,
		},
	}
	val, err = rvUpdate.Marshal()
	if err != nil {
		return errors.New("Unable to marshal rv pb")
	}
	mds.cache.Set(getResourceVersionMapKey(e.Metadata.ResourceVersion), string(val))

	return nil
}

/* =============== Service Operations ============== */

// GetServices gets all services in the metadata store.
func (mds *KVMetadataStore) GetServices() ([]*metadatapb.Service, error) {
	_, vals, err := mds.cache.GetWithPrefix(getServicesKey())
	if err != nil {
		return nil, err
	}

	services := make([]*metadatapb.Service, len(vals))
	for i, val := range vals {
		pb := &metadatapb.Service{}
		proto.Unmarshal(val, pb)
		services[i] = pb
	}
	return services, nil
}

// UpdateServiceCIDR updates the current best guess of the service CIDR.
func (mds *KVMetadataStore) UpdateServiceCIDR(s *metadatapb.Service) {
	ip := net.ParseIP(s.Spec.ClusterIP).To16()

	// Some services don't have a ClusterIP.
	if ip == nil {
		return
	}

	if mds.clusterInfo.ServiceCIDR == nil {
		mds.clusterInfo.ServiceCIDR = &net.IPNet{IP: ip, Mask: net.CIDRMask(128, 128)}
	} else {
		for !mds.clusterInfo.ServiceCIDR.Contains(ip) {
			ones, bits := mds.clusterInfo.ServiceCIDR.Mask.Size()
			mds.clusterInfo.ServiceCIDR.Mask = net.CIDRMask(ones-1, bits)
			mds.clusterInfo.ServiceCIDR.IP = mds.clusterInfo.ServiceCIDR.IP.Mask(mds.clusterInfo.ServiceCIDR.Mask)
		}
	}
	log.Debug("Service IP: " + ip.String() + " -> Service CIDR updated to: " + mds.clusterInfo.ServiceCIDR.String())
}

// UpdateService adds or updates the given service in the metadata store.
func (mds *KVMetadataStore) UpdateService(s *metadatapb.Service, deleted bool) error {
	mds.UpdateServiceCIDR(s)

	if deleted && s.Metadata.DeletionTimestampNS == 0 {
		s.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := s.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getServiceKey(s)

	if s.Metadata.DeletionTimestampNS > 0 {
		mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
	} else {
		mds.cache.Set(key, string(val))
	}

	// Add mapping from resource version -> service.
	rvUpdate := &metadatapb.MetadataObject{
		Object: &metadatapb.MetadataObject_Service{
			Service: s,
		},
	}
	val, err = rvUpdate.Marshal()
	if err != nil {
		return errors.New("Unable to marshal rv pb")
	}
	mds.cache.Set(getResourceVersionMapKey(s.Metadata.ResourceVersion), string(val))

	return nil
}

/* =============== Namespace Operations ============== */

// GetNamespaces gets all namespaces in the metadata store.
func (mds *KVMetadataStore) GetNamespaces() ([]*metadatapb.Namespace, error) {
	_, vals, err := mds.cache.GetWithPrefix(getNamespacesKey())
	if err != nil {
		return nil, err
	}

	namespaces := make([]*metadatapb.Namespace, len(vals))
	for i, val := range vals {
		pb := &metadatapb.Namespace{}
		proto.Unmarshal(val, pb)
		namespaces[i] = pb
	}
	return namespaces, nil
}

// UpdateNamespace adds or updates the given namespace in the metadata store.
func (mds *KVMetadataStore) UpdateNamespace(s *metadatapb.Namespace, deleted bool) error {
	if deleted && s.Metadata.DeletionTimestampNS == 0 {
		s.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := s.Marshal()
	if err != nil {
		return errors.New("Unable to marshal namespace pb")
	}

	key := getNamespaceKey(s)

	if s.Metadata.DeletionTimestampNS > 0 {
		mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
	} else {
		mds.cache.Set(key, string(val))
	}

	// Add mapping from resource version -> namespace.
	rvUpdate := &metadatapb.MetadataObject{
		Object: &metadatapb.MetadataObject_Namespace{
			Namespace: s,
		},
	}
	val, err = rvUpdate.Marshal()
	if err != nil {
		return errors.New("Unable to marshal rv pb")
	}
	mds.cache.Set(getResourceVersionMapKey(s.Metadata.ResourceVersion), string(val))

	return nil
}

/* =============== Node Operations ============== */

// GetNodes gets all nodes in the metadata store.
func (mds *KVMetadataStore) GetNodes() ([]*metadatapb.Node, error) {
	_, vals, err := mds.cache.GetWithPrefix(getNodesKey())
	if err != nil {
		return nil, err
	}

	namespaces := make([]*metadatapb.Node, len(vals))
	for i, val := range vals {
		pb := &metadatapb.Node{}
		proto.Unmarshal(val, pb)
		namespaces[i] = pb
	}
	return namespaces, nil
}

// UpdateNode adds or updates the given node in the metadata store.
func (mds *KVMetadataStore) UpdateNode(s *metadatapb.Node, deleted bool) error {
	if deleted && s.Metadata.DeletionTimestampNS == 0 {
		s.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := s.Marshal()
	if err != nil {
		return errors.New("Unable to marshal node pb")
	}

	key := getNodeKey(s)

	if s.Metadata.DeletionTimestampNS > 0 {
		mds.cache.SetWithTTL(key, string(val), mds.expiryDuration)
	} else {
		mds.cache.Set(key, string(val))
	}

	// Add mapping from resource version -> namespace.
	rvUpdate := &metadatapb.MetadataObject{
		Object: &metadatapb.MetadataObject_Node{
			Node: s,
		},
	}
	val, err = rvUpdate.Marshal()
	if err != nil {
		return errors.New("Unable to marshal rv pb")
	}
	mds.cache.Set(getResourceVersionMapKey(s.Metadata.ResourceVersion), string(val))

	return nil
}

/* =============== Resource Versions ============== */

// AddResourceVersion creates a mapping from a resourceVersion to the update for that resource.
func (mds *KVMetadataStore) AddResourceVersion(rv string, update *metadatapb.MetadataObject) error {
	val, err := update.Marshal()
	if err != nil {
		return err
	}

	mds.cache.Set(getResourceVersionMapKey(rv), string(val))
	return nil
}

// UpdateSubscriberResourceVersion updates the last resource version processed by a subscriber.
func (mds *KVMetadataStore) UpdateSubscriberResourceVersion(sub string, rv string) error {
	mds.cache.Set(getSubscriberResourceVersionKey(sub), rv)
	return nil
}

// GetSubscriberResourceVersion gets the last resource version processed by a subscriber.
func (mds *KVMetadataStore) GetSubscriberResourceVersion(sub string) (string, error) {
	resp, err := mds.cache.Get(getSubscriberResourceVersionKey(sub))
	if err != nil {
		return "", err
	}
	if resp == nil {
		return "", nil
	}

	return string(resp), nil
}
