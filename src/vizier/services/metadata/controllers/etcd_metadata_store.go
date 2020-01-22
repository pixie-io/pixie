package controllers

import (
	"context"
	"errors"
	"fmt"
	"net"
	"path"
	"strconv"
	"strings"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// EtcdMetadataStore is the implementation of our metadata store in etcd.
type EtcdMetadataStore struct {
	client         *clientv3.Client
	expiryDuration time.Duration
	clusterInfo    ClusterInfo
}

// ClusterInfo contains static information about the cluster.
type ClusterInfo struct {
	CIDR string
}

// NewEtcdMetadataStore creates a new etcd metadata store.
func NewEtcdMetadataStore(client *clientv3.Client) (*EtcdMetadataStore, error) {
	return NewEtcdMetadataStoreWithExpiryTime(client, 24*time.Hour)
}

// NewEtcdMetadataStoreWithExpiryTime creates a new etcd metadata store with the given expiry time.
func NewEtcdMetadataStoreWithExpiryTime(client *clientv3.Client, expiryDuration time.Duration) (*EtcdMetadataStore, error) {
	mds := &EtcdMetadataStore{
		client:         client,
		expiryDuration: expiryDuration,
		clusterInfo:    ClusterInfo{CIDR: ""},
	}

	return mds, nil
}

// GetClusterCIDR returns the CIDR for the current cluster.
func (mds *EtcdMetadataStore) GetClusterCIDR() string {
	return mds.clusterInfo.CIDR
}

// SetClusterInfo sets static information about the current cluster.
func (mds *EtcdMetadataStore) SetClusterInfo(clusterInfo ClusterInfo) {
	mds.clusterInfo = clusterInfo
}

// UpdateEndpoints adds or updates the given endpoint in the metadata store.
func (mds *EtcdMetadataStore) UpdateEndpoints(e *metadatapb.Endpoints, deleted bool) error {
	if deleted && e.Metadata.DeletionTimestampNS == 0 {
		e.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := e.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getEndpointKey(e)

	err = mds.updateValue(key, string(val), e.Metadata.DeletionTimestampNS > 0)
	if err != nil {
		return err
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

	return mds.updateValue(mapKey, strings.Join(podIds, ","), e.Metadata.DeletionTimestampNS > 0)
}

// GetEndpoints gets all endpoints in the metadata store.
func (mds *EtcdMetadataStore) GetEndpoints() ([]*metadatapb.Endpoints, error) {
	resp, err := mds.client.Get(context.Background(), getEndpointsKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	endpoints := make([]*metadatapb.Endpoints, len(resp.Kvs))
	for i, kv := range resp.Kvs {
		pb := &metadatapb.Endpoints{}
		proto.Unmarshal(kv.Value, pb)
		endpoints[i] = pb
	}
	return endpoints, nil
}

// GetNodeEndpoints gets all endpoints in the metadata store that belong to a particular hostname.
func (mds *EtcdMetadataStore) GetNodeEndpoints(hostname string) ([]*metadatapb.Endpoints, error) {
	resp, err := mds.client.Get(context.Background(), getEndpointsKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	var endpoints []*metadatapb.Endpoints
	for _, kv := range resp.Kvs {
		pb := &metadatapb.Endpoints{}
		proto.Unmarshal(kv.Value, pb)

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

// UpdatePod adds or updates the given pod in the metadata store.
func (mds *EtcdMetadataStore) UpdatePod(p *metadatapb.Pod, deleted bool) error {
	if deleted && p.Metadata.DeletionTimestampNS == 0 {
		p.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := p.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getPodKey(p)

	return mds.updateValue(key, string(val), p.Metadata.DeletionTimestampNS > 0)
}

// GetPods gets all pods in the metadata store.
func (mds *EtcdMetadataStore) GetPods() ([]*metadatapb.Pod, error) {
	resp, err := mds.client.Get(context.Background(), getPodsKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	pods := make([]*metadatapb.Pod, len(resp.Kvs))
	for i, kv := range resp.Kvs {
		pb := &metadatapb.Pod{}
		proto.Unmarshal(kv.Value, pb)
		pods[i] = pb
	}
	return pods, nil
}

// GetNodePods gets all pods belonging to a node in the metadata store.
func (mds *EtcdMetadataStore) GetNodePods(hostname string) ([]*metadatapb.Pod, error) {
	resp, err := mds.client.Get(context.Background(), getPodsKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	ip, _ := net.LookupIP(hostname)
	ipStr := ""

	if len(ip) > 0 {
		ipStr = ip[0].String()
	}

	var pods []*metadatapb.Pod
	for _, kv := range resp.Kvs {
		pb := &metadatapb.Pod{}
		proto.Unmarshal(kv.Value, pb)
		if (pb.Status.HostIP == ipStr || hostname == "") && pb.Metadata.DeletionTimestampNS == 0 {
			pods = append(pods, pb)
		}
	}
	return pods, nil
}

// GetContainers gets all containers in the metadata store.
func (mds *EtcdMetadataStore) GetContainers() ([]*metadatapb.ContainerInfo, error) {
	resp, err := mds.client.Get(context.Background(), getContainersKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	containers := make([]*metadatapb.ContainerInfo, len(resp.Kvs))
	for i, kv := range resp.Kvs {
		pb := &metadatapb.ContainerInfo{}
		proto.Unmarshal(kv.Value, pb)
		containers[i] = pb
	}
	return containers, nil
}

// GetServices gets all services in the metadata store.
func (mds *EtcdMetadataStore) GetServices() ([]*metadatapb.Service, error) {
	resp, err := mds.client.Get(context.Background(), getServicesKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	services := make([]*metadatapb.Service, len(resp.Kvs))
	for i, kv := range resp.Kvs {
		pb := &metadatapb.Service{}
		proto.Unmarshal(kv.Value, pb)
		services[i] = pb
	}
	return services, nil
}

// GetAgentSchemaKey gets the etcd key for an agent's schema.
func GetAgentSchemaKey(agentID string, schemaName string) string {
	return path.Join(GetAgentSchemasKey(agentID), schemaName)
}

// GetAgentSchemasKey gets all schemas belonging to an agent.
func GetAgentSchemasKey(agentID string) string {
	return path.Join("/", "agents", agentID, "schema")
}

// UpdateService adds or updates the given service in the metadata store.
func (mds *EtcdMetadataStore) UpdateService(s *metadatapb.Service, deleted bool) error {
	if deleted && s.Metadata.DeletionTimestampNS == 0 {
		s.Metadata.DeletionTimestampNS = time.Now().UnixNano()
	}

	val, err := s.Marshal()
	if err != nil {
		return errors.New("Unable to marshal endpoints pb")
	}

	key := getServiceKey(s)

	return mds.updateValue(key, string(val), s.Metadata.DeletionTimestampNS > 0)
}

// UpdateContainer adds or updates the given container in the metadata store.
func (mds *EtcdMetadataStore) UpdateContainer(c *metadatapb.ContainerInfo) error {
	val, err := c.Marshal()
	if err != nil {
		return errors.New("Unable to marshal containerInfo pb")
	}

	key := getContainerKey(c)

	return mds.updateValue(key, string(val), c.StopTimestampNS > 0)
}

// UpdateContainersFromPod updates the containers from the given pod in the metadata store.
func (mds *EtcdMetadataStore) UpdateContainersFromPod(pod *metadatapb.Pod, deleted bool) error {
	containers := make([]*metadatapb.ContainerStatus, 0)
	for _, status := range pod.Status.ContainerStatuses {
		if status.ContainerID != "" {
			containers = append(containers, status)
		}
	}

	cOps := make([]clientv3.Op, len(containers))
	for i, container := range containers {
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

		leaseID := clientv3.NoLease
		if container.StopTimestampNS > 0 {
			resp, err := mds.client.Grant(context.TODO(), int64(mds.expiryDuration.Seconds()))
			if err != nil {
				return errors.New("Could not get grant")
			}
			leaseID = resp.ID
		}

		cOps[i] = clientv3.OpPut(key, string(val), clientv3.WithLease(leaseID))
	}

	_, err := mds.client.Txn(context.TODO()).If().Then(cOps...).Commit()

	return err
}

// UpdateSchemas updates the given schemas in the metadata store.
func (mds *EtcdMetadataStore) UpdateSchemas(agentID uuid.UUID, schemas []*metadatapb.SchemaInfo) error {
	ops := make([]clientv3.Op, len(schemas))

	computedSchemaPb := metadatapb.ComputedSchema{
		Tables: schemas,
	}
	for i, schemaPb := range schemas {
		schema, err := schemaPb.Marshal()
		if err != nil {
			log.WithError(err).Error("Could not marshal schema update message.")
			continue
		}

		schemaKey := GetAgentSchemaKey(agentID.String(), schemaPb.Name)
		ops[i] = clientv3.OpPut(schemaKey, string(schema))
	}

	_, err := mds.client.Txn(context.TODO()).If().Then(ops...).Commit()
	if err != nil {
		return err
	}

	computedSchema, err := computedSchemaPb.Marshal()
	if err != nil {
		log.WithError(err).Error("Could not computed schema update message.")
		return err
	}

	computedSchemaOp := clientv3.OpPut(getComputedSchemasKey(), string(computedSchema))
	// TODO(michelle): PL-695 This currently assumes that if a schema is available on one agent,
	// then it is available on all agents. This should be updated so that we handle situations where that is not the case.
	_, err = mds.client.Txn(context.TODO()).If().Then(computedSchemaOp).Commit()

	return err
}

// GetComputedSchemas gets all computed schemas in the metadata store.
func (mds *EtcdMetadataStore) GetComputedSchemas() ([]*metadatapb.SchemaInfo, error) {
	resp, err := mds.client.Get(context.Background(), getComputedSchemasKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	if len(resp.Kvs) != 1 {
		return nil, fmt.Errorf("resp should be 1, received %v", len(resp.Kvs))
	}
	computedSchemaPb := &metadatapb.ComputedSchema{}
	err = proto.Unmarshal(resp.Kvs[0].Value, computedSchemaPb)
	if err != nil {
		return nil, err
	}

	return computedSchemaPb.Tables, nil
}

func getServiceKey(e *metadatapb.Service) string {
	return path.Join("/", "service", getNamespaceFromMetadata(e.Metadata), e.Metadata.UID)
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

// GetKelvinAgentKey gets the etcd key for a kelvin node.
func GetKelvinAgentKey(agentID string) string {
	return "/kelvin/" + agentID
}

// GetUpdateKey gets the etcd key that the service should grab a lock on before updating any values in etcd.
func GetUpdateKey() string {
	return "/updateKey"
}

func getAgentUpdateKeyFromString(agentID string) string {
	return path.Join("/", "agents", agentID, "updates")
}

func (mds *EtcdMetadataStore) updateValue(key string, value string, expire bool) error {
	leaseID := clientv3.NoLease
	if expire {
		resp, err := mds.client.Grant(context.TODO(), int64(mds.expiryDuration.Seconds()))
		if err != nil {
			return errors.New("Could not get grant")
		}
		leaseID = resp.ID
	}

	_, err := mds.client.Put(context.Background(), key, value, clientv3.WithLease(leaseID))
	if err != nil {
		return errors.New("Unable to update etcd")
	}

	return nil
}

// GetAgentsForHostnames gets the agents running on the given hostnames.
func (mds *EtcdMetadataStore) GetAgentsForHostnames(hostnames *[]string) (*[]string, error) {
	agents := []string{}
	if len(*hostnames) == 0 {
		return &agents, nil
	}

	cmps := make([]clientv3.Cmp, len(*hostnames))
	ops := make([]clientv3.Op, len(*hostnames))
	for i, hostname := range *hostnames {
		cmps[i] = clientv3.Compare(clientv3.CreateRevision(GetHostnameAgentKey(hostname)), "=", 0)
		ops[i] = clientv3.OpGet(GetHostnameAgentKey(hostname))
	}

	txnresp, err := mds.client.Txn(context.TODO()).If(cmps...).Then(ops...).Else(ops...).Commit()
	if err != nil {
		return nil, err
	}

	for _, resp := range txnresp.Responses {
		if len(resp.GetResponseRange().Kvs) > 0 {
			agents = append(agents, string(resp.GetResponseRange().Kvs[0].Value))
		}
	}

	return &agents, nil
}

// AddToAgentUpdateQueue adds the given value to the agent's update queue.
func (mds *EtcdMetadataStore) AddToAgentUpdateQueue(agentID string, value string) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKeyFromString(agentID))

	return q.Enqueue(value)
}

// AddToFrontOfAgentQueue adds the given value to the front of the agent's update queue.
func (mds *EtcdMetadataStore) AddToFrontOfAgentQueue(agentID string, value *metadatapb.ResourceUpdate) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKeyFromString(agentID))

	i, err := value.Marshal()
	if err != nil {
		log.WithError(err).Fatal("Unable to marshal update pb.")
	}

	return q.EnqueueAtFront(string(i))
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (mds *EtcdMetadataStore) GetFromAgentQueue(agentID string) ([]*metadatapb.ResourceUpdate, error) {
	var pbs []*metadatapb.ResourceUpdate

	q := etcd.NewQueue(mds.client, getAgentUpdateKeyFromString(agentID))
	resp, err := q.DequeueAll()
	if err != nil {
		return pbs, err
	}

	// Convert strings to pbs.
	for _, update := range *resp {
		updatePb := metadatapb.ResourceUpdate{}
		if err := proto.Unmarshal([]byte(update), &updatePb); err != nil {
			// For whatever reason, the updatePb was invalid and could not be parsed. Realistically, this
			// should never happen unless there are some corrupt values in etcd. Continue so that the agent
			// atleast gets the other updates.
			// TODO(michelle): PL-664: The agent may get out of sync. Think about how we can sync the agent + metadata service
			// so that even if they do get out of sync, we can recover.
			log.WithError(err).Error("Could not unmarshal resource update pb.")
			continue
		}
		pbs = append(pbs, &updatePb)
	}

	return pbs, nil
}

// AddUpdatesToAgentQueue adds all updates to the agent's queue in etcd.
func (mds *EtcdMetadataStore) AddUpdatesToAgentQueue(agentID string, updates []*metadatapb.ResourceUpdate) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKeyFromString(agentID))

	var updateStrs []string
	for _, update := range updates {
		updateBytes, err := update.Marshal()
		if err != nil {
			log.WithError(err).Fatal("Unable to marshal update pb.")
		} else {
			updateStrs = append(updateStrs, string(updateBytes))
		}
	}
	return q.EnqueueAll(updateStrs)
}

// GetAgents gets all of the current active agents.
func (mds *EtcdMetadataStore) GetAgents() ([]*agentpb.Agent, error) {
	var agents []*agentpb.Agent

	// Get all agents.
	resp, err := mds.client.Get(context.Background(), GetAgentKey(""), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}
	for _, kv := range resp.Kvs {
		// Filter out keys that aren't of the form /agent/<uuid>.
		splitKey := strings.Split(string(kv.Key), "/")
		if len(splitKey) != 3 {
			continue
		}

		pb := &agentpb.Agent{}
		proto.Unmarshal(kv.Value, pb)
		if len(pb.Info.AgentID.Data) > 0 {
			agents = append(agents, pb)
		}
	}

	return agents, nil
}

// GetKelvinIDs gets the IDs of the current active kelvins.
func (mds *EtcdMetadataStore) GetKelvinIDs() ([]string, error) {
	var ids []string

	// Get all kelvins.
	resp, err := mds.client.Get(context.Background(), GetKelvinAgentKey(""), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}
	for _, kv := range resp.Kvs {
		ids = append(ids, string(kv.Value))
	}

	return ids, nil
}

// GetASID gets the next assignable ASID.
func (mds *EtcdMetadataStore) GetASID() (uint32, error) {
	asid := "1" // Starting ASID.

	resp, err := mds.client.Get(context.Background(), getAsidKey())

	if err != nil {
		return 0, err
	} else if len(resp.Kvs) != 0 {
		asid = string(resp.Kvs[0].Value)
	}

	// Convert ASID from etcd into uint32.
	asidInt, err := strconv.ParseUint(asid, 10, 32)
	if err != nil {
		return 0, err
	}

	// Increment ASID in etcd.
	updatedAsid := asidInt + 1

	_, err = mds.client.Put(context.Background(), getAsidKey(), fmt.Sprint(updatedAsid))
	if err != nil {
		return 0, errors.New("Unable to update asid in etcd")
	}

	return uint32(asidInt), nil
}

// GetProcesses gets the process infos for the given process upids.
func (mds *EtcdMetadataStore) GetProcesses(upids []*types.UInt128) ([]*metadatapb.ProcessInfo, error) {
	getOps := make([]clientv3.Op, len(upids))
	for i, upid := range upids {
		getOps[i] = clientv3.OpGet(getProcessKey(k8s.StringFromUPID(upid)))
	}

	txnresp, err := mds.client.Txn(context.TODO()).If().Then(getOps...).Commit()
	if err != nil {
		return nil, err
	}

	var processes []*metadatapb.ProcessInfo
	for _, resp := range txnresp.Responses {
		if len(resp.GetResponseRange().Kvs) > 0 {
			processPb := metadatapb.ProcessInfo{}
			if err := proto.Unmarshal([]byte(resp.GetResponseRange().Kvs[0].Value), &processPb); err != nil {
				log.WithError(err).Error("Could not unmarshal process pb.")
				processes = append(processes, nil)
				continue
			}
			processes = append(processes, &processPb)
		} else {
			processes = append(processes, nil)
		}
	}
	return processes, nil
}

// UpdateProcesses updates the given processes in the metadata store.
func (mds *EtcdMetadataStore) UpdateProcesses(processes []*metadatapb.ProcessInfo) error {
	ops := make([]clientv3.Op, len(processes))

	for i, processPb := range processes {
		process, err := processPb.Marshal()
		if err != nil {
			log.WithError(err).Error("Could not marshall processInfo.")
			continue
		}
		upid := types.UInt128FromProto(processPb.UPID)

		processKey := getProcessKey(k8s.StringFromUPID(upid))

		leaseID := clientv3.NoLease
		if processPb.StopTimestampNS > 0 {
			resp, err := mds.client.Grant(context.TODO(), int64(mds.expiryDuration.Seconds()))
			if err != nil {
				return errors.New("Could not get grant")
			}
			leaseID = resp.ID
		}

		ops[i] = clientv3.OpPut(processKey, string(process), clientv3.WithLease(leaseID))
	}

	_, err := mds.client.Txn(context.TODO()).If().Then(ops...).Commit()
	return err
}

// Close cleans up the etcd metadata store, such as its etcd session.
func (mds *EtcdMetadataStore) Close() {
}
