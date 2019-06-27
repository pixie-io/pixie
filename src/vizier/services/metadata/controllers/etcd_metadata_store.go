package controllers

import (
	"context"
	"errors"
	"path"
	"strings"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/clientv3/concurrency"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
	datapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// EtcdMetadataStore is the implementation of our metadata store in etcd.
type EtcdMetadataStore struct {
	client *clientv3.Client
}

// NewEtcdMetadataStore creates a new etcd metadata store.
func NewEtcdMetadataStore(client *clientv3.Client) (*EtcdMetadataStore, error) {
	mds := &EtcdMetadataStore{
		client: client,
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

	return mds.updateValue(key, string(val))
}

func getNamespaceFromMetadata(md *metadatapb.ObjectMetadata) string {
	ns := md.Namespace
	if ns == "" {
		ns = "default"
	}
	return ns
}

func getEndpointKey(e *metadatapb.Endpoints) string {
	return path.Join("/", "endpoints", getNamespaceFromMetadata(e.Metadata), e.Metadata.Uid)
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

func getPodKey(e *metadatapb.Pod) string {
	return path.Join("/", "pod", getNamespaceFromMetadata(e.Metadata), e.Metadata.Uid)
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

func getServiceKey(e *metadatapb.Service) string {
	return path.Join("/", "service", getNamespaceFromMetadata(e.Metadata), e.Metadata.Uid)
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

func (mds *EtcdMetadataStore) updateValue(key string, value string) error {
	ctx := context.Background()
	ss, err := concurrency.NewSession(mds.client, concurrency.WithContext(ctx))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session")
	}
	defer ss.Close()

	mu := concurrency.NewMutex(ss, key)
	mu.Lock(ctx)
	defer mu.Unlock(ctx)
	_, err = mds.client.Put(context.Background(), key, value)
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
func (mds *EtcdMetadataStore) AddToFrontOfAgentQueue(agentID string, value *messagespb.MetadataUpdateInfo_ResourceUpdate) error {
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID))

	i, err := value.Marshal()
	if err != nil {
		log.WithError(err).Fatal("Unable to marshal update pb.")
	}

	return q.EnqueueAtFront(string(i))
}

// GetFromAgentQueue gets all items currently in the agent's update queue.
func (mds *EtcdMetadataStore) GetFromAgentQueue(agentID string) (*[]messagespb.MetadataUpdateInfo_ResourceUpdate, error) {
	q := etcd.NewQueue(mds.client, getAgentUpdateKey(agentID))
	resp, err := q.DequeueAll()
	if err != nil {
		return nil, err
	}

	// Convert strings to pbs.
	pbs := make([]messagespb.MetadataUpdateInfo_ResourceUpdate, len(*resp))
	for i, update := range *resp {
		updatePb := messagespb.MetadataUpdateInfo_ResourceUpdate{}
		if err := proto.Unmarshal([]byte(update), &updatePb); err != nil {
			// For whatever reason, the updatePb was invalid and could not be parsed. Realistically, this
			// should never happen unless there are some corrupt values in etcd. Continue so that the agent
			// atleast gets the other updates.
			// TODO(michelle): PL-664: The agent may get out of sync. Think about how we can sync the agent + metadata service
			// so that even if they do get out of sync, we can recover.
			log.WithError(err).Error("Could not unmarshal resource update pb.")
			continue
		}
		pbs[i] = updatePb
	}

	return &pbs, nil
}

// GetAgents gets all of the current active agents.
func (mds *EtcdMetadataStore) GetAgents() (*[]datapb.AgentData, error) {
	var agents []datapb.AgentData

	ctx := context.Background()
	ss, err := concurrency.NewSession(mds.client, concurrency.WithContext(ctx))
	if err != nil {
		log.WithError(err).Fatal("Could not create new session")
	}
	defer ss.Close()

	// Get all agents.
	resp, err := mds.client.Get(ctx, GetAgentKey(""), clientv3.WithPrefix())
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
