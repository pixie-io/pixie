package controllers_test

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/etcd"
)

var containerInfoPB = `
name: "container_1"
uid: "container1"
pod_uid: "ijkl"
namespace: "ns"
`

var schemaInfoPB = `
name: "a_table"
start_timestamp_ns: 2
columns {
	name: "column_1"
	data_type: 2
}
columns {
	name: "column_2"
	data_type: 4
}
`

func TestUpdateEndpoints(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, expectedPb); err != nil {
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
	assert.Equal(t, "abcd", string(mapResp.Kvs[0].Value))
}

func TestUpdatePod(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPb, expectedPb); err != nil {
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
	if err := proto.UnmarshalText(servicePb, expectedPb); err != nil {
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

func TestUpdateContainers(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	podInfo := &metadatapb.Pod{}
	if err := proto.UnmarshalText(podPb, podInfo); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	val, err := podInfo.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal pod pb")
	}

	_, err = etcdClient.Put(context.Background(), "/pod/ns/ijkl", string(val))
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	containers := make([]*metadatapb.ContainerInfo, 1)

	container1 := new(metadatapb.ContainerInfo)
	if err := proto.UnmarshalText(containerInfoPB, container1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	containers[0] = container1

	err = mds.UpdateContainers(containers)
	assert.Nil(t, err)

	containerResp, err := etcdClient.Get(context.Background(), "/pods/ns/object_md/containers/container_1/info")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(containerResp.Kvs))
	containerPb := &metadatapb.ContainerInfo{}
	proto.Unmarshal(containerResp.Kvs[0].Value, containerPb)
	assert.Equal(t, "container_1", containerPb.Name)
}

func TestUpdateSchemas(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*metadatapb.SchemaInfo, 1)

	schema1 := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(schemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	err = mds.UpdateSchemas(u, schemas)
	assert.Nil(t, err)

	schemaResp, err := etcdClient.Get(context.Background(), "/agents/"+NewAgentUUID+"/schema/a_table")
	if err != nil {
		t.Fatal("Unable to get container from etcd")
	}

	assert.Equal(t, 1, len(schemaResp.Kvs))
	schemaPb := &metadatapb.SchemaInfo{}
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

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Type: messagespb.POD,
		Metadata: &metadatapb.ObjectMetadata{
			UID:  "podUid",
			Name: "podName",
		},
	}

	err = mds.AddToFrontOfAgentQueue("agent1", updatePb)
	assert.Nil(t, err)

	resp, err := mds.GetFromAgentQueue("agent1")
	assert.Nil(t, err)
	assert.Equal(t, 1, len(*resp))
	assert.Equal(t, "podUid", (*resp)[0].Metadata.UID)
}

func TestGetFromAgentQueue(t *testing.T) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)
	defer cleanup()

	mds, err := controllers.NewEtcdMetadataStore(etcdClient)
	if err != nil {
		t.Fatal("Failed to create metadata store.")
	}

	updatePb := &messagespb.MetadataUpdateInfo_ResourceUpdate{
		Type: messagespb.POD,
		Metadata: &metadatapb.ObjectMetadata{
			UID:  "podUid",
			Name: "podName",
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
	assert.Equal(t, 1, len(*resp))
	assert.Equal(t, "podUid", (*resp)[0].Metadata.UID)

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

	CreateAgent(t, ExistingAgentUUID, etcdClient, ExistingAgentInfo)
	CreateAgent(t, UnhealthyAgentUUID, etcdClient, UnhealthyAgentInfo)

	// Add agent lock key to etcd to make sure GetAgents filters it out.
	_, err = etcdClient.Put(context.Background(),
		controllers.GetAgentKey("ae6f4648-c06b-470c-a01f-1209a3dfa4bc")+"/487f6ad73ac92645", "")
	if err != nil {
		t.Fatal("Unable to add fake agent key to etcd.")
	}

	agents, err := mds.GetAgents()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(*agents))

	uid, err := utils.UUIDFromProto((*agents)[0].AgentID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto")
	}
	assert.Equal(t, ExistingAgentUUID, uid.String())

	uid, err = utils.UUIDFromProto((*agents)[1].AgentID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto")
	}
	assert.Equal(t, UnhealthyAgentUUID, uid.String())
}
