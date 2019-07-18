package controllers_test

import (
	"context"
	"errors"
	"sync"
	"testing"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	utils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	data "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// LastHeartBeatNS is 65 seconds, in NS.
var ExistingAgentInfo = `
agent_id {
	data: "7ba7b8109dad11d180b400c04fd430c8"
}
host_info {
	hostname: "testhost"
}
create_time_ns: 0
last_heartbeat_ns: 65000000000
`

const clockNowNS = 1E9 * 70                  // 70s in NS. This is slightly greater than the expiration time for the unhealthy agent.
const healthyAgentLastHeartbeatNS = 1E9 * 65 // 65 seconds in NS. This is slightly less than the current time.

var UnhealthyAgentInfo = `
agent_id {
	data: "8ba7b8109dad11d180b400c04fd430c8"
}
host_info {
	hostname: "anotherhost"
}
create_time_ns: 0
last_heartbeat_ns: 0
`

var NewAgentUUID = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
var ExistingAgentUUID = "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
var UnhealthyAgentUUID = "8ba7b810-9dad-11d1-80b4-00c04fd430c8"

func setupAgentManager(t *testing.T, isLeader bool) (*clientv3.Client, controllers.AgentManager, *mock_controllers.MockMetadataStore, func()) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	CreateAgent(t, ExistingAgentUUID, etcdClient, ExistingAgentInfo)
	CreateAgent(t, UnhealthyAgentUUID, etcdClient, UnhealthyAgentInfo)

	clock := testingutils.NewTestClock(time.Unix(0, clockNowNS))
	agtMgr := controllers.NewAgentManagerWithClock(etcdClient, mockMds, isLeader, clock)

	return etcdClient, agtMgr, mockMds, cleanup
}

func CreateAgent(t *testing.T, agentID string, client *clientv3.Client, agentPb string) {
	info := new(data.AgentData)
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

	_, err = client.Put(context.Background(), controllers.GetHostnameAgentKey(info.HostInfo.Hostname), agentID)
	if err != nil {
		t.Fatal("Unable to add agentData to etcd.")
	}

	// Add schema info.
	schema := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(schemaInfoPB, schema); err != nil {
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

func TestCreateAgent(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "localhost",
		AgentID:         u,
	}
	err = agtMgr.CreateAgent(agentInfo)
	assert.Equal(t, nil, err)

	// Check that correct agent info is in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &data.AgentData{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(clockNowNS), pb.LastHeartbeatNS)
	assert.Equal(t, int64(clockNowNS), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, NewAgentUUID, uid.String())
	assert.Equal(t, "localhost", pb.HostInfo.Hostname)

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("localhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, NewAgentUUID, string(resp.Kvs[0].Value))
}

func TestCreateAgentNotLeader(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, false)
	defer cleanup()

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "localhost",
		AgentID:         u,
	}
	err = agtMgr.CreateAgent(agentInfo)
	assert.Equal(t, nil, err)

	// Check that agent info is not in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestCreateAgentWithExistingHostname(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2, err := uuid.FromString(ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "testhost",
		AgentID:         u,
	}
	err = agtMgr.CreateAgent(agentInfo)
	assert.Nil(t, err)

	// Check that correct agent info is in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &data.AgentData{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(clockNowNS), pb.LastHeartbeatNS)
	assert.Equal(t, int64(clockNowNS), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, NewAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("testhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, NewAgentUUID, string(resp.Kvs[0].Value))

	// Check that previous agent has been deleted.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u2))
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestCreateExistingAgent(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "localhost",
		AgentID:         u,
	}
	err = agtMgr.CreateAgent(agentInfo)
	assert.NotNil(t, err)

	// Check that correct agent info is in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &data.AgentData{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(healthyAgentLastHeartbeatNS), pb.LastHeartbeatNS) // 70 seconds in NS.
	assert.Equal(t, int64(0), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, ExistingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)
}

func TestUpdateHeartbeat(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.UpdateHeartbeat(u)
	assert.Nil(t, err)

	// Check that correct agent info is in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &data.AgentData{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(clockNowNS), pb.LastHeartbeatNS)
	assert.Equal(t, int64(0), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, ExistingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)
}

func TestUpdateHeartbeatNotLeader(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, false)
	defer cleanup()

	u, err := uuid.FromString(ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.UpdateHeartbeat(u)
	assert.Nil(t, err)

	// Check that correct agent info is in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &data.AgentData{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(healthyAgentLastHeartbeatNS), pb.LastHeartbeatNS)
}

func TestUpdateHeartbeatForNonExistingAgent(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.UpdateHeartbeat(u)
	assert.NotNil(t, err)

	resp, err := etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("localhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestUpdateAgentState(t *testing.T) {
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	agents := make([]data.AgentData, 2)

	agent1 := &data.AgentData{}
	if err := proto.UnmarshalText(ExistingAgentInfo, agent1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agents[0] = *agent1

	agent2 := &data.AgentData{}
	if err := proto.UnmarshalText(UnhealthyAgentInfo, agent2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agents[1] = *agent2

	mockMds.
		EXPECT().
		GetAgents().
		Return(&agents, nil)

	err := agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKey(""), clientv3.WithPrefix())
	assert.Equal(t, 1, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKey(UnhealthyAgentUUID))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("anotherhost"))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))

	// Unhealthy agent should no longer have any schemas.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentSchemasKey(UnhealthyAgentUUID), clientv3.WithPrefix())
	assert.Equal(t, 0, len(resp.Kvs))
	// Healthy agent should still have a schema.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentSchemasKey(ExistingAgentUUID), clientv3.WithPrefix())
	assert.Equal(t, 1, len(resp.Kvs))
}

func TestUpdateAgentStateNotLeader(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, false)
	defer cleanup()

	err := agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKey(""), clientv3.WithPrefix())
	assert.Equal(t, 2, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKey(UnhealthyAgentUUID))
	// Agent should still exist in etcd.
	assert.Equal(t, 1, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("anotherhost"))
	// Agent should still exist in etcd.
	assert.Equal(t, 1, len(resp.Kvs))
}

func TestUpdateAgentStateGetAgentsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	agents := make([]data.AgentData, 0)

	mockMds.
		EXPECT().
		GetAgents().
		Return(&agents, errors.New("could not get agents"))

	err := agtMgr.UpdateAgentState()
	assert.NotNil(t, err)
}

func TestGetActiveAgents(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	agentsMock := make([]data.AgentData, 2)

	agent1 := &data.AgentData{}
	if err := proto.UnmarshalText(ExistingAgentInfo, agent1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsMock[0] = *agent1

	agent2 := &data.AgentData{}
	if err := proto.UnmarshalText(UnhealthyAgentInfo, agent2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsMock[1] = *agent2

	mockMds.
		EXPECT().
		GetAgents().
		Return(&agentsMock, nil)

	agents, err := agtMgr.GetActiveAgents()
	assert.Nil(t, err)

	assert.Equal(t, 2, len(agents))
	// Check agents contain correct info.
	u1, err := uuid.FromString(ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	agent1Info := &controllers.AgentInfo{
		LastHeartbeatNS: healthyAgentLastHeartbeatNS,
		CreateTimeNS:    0,
		AgentID:         u1,
		Hostname:        "testhost",
	}
	assert.Equal(t, *agent1Info, agents[0])
	u2, err := uuid.FromString(UnhealthyAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	agent2Info := &controllers.AgentInfo{
		LastHeartbeatNS: 0,
		CreateTimeNS:    0,
		AgentID:         u2,
		Hostname:        "anotherhost",
	}
	assert.Equal(t, *agent2Info, agents[1])
}

func TestGetActiveAgentsGetAgentsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	agents := make([]data.AgentData, 0)

	mockMds.
		EXPECT().
		GetAgents().
		Return(&agents, errors.New("could not get agents"))

	_, err := agtMgr.GetActiveAgents()
	assert.NotNil(t, err)
}

func TestAddToUpdateQueue(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(2)
	defer wg.Wait()

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	containers := make([]*metadatapb.ContainerInfo, 1)

	container1 := new(metadatapb.ContainerInfo)
	if err := proto.UnmarshalText(containerInfoPB, container1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	containers[0] = container1

	schemas := make([]*metadatapb.SchemaInfo, 1)

	schema1 := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(schemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	mockMds.
		EXPECT().
		UpdateContainers(containers).
		DoAndReturn(func(e []*metadatapb.ContainerInfo) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		UpdateSchemas(u, schemas).
		DoAndReturn(func(u uuid.UUID, e []*metadatapb.SchemaInfo) error {
			wg.Done()
			return nil
		})

	update := &messagespb.AgentUpdateInfo{
		Containers: containers,
		Schema:     schemas,
	}

	agtMgr.AddToUpdateQueue(u, update)
}

func TestAddToUpdateQueueFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	u, err := uuid.FromString(NewAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string")
	}

	containers := make([]*metadatapb.ContainerInfo, 1)

	container1 := new(metadatapb.ContainerInfo)
	if err := proto.UnmarshalText(containerInfoPB, container1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	containers[0] = container1

	mockMds.
		EXPECT().
		UpdateContainers(containers).
		DoAndReturn(func(e []*metadatapb.ContainerInfo) error {
			wg.Done()
			return errors.New("Could not update containers")
		})

	update := &messagespb.AgentUpdateInfo{
		Containers: containers,
	}

	agtMgr.AddToUpdateQueue(u, update)
}

func TestGetMetadataUpdates(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	podMock := make([]*metadatapb.Pod, 2)
	containers := make([]*metadatapb.ContainerStatus, 2)

	containers[0] = &metadatapb.ContainerStatus{
		Name:        "c1",
		ContainerID: "0987",
	}

	containers[1] = &metadatapb.ContainerStatus{
		Name:        "c2",
		ContainerID: "2468",
	}

	pod1 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name: "abcd",
			UID:  "1234",
		},
		Status: &metadatapb.PodStatus{
			ContainerStatuses: containers,
		},
	}
	podMock[0] = pod1
	pod2 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name: "efgh",
			UID:  "5678",
		},
		Status: &metadatapb.PodStatus{},
	}
	podMock[1] = pod2

	epMock := make([]*metadatapb.Endpoints, 1)
	ep1 := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(endpointsPb, ep1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	epMock[0] = ep1

	mockMds.
		EXPECT().
		GetPods().
		Return(podMock, nil)

	mockMds.
		EXPECT().
		GetEndpoints().
		Return(epMock, nil)

	updates, err := agtMgr.GetMetadataUpdates()
	assert.Nil(t, err)

	assert.Equal(t, 5, len(*updates))

	update1 := (*updates)[0].GetContainerUpdate()
	assert.NotNil(t, update1)
	assert.Equal(t, "0987", update1.CID)

	update2 := (*updates)[1].GetContainerUpdate()
	assert.NotNil(t, update2)
	assert.Equal(t, "2468", update2.CID)

	update3 := (*updates)[2].GetPodUpdate()
	assert.NotNil(t, update3)
	assert.Equal(t, "1234", update3.UID)

	update4 := (*updates)[3].GetPodUpdate()
	assert.NotNil(t, update4)
	assert.Equal(t, "5678", update4.UID)

	update5 := (*updates)[4].GetServiceUpdate()
	assert.NotNil(t, update5)
	assert.Equal(t, "object_md", update5.Name)
}

func TestGetMetadataUpdatesGetPodsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	mockMds.
		EXPECT().
		GetPods().
		Return(nil, errors.New("Could not get pods"))

	_, err := agtMgr.GetMetadataUpdates()
	assert.NotNil(t, err)
}

func TestGetMetadataUpdatesGetEndpointsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	podMock := make([]*metadatapb.Pod, 2)

	pod1 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name: "abcd",
			UID:  "1234",
		},
	}
	podMock[0] = pod1
	pod2 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name: "efgh",
			UID:  "5678",
		},
	}
	podMock[1] = pod2

	mockMds.
		EXPECT().
		GetPods().
		Return(podMock, nil)

	mockMds.
		EXPECT().
		GetEndpoints().
		Return(nil, errors.New("Get endpoints failed"))

	_, err := agtMgr.GetMetadataUpdates()
	assert.NotNil(t, err)
}
