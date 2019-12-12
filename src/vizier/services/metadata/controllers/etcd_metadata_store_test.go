package controllers_test

import (
	"context"
	"testing"

	"github.com/coreos/etcd/clientv3"
	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func CreateAgent(t *testing.T, agentID string, client *clientv3.Client, agentPb string) {
	info := new(agentpb.Agent)
	if err := proto.UnmarshalText(agentPb, info); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	i, err := info.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal agentData pb.")
	}

	_, err = client.Put(context.Background(), controllers.GetAgentKey(agentID), string(i))
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	_, err = client.Put(context.Background(), controllers.GetHostnameAgentKey(info.Info.HostInfo.Hostname), agentID)
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	// Add schema info.
	schema := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	s, err := schema.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal schema pb.")
	}

	_, err = client.Put(context.Background(), controllers.GetAgentSchemaKey(agentID, schema.Name), string(s))
	if err != nil {
		t.Fatal("Unable to add agent schema to etcd.")
	}
}

func TestUpdateEndpoints(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateEndpoints(expectedPb)
	if err != nil {
		t.Fatal("Could not update endpoints.")
	}

	// Check that correct endpoint info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/endpoints/a_namespace/ijkl")
	if err != nil {
		t.Fatal("Failed to get endpoints.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.Endpoints{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	mapResp, err := etcdClient.Get(context.Background(), "/services/a_namespace/object_md/pods")
	if err != nil {
		t.Fatal("Failed to get service to pod mapping.")
	}
	assert.Equal(t, 1, len(mapResp.Kvs))
	assert.Equal(t, "abcd,efgh", string(mapResp.Kvs[0].Value))
}

func TestUpdatePod(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdatePod(expectedPb)
	if err != nil {
		t.Fatal("Could not update pod.")
	}

	// Check that correct pod info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/pod/default/ijkl")
	if err != nil {
		t.Fatal("Failed to get pod.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.Pod{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, expectedPb, pb)
}

func TestUpdateService(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateService(expectedPb)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/service/a_namespace/ijkl")
	if err != nil {
		t.Fatal("Failed to get service.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.Service{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, expectedPb, pb)
}

func TestUpdateContainer(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.ContainerInfo{}
	if err := proto.UnmarshalText(testutils.ContainerInfoPB, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateContainer(expectedPb)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in etcd.
	resp, err := etcdClient.Get(context.Background(), "/containers/container1/info")
	if err != nil {
		t.Fatal("Failed to get container.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &metadatapb.ContainerInfo{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, expectedPb, pb)
}

func TestUpdateContainersFromPod(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	podInfo := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, podInfo); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateContainersFromPod(podInfo)
	assert.Nil(t, err)

	containerResp, err := etcdClient.Get(context.Background(), "/containers/test/info")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(containerResp.Kvs))
	containerPb := &metadatapb.ContainerInfo{}
	proto.Unmarshal(containerResp.Kvs[0].Value, containerPb)
	assert.Equal(t, "container1", containerPb.Name)
	assert.Equal(t, "test", containerPb.UID)
	assert.Equal(t, "ijkl", containerPb.PodUID)
}

func TestUpdateContainersFromPendingPod(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	podInfo := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PendingPodPb, podInfo); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateContainersFromPod(podInfo)
	assert.Nil(t, err)

	containerResp, err := etcdClient.Get(context.Background(), "/containers/", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 0, len(containerResp.Kvs))
}

func TestUpdateSchemas(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*metadatapb.SchemaInfo, 1)

	schema1 := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	err = mds.UpdateSchemas(u, schemas)
	assert.Nil(t, err)

	schemaResp, err := etcdClient.Get(context.Background(), "/agents/"+testutils.NewAgentUUID+"/schema/a_table")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(schemaResp.Kvs))
	schemaPb := &metadatapb.SchemaInfo{}
	proto.Unmarshal(schemaResp.Kvs[0].Value, schemaPb)
	assert.Equal(t, "a_table", schemaPb.Name)

	schemaResp, err = etcdClient.Get(context.Background(), "/schema/computed/a_table")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(schemaResp.Kvs))
	schemaPb = &metadatapb.SchemaInfo{}
	proto.Unmarshal(schemaResp.Kvs[0].Value, schemaPb)
	assert.Equal(t, "a_table", schemaPb.Name)
}

func TestGetAgentsForHostnames(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetHostnameAgentKey("test"), "agent1")
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetHostnameAgentKey("test2"), "agent2")
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetHostnameAgentKey("test3"), "agent3")
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetHostnameAgentKey("test4"), "agent4")
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	hostnames := []string{"test", "test2", "test3", "test4"}

	agents, err := mds.GetAgentsForHostnames(&hostnames)
	assert.Nil(t, err)

	assert.Equal(t, 4, len(*agents))
	assert.Equal(t, "agent1", (*agents)[0])
	assert.Equal(t, "agent2", (*agents)[1])
	assert.Equal(t, "agent3", (*agents)[2])
	assert.Equal(t, "agent4", (*agents)[3])
}

func TestGetKelvinIDs(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetKelvinAgentKey("test"), "test")
	if err != nil {
		t.Fatal("Unable to add kelvinData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetKelvinAgentKey("test2"), "test2")
	if err != nil {
		t.Fatal("Unable to add kelvinData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetKelvinAgentKey("test3"), "test3")
	if err != nil {
		t.Fatal("Unable to add kelvinData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetKelvinAgentKey("test4"), "test4")
	if err != nil {
		t.Fatal("Unable to add kelvinData to etcd.")
	}

	kelvins, err := mds.GetKelvinIDs()
	assert.Nil(t, err)

	assert.Equal(t, 4, len(kelvins))
	assert.Equal(t, "test", kelvins[0])
	assert.Equal(t, "test2", kelvins[1])
	assert.Equal(t, "test3", kelvins[2])
	assert.Equal(t, "test4", kelvins[3])
}

func TestGetAgentsForMissingHostnames(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetHostnameAgentKey("test"), "agent1")
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), controllers.GetHostnameAgentKey("test2"), "agent2")
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	hostnames := []string{"test", "test2", "test3", "test4"}

	agents, err := mds.GetAgentsForHostnames(&hostnames)
	assert.Nil(t, err)

	assert.Equal(t, 2, len(*agents))
	assert.Equal(t, "agent1", (*agents)[0])
	assert.Equal(t, "agent2", (*agents)[1])
}

func TestAddToAgentQueue(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	err = mds.AddToAgentUpdateQueue("agent1", "test")
	assert.Nil(t, err)

	q := etcd.NewQueue(etcdClient, "/agents/agent1/updates")
	resp, err := q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "test", resp)

}

func TestAddToFrontOfAgentQueue(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	err = mds.AddToAgentUpdateQueue("agent1", "test")
	assert.Nil(t, err)

	updatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	err = mds.AddToFrontOfAgentQueue("agent1", updatePb)
	assert.Nil(t, err)

	resp, err := mds.GetFromAgentQueue("agent1")
	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)
}

func TestAddUpdatesToAgentQueue(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	updatePb1 := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	updatePb2 := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid2",
				Name: "podName2",
			},
		},
	}

	err = mds.AddUpdatesToAgentQueue("agent1", []*metadatapb.ResourceUpdate{updatePb1, updatePb2})
	assert.Nil(t, err)

	resp, err := mds.GetFromAgentQueue("agent1")
	assert.Nil(t, err)
	assert.Equal(t, 2, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)
	assert.Equal(t, "podUid2", resp[1].GetPodUpdate().UID)
}

func TestGetFromAgentQueue(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	updatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	update, err := updatePb.Marshal()
	if err != nil {
		t.Fatal("Could not marshall pod update message.")
	}

	err = mds.AddToAgentUpdateQueue("agent1", string(update))
	assert.Nil(t, err)

	resp, err := mds.GetFromAgentQueue("agent1")
	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)

	q := etcd.NewQueue(etcdClient, "/agents/agent1/updates")
	dequeueResp, err := q.Dequeue()
	assert.Nil(t, err)
	assert.Equal(t, "", dequeueResp)
}

func TestGetAgents(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	CreateAgent(t, testutils.ExistingAgentUUID, etcdClient, testutils.ExistingAgentInfo)
	CreateAgent(t, testutils.UnhealthyAgentUUID, etcdClient, testutils.UnhealthyAgentInfo)

	// Add agent lock key to etcd to make sure GetAgents filters it out.
	_, err = etcdClient.Put(context.Background(),
		controllers.GetAgentKey("ae6f4648-c06b-470c-a01f-1209a3dfa4bc")+"/487f6ad73ac92645", "")
	if err != nil {
		t.Fatal("Unable to add fake agent key to etcd.")
	}

	agents, err := mds.GetAgents()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(agents))

	uid, err := utils.UUIDFromProto((agents)[0].Info.AgentID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto")
	}
	assert.Equal(t, testutils.ExistingAgentUUID, uid.String())

	uid, err = utils.UUIDFromProto((agents)[1].Info.AgentID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto")
	}
	assert.Equal(t, testutils.UnhealthyAgentUUID, uid.String())
}

func TestGetPods(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	// Create pods.
	pod1 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "abcd",
			Namespace: "test",
			UID:       "abcd-pod",
		},
	}
	pod1Text, err := pod1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal pod pb")
	}

	pod2 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "efgh",
			Namespace: "test",
			UID:       "efgh-pod",
		},
	}
	pod2Text, err := pod2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal pod pb")
	}

	_, err = etcdClient.Put(context.Background(), "/pod/test/abcd-pod", string(pod1Text))
	if err != nil {
		t.Fatal("Unable to add pod to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/pod/test/efgh-pod", string(pod2Text))
	if err != nil {
		t.Fatal("Unable to add pod to etcd.")
	}

	pods, err := mds.GetPods()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(pods))

	assert.Equal(t, pod1.Metadata.Name, (*pods[0]).Metadata.Name)
	assert.Equal(t, pod2.Metadata.Name, (*pods[1]).Metadata.Name)
}

func TestGetContainers(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	// Create containers.
	c1 := &metadatapb.ContainerInfo{
		Name: "container_1",
		UID:  "container_id_1",
	}
	c1Text, err := c1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal container pb")
	}

	c2 := &metadatapb.ContainerInfo{
		Name: "container_2",
		UID:  "container_id_2",
	}
	c2Text, err := c2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal container pb")
	}

	_, err = etcdClient.Put(context.Background(), "/containers/container_id_1/info", string(c1Text))
	if err != nil {
		t.Fatal("Unable to add container to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/containers/container_id_2/info", string(c2Text))
	if err != nil {
		t.Fatal("Unable to add container to etcd.")
	}

	containers, err := mds.GetContainers()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(containers))

	assert.Equal(t, c1.UID, (*containers[0]).UID)
	assert.Equal(t, c2.UID, (*containers[1]).UID)
}

func TestGetEndpoints(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	// Create endpoints.
	e1 := &metadatapb.Endpoints{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "abcd",
			Namespace: "test",
			UID:       "abcd",
		},
	}
	e1Text, err := e1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal endpoint pb")
	}

	e2 := &metadatapb.Endpoints{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "efgh",
			Namespace: "test",
			UID:       "efgh",
		},
	}
	e2Text, err := e2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal endpoint pb")
	}

	_, err = etcdClient.Put(context.Background(), "/endpoints/test/abcd", string(e1Text))
	if err != nil {
		t.Fatal("Unable to add endpoint to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/endpoints/test/efgh", string(e2Text))
	if err != nil {
		t.Fatal("Unable to add endpoint to etcd.")
	}

	eps, err := mds.GetEndpoints()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(eps))

	assert.Equal(t, e1.Metadata.Name, (*eps[0]).Metadata.Name)
	assert.Equal(t, e2.Metadata.Name, (*eps[1]).Metadata.Name)
}

func TestGetServices(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	// Create services.
	s1 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "abcd",
			Namespace: "test",
			UID:       "abcd-service",
		},
	}
	s1Text, err := s1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal service pb")
	}

	s2 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "efgh",
			Namespace: "test",
			UID:       "efgh-service",
		},
	}
	s2Text, err := s2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal service pb")
	}

	_, err = etcdClient.Put(context.Background(), "/service/test/abcd-service", string(s1Text))
	if err != nil {
		t.Fatal("Unable to add service to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/service/test/efgh-service", string(s2Text))
	if err != nil {
		t.Fatal("Unable to add service to etcd.")
	}

	services, err := mds.GetServices()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(services))

	assert.Equal(t, s1.Metadata.Name, (*services[0]).Metadata.Name)
	assert.Equal(t, s2.Metadata.Name, (*services[1]).Metadata.Name)
}

func TestGetASID(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	id1, err := mds.GetASID()
	assert.Nil(t, err)
	assert.Equal(t, uint32(1), id1)

	id2, err := mds.GetASID()
	assert.Nil(t, err)
	assert.Equal(t, uint32(2), id2)

	id3, err := mds.GetASID()
	assert.Nil(t, err)
	assert.Equal(t, uint32(3), id3)
}

func TestGetProcesses(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	upid1 := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	p1 := &metadatapb.ProcessInfo{
		Name: "process1",
		PID:  123,
		CID:  "567",
	}
	p1Text, err := p1.Marshal()
	_, err = etcdClient.Put(context.Background(), "/processes/123:567:89101", string(p1Text))
	if err != nil {
		t.Fatal("Unable to add process to etcd.")
	}

	upid2 := &types.UInt128{
		Low:  uint64(123),
		High: uint64(1056561955185),
	}

	upid3 := &types.UInt128{
		Low:  uint64(135),
		High: uint64(1056561955185),
	}

	p3 := &metadatapb.ProcessInfo{
		Name: "process2",
		PID:  246,
		CID:  "369",
	}
	p3Text, err := p3.Marshal()
	_, err = etcdClient.Put(context.Background(), "/processes/246:369:135", string(p3Text))
	if err != nil {
		t.Fatal("Unable to add process to etcd.")
	}

	processes, err := mds.GetProcesses([]*types.UInt128{upid1, upid2, upid3})
	assert.Nil(t, err)
	assert.Equal(t, 3, len(processes))
	assert.Equal(t, "process1", processes[0].Name)
	assert.Equal(t, (*metadatapb.ProcessInfo)(nil), processes[1])
	assert.Equal(t, "process2", processes[2].Name)
}

func TestUpdateProcesses(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	processes := make([]*metadatapb.ProcessInfo, 2)

	p1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.Process1PB, p1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	processes[0] = p1

	p2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.Process2PB, p2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	processes[1] = p2

	err = mds.UpdateProcesses(processes)
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), "/processes/123:567:89101")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	p1Pb := &metadatapb.ProcessInfo{}
	proto.Unmarshal(resp.Kvs[0].Value, p1Pb)
	assert.Equal(t, "p1", p1Pb.Name)

	resp, err = etcdClient.Get(context.Background(), "/processes/123:567:246")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	p2Pb := &metadatapb.ProcessInfo{}
	proto.Unmarshal(resp.Kvs[0].Value, p2Pb)
	assert.Equal(t, "p2", p2Pb.Name)

	// Update Process.
	p2Pb.Name = "new name"
	err = mds.UpdateProcesses([]*metadatapb.ProcessInfo{p2Pb})

	resp, err = etcdClient.Get(context.Background(), "/processes/123:567:246")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(resp.Kvs))
	p2Pb = &metadatapb.ProcessInfo{}
	proto.Unmarshal(resp.Kvs[0].Value, p2Pb)
	assert.Equal(t, "new name", p2Pb.Name)
}

func TestGetComputedSchemas(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	// Create schemas.
	c1 := &metadatapb.SchemaInfo{
		Name:             "table1",
		StartTimestampNS: 4,
	}
	c1Text, err := c1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal schema pb")
	}

	c2 := &metadatapb.SchemaInfo{
		Name:             "table2",
		StartTimestampNS: 5,
	}
	c2Text, err := c2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal schema pb")
	}

	_, err = etcdClient.Put(context.Background(), "/schema/computed/table1", string(c1Text))
	if err != nil {
		t.Fatal("Unable to add schema to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/schema/computed/table2", string(c2Text))
	if err != nil {
		t.Fatal("Unable to add schema to etcd.")
	}

	schemas, err := mds.GetComputedSchemas()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(schemas))

	assert.Equal(t, "table1", (*schemas[0]).Name)
	assert.Equal(t, "table2", (*schemas[1]).Name)
}

func TestGetNodeEndpoints(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	// Create endpoints.
	e1 := &metadatapb.Endpoints{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "abcd",
			Namespace: "test",
			UID:       "abcd",
		},
		Subsets: []*metadatapb.EndpointSubset{
			&metadatapb.EndpointSubset{
				Addresses: []*metadatapb.EndpointAddress{
					&metadatapb.EndpointAddress{
						NodeName: "test",
					},
				},
			},
		},
	}
	e1Text, err := e1.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal endpoint pb")
	}

	e2 := &metadatapb.Endpoints{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "efgh",
			Namespace: "test",
			UID:       "efgh",
		},
		Subsets: []*metadatapb.EndpointSubset{
			&metadatapb.EndpointSubset{
				Addresses: []*metadatapb.EndpointAddress{
					&metadatapb.EndpointAddress{
						NodeName: "localhost",
					},
				},
			},
		},
	}
	e2Text, err := e2.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal endpoint pb")
	}

	e3 := &metadatapb.Endpoints{
		Metadata: &metadatapb.ObjectMetadata{
			Name:                "xyz",
			Namespace:           "test",
			UID:                 "xyz",
			DeletionTimestampNS: 10,
		},
		Subsets: []*metadatapb.EndpointSubset{
			&metadatapb.EndpointSubset{
				Addresses: []*metadatapb.EndpointAddress{
					&metadatapb.EndpointAddress{
						NodeName: "localhost",
					},
				},
			},
		},
	}
	e3Text, err := e3.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal endpoint pb")
	}

	_, err = etcdClient.Put(context.Background(), "/endpoints/test/abcd", string(e1Text))
	if err != nil {
		t.Fatal("Unable to add endpoint to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/endpoints/test/efgh", string(e2Text))
	if err != nil {
		t.Fatal("Unable to add endpoint to etcd.")
	}

	_, err = etcdClient.Put(context.Background(), "/endpoints/test/xyz", string(e3Text))
	if err != nil {
		t.Fatal("Unable to add endpoint to etcd.")
	}

	eps, err := mds.GetNodeEndpoints("localhost")
	assert.Nil(t, err)
	assert.Equal(t, 1, len(eps))

	assert.Equal(t, e2.Metadata.Name, (*eps[0]).Metadata.Name)

	eps, err = mds.GetNodeEndpoints("")
	assert.Nil(t, err)
	assert.Equal(t, 2, len(eps))
	assert.Equal(t, e1.Metadata.Name, (*eps[0]).Metadata.Name)
	assert.Equal(t, e2.Metadata.Name, (*eps[1]).Metadata.Name)
}
