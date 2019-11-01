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
	"github.com/coreos/etcd/clientv3/concurrency"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	"pixielabs.ai/pixielabs/src/shared/k8s"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
	datapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// EtcdMetadataStore is the implementation of our metadata store in etcd.
type EtcdMetadataStore struct {
	client         *clientv3.Client
	sess           *concurrency.Session
	expiryDuration time.Duration
}

// NewEtcdMetadataStore creates a new etcd metadata store.
func NewEtcdMetadataStore(client *clientv3.Client) (*EtcdMetadataStore, error) {
	return NewEtcdMetadataStoreWithExpiryTime(client, 24*time.Hour)
}

// NewEtcdMetadataStoreWithExpiryTime creates a new etcd metadata store with the given expiry time.
func NewEtcdMetadataStoreWithExpiryTime(client *clientv3.Client, expiryDuration time.Duration) (*EtcdMetadataStore, error) {
	sess, err := concurrency.NewSession(client, concurrency.WithContext(context.Background()))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session for etcd")
	}

	mds := &EtcdMetadataStore{
		client:         client,
		sess:           sess,
		expiryDuration: expiryDuration,
	}

	return mds, nil
}

// UpdateEndpoints adds or updates the given endpoint in the metadata store.
func (mds *EtcdMetadataStore) UpdateEndpoints(e *metadatapb.Endpoints) error {
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
				if address.NodeName == hostname {
					endpoints = append(endpoints, pb)
				}
			}
		}
	}
	return endpoints, nil
}

func getNamespaceFromMetadata(md *metadatapb.ObjectMetadata) string {
	return getNamespaceFromString(md.Namespace)
}

func getNamespaceFromString(ns string) string {
	if ns == "" {
		ns = "default"
	}
	return ns
}

func getEndpointsKey() string {
	return path.Join("/", "endpoints") + "/"
}

func getEndpointKey(e *metadatapb.Endpoints) string {
	return path.Join(getEndpointsKey(), getNamespaceFromMetadata(e.Metadata), e.Metadata.UID)
}

// UpdatePod adds or updates the given pod in the metadata store.
func (mds *EtcdMetadataStore) UpdatePod(p *metadatapb.Pod) error {
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
		if pb.Status.HostIP == ipStr {
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

func getPodsKey() string {
	return path.Join("/", "pod") + "/"
}

func getPodKey(e *metadatapb.Pod) string {
	return getPodKeyFromStrings(e.Metadata.UID, getNamespaceFromMetadata(e.Metadata))
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

// GetAgentSchemaKey gets the etcd key for an agent's schema.
func GetAgentSchemaKey(agentID string, schemaName string) string {
	return path.Join(GetAgentSchemasKey(agentID), schemaName)
}

// GetAgentSchemasKey gets all schemas belonging to an agent.
func GetAgentSchemasKey(agentID string) string {
	return path.Join("/", "agents", agentID, "schema")
}

// UpdateService adds or updates the given service in the metadata store.
func (mds *EtcdMetadataStore) UpdateService(s *metadatapb.Service) error {
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
func (mds *EtcdMetadataStore) UpdateContainersFromPod(pod *metadatapb.Pod) error {
	mu := concurrency.NewMutex(mds.sess, GetUpdateKey())
	mu.Lock(context.Background())
	defer mu.Unlock(context.Background())

	containers := make([]*metadatapb.ContainerStatus, 0)
	for _, status := range pod.Status.ContainerStatuses {
		log.Info(status)
		if status.ContainerID != "" {
			containers = append(containers, status)
		}
	}

	cOps := make([]clientv3.Op, len(containers))
	for i, container := range containers {
		cid := formatContainerID(container.ContainerID)
		key := getContainerKeyFromStrings(cid)
		cInfo := metadatapb.ContainerInfo{
			Name:             container.Name,
			UID:              cid,
			StartTimestampNS: container.StartTimestampNS,
			StopTimestampNS:  container.StopTimestampNS,
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
	mu := concurrency.NewMutex(mds.sess, GetUpdateKey())
	mu.Lock(context.Background())
	defer mu.Unlock(context.Background())

	ops := make([]clientv3.Op, len(schemas))
	computedSchemaOps := make([]clientv3.Op, len(schemas))
	for i, schemaPb := range schemas {
		schema, err := schemaPb.Marshal()
		if err != nil {
			log.WithError(err).Error("Could not marshall schema update message.")
			continue
		}

		schemaKey := GetAgentSchemaKey(agentID.String(), schemaPb.Name)
		ops[i] = clientv3.OpPut(schemaKey, string(schema))

		computedSchemaKey := getComputedSchemaKey(schemaPb.Name)
		computedSchemaOps[i] = clientv3.OpPut(computedSchemaKey, string(schema))
	}

	_, err := mds.client.Txn(context.TODO()).If().Then(ops...).Commit()
	if err != nil {
		return err
	}

	// TODO(michelle): PL-695 This currently assumes that if a schema is available on one agent,
	// then it is available on all agents. This should be updated so that we handle situations where that is not the case.
	_, err = mds.client.Txn(context.TODO()).If().Then(computedSchemaOps...).Commit()

	return err
}

// GetComputedSchemas gets all computed schemas in the metadata store.
func (mds *EtcdMetadataStore) GetComputedSchemas() ([]*metadatapb.SchemaInfo, error) {
	resp, err := mds.client.Get(context.Background(), getComputedSchemasKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Error("Failed to execute etcd Get")
		return nil, err
	}

	schemas := make([]*metadatapb.SchemaInfo, len(resp.Kvs))
	for i, kv := range resp.Kvs {
		pb := &metadatapb.SchemaInfo{}
		proto.Unmarshal(kv.Value, pb)
		schemas[i] = pb
	}
	return schemas, nil
}

func getComputedSchemaKey(schemaName string) string {
	return path.Join(getComputedSchemasKey(), schemaName)
}

func getComputedSchemasKey() string {
	return path.Join("/", "schema", "computed")
}

func getServicesKey() string {
	return path.Join("/", "service") + "/"
}

func getServiceKey(e *metadatapb.Service) string {
	return path.Join("/", "service", getNamespaceFromMetadata(e.Metadata), e.Metadata.UID)
}

func getServicePodMapKey(e *metadatapb.Endpoints) string {
	return path.Join("/", "services", getNamespaceFromMetadata(e.Metadata), e.Metadata.Name, "pods")
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

// GetUpdateKey gets the etcd key that the service should grab a lock on before updating any values in etcd.
func GetUpdateKey() string {
	return "/updateKey"
}

func getAsidKey() string {
	return "/asid"
}

func getProcessKey(upid string) string {
	return path.Join("/", "processes", upid)
}

func (mds *EtcdMetadataStore) updateValue(key string, value string, expire bool) error {
	mu := concurrency.NewMutex(mds.sess, GetUpdateKey())
	mu.Lock(context.Background())
	defer mu.Unlock(context.Background())

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

func getAgentUpdateKey(agentID string) string {
	return path.Join("/", "agents", agentID, "updates")
}

// GetAgentsForHostnames gets the agents running on the given hostnames.
func (mds *EtcdMetadataStore) GetAgentsForHostnames(hostnames *[]string) (*[]string, error) {
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

	var agents []string
	for _, resp := range txnresp.Responses {
		if len(resp.GetResponseRange().Kvs) > 0 {
			agents = append(agents, string(resp.GetResponseRange().Kvs[0].Value))
		}
	}

	return &agents, nil
}

// AddToAgentUpdateQueue adds the given value to the agent's update queue.
func (mds *EtcdMetadataStore) AddToAgentUpdateQueue(agentID string, value string) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID), mds.sess, GetUpdateKey())

	return q.Enqueue(value)
}

// AddToFrontOfAgentQueue adds the given value to the front of the agent's update queue.
func (mds *EtcdMetadataStore) AddToFrontOfAgentQueue(agentID string, value *metadatapb.ResourceUpdate) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID), mds.sess, GetUpdateKey())

	i, err := value.Marshal()
	if err != nil {
		log.WithError(err).Fatal("Unable to marshal update pb.")
	}

	return q.EnqueueAtFront(string(i))
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (mds *EtcdMetadataStore) GetFromAgentQueue(agentID string) ([]*metadatapb.ResourceUpdate, error) {
	var pbs []*metadatapb.ResourceUpdate

	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID), mds.sess, GetUpdateKey())
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
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID), mds.sess, GetUpdateKey())

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
func (mds *EtcdMetadataStore) GetAgents() (*[]datapb.AgentData, error) {
	var agents []datapb.AgentData

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

		pb := &datapb.AgentData{}
		proto.Unmarshal(kv.Value, pb)
		agents = append(agents, *pb)
	}

	return &agents, nil
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
	mu := concurrency.NewMutex(mds.sess, GetUpdateKey())
	mu.Lock(context.Background())
	defer mu.Unlock(context.Background())

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

func getIPMapKey(ip string) string {
	return path.Join("/", "ip", ip, "hostname")
}

// UpdateIPMap updates mapping from IP to hostname in the metadata store.
func (mds *EtcdMetadataStore) UpdateIPMap(ip string, hostname string) error {
	key := getIPMapKey(ip)

	return mds.updateValue(key, hostname, false /*expire*/)
}

// Close cleans up the etcd metadata store, such as its etcd session.
func (mds *EtcdMetadataStore) Close() {
	mds.sess.Close()
}
