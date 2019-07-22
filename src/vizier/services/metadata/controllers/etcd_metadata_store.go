package controllers

import (
	"context"
	"errors"
	"fmt"
	"path"
	"strconv"
	"strings"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/concurrency"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
	datapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// EtcdMetadataStore is the implementation of our metadata store in etcd.
type EtcdMetadataStore struct {
	client *clientv3.Client
	sess   *concurrency.Session
}

// NewEtcdMetadataStore creates a new etcd metadata store.
func NewEtcdMetadataStore(client *clientv3.Client) (*EtcdMetadataStore, error) {
	sess, err := concurrency.NewSession(client, concurrency.WithContext(context.Background()))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session for etcd")
	}

	mds := &EtcdMetadataStore{
		client: client,
		sess:   sess,
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

	err = mds.updateValue(key, string(val))
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

	return mds.updateValue(mapKey, strings.Join(podIds, ","))
}

// GetEndpoints gets all endpoints in the metadata store.
func (mds *EtcdMetadataStore) GetEndpoints() ([]*metadatapb.Endpoints, error) {
	resp, err := mds.client.Get(context.Background(), getEndpointsKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
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

	return mds.updateValue(key, string(val))
}

// GetPods gets all pods in the metadata store.
func (mds *EtcdMetadataStore) GetPods() ([]*metadatapb.Pod, error) {
	resp, err := mds.client.Get(context.Background(), getPodsKey(), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
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

func getPodsKey() string {
	return path.Join("/", "pod") + "/"
}

func getPodKey(e *metadatapb.Pod) string {
	return getPodKeyFromStrings(e.Metadata.UID, getNamespaceFromMetadata(e.Metadata))
}

func getPodKeyFromStrings(uid string, namespace string) string {
	return path.Join("/", "pod", namespace, uid)
}

func getContainerKeyFromStrings(namespace string, podName string, containerName string) string {
	return path.Join("/", "pods", namespace, podName, "containers", containerName, "info")
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

	return mds.updateValue(key, string(val))
}

// UpdateContainers updates the given containers in the metadata store.
func (mds *EtcdMetadataStore) UpdateContainers(containers []*metadatapb.ContainerInfo) error {
	// Get all pods with the given pod ids.
	podOps := make([]clientv3.Op, len(containers))
	for i, container := range containers {
		podOps[i] = clientv3.OpGet(getPodKeyFromStrings(container.PodUID, getNamespaceFromString(container.Namespace)))
	}

	txnresp, err := mds.client.Txn(context.TODO()).If().Then(podOps...).Commit()
	if err != nil {
		return err
	}

	// Update containers.
	var containerOps []clientv3.Op

	mu := concurrency.NewMutex(mds.sess, GetUpdateKey())
	mu.Lock(context.Background())
	defer mu.Unlock(context.Background())

	for i, resp := range txnresp.Responses {
		if len(resp.GetResponseRange().Kvs) > 0 {
			podPb := &metadatapb.Pod{}
			proto.Unmarshal(resp.GetResponseRange().Kvs[0].Value, podPb)

			container, err := containers[i].Marshal()
			if err != nil {
				log.WithError(err).Error("Could not marshall container update message.")
				continue
			}

			// TODO(michelle): Add a Lease to the key, so that they expire after a certain period of time.
			containerKey := getContainerKeyFromStrings(getNamespaceFromString(containers[i].Namespace), podPb.Metadata.Name, containers[i].Name)
			containerOps = append(containerOps, clientv3.OpPut(containerKey, string(container)))
		}
	}

	_, err = mds.client.Txn(context.TODO()).If().Then(containerOps...).Commit()

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

func getComputedSchemaKey(schemaName string) string {
	return path.Join("/", "schema", "computed", schemaName)
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

func (mds *EtcdMetadataStore) updateValue(key string, value string) error {
	mu := concurrency.NewMutex(mds.sess, GetUpdateKey())
	mu.Lock(context.Background())
	defer mu.Unlock(context.Background())
	_, err := mds.client.Put(context.Background(), key, value)
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
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID))

	return q.Enqueue(value)
}

// AddToFrontOfAgentQueue adds the given value to the front of the agent's update queue.
func (mds *EtcdMetadataStore) AddToFrontOfAgentQueue(agentID string, value *metadatapb.ResourceUpdate) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID))

	i, err := value.Marshal()
	if err != nil {
		log.WithError(err).Fatal("Unable to marshal update pb.")
	}

	return q.EnqueueAtFront(string(i))
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (mds *EtcdMetadataStore) GetFromAgentQueue(agentID string) ([]*metadatapb.ResourceUpdate, error) {
	var pbs []*metadatapb.ResourceUpdate

	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID))
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

// GetAgents gets all of the current active agents.
func (mds *EtcdMetadataStore) GetAgents() (*[]datapb.AgentData, error) {
	var agents []datapb.AgentData

	// Get all agents.
	resp, err := mds.client.Get(context.Background(), GetAgentKey(""), clientv3.WithPrefix())
	if err != nil {
		log.WithError(err).Fatal("Failed to execute etcd Get")
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

// Close cleans up the etcd metadata store, such as its etcd session.
func (mds *EtcdMetadataStore) Close() {
	mds.sess.Close()
}
