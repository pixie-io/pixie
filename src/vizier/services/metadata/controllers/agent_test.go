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

func setupAgentManager(t *testing.T, isLeader bool) (*clientv3.Client, controllers.AgentManager, *mock_controllers.MockMetadataStore, func()) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	CreateAgent(t, existingAgentUUID, etcdClient, existingAgentInfo)
	CreateAgent(t, unhealthyAgentUUID, etcdClient, unhealthyAgentInfo)

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

func TestRegisterAgent(t *testing.T) {
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	mockMds.
		EXPECT().
		GetASID().
		Return(uint32(1), nil)

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "localhost",
		AgentID:         u,
	}
	id, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)
	assert.Equal(t, uint32(1), id)

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
	assert.Equal(t, newAgentUUID, uid.String())
	assert.Equal(t, "localhost", pb.HostInfo.Hostname)

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("localhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, newAgentUUID, string(resp.Kvs[0].Value))
}

func TestRegisterAgentNotLeader(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, false)
	defer cleanup()

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "localhost",
		AgentID:         u,
	}
	id, err := agtMgr.RegisterAgent(agentInfo)
	assert.NotNil(t, err)
	assert.Equal(t, uint32(0), id)

	// Check that agent info is not in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestRegisterAgentWithExistingHostname(t *testing.T) {
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2, err := uuid.FromString(existingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	mockMds.
		EXPECT().
		GetASID().
		Return(uint32(1), nil)

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "testhost",
		AgentID:         u,
	}
	id, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)
	assert.Equal(t, uint32(1), id)

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
	assert.Equal(t, newAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("testhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, newAgentUUID, string(resp.Kvs[0].Value))

	// Check that previous agent has been deleted.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u2))
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestRegisterExistingAgent(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(existingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agentInfo := &controllers.AgentInfo{
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
		Hostname:        "localhost",
		AgentID:         u,
	}
	_, err = agtMgr.RegisterAgent(agentInfo)
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
	assert.Equal(t, existingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)
}

func TestUpdateHeartbeat(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(existingAgentUUID)
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
	assert.Equal(t, existingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)
}

func TestUpdateHeartbeatNotLeader(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, false)
	defer cleanup()

	u, err := uuid.FromString(existingAgentUUID)
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

	u, err := uuid.FromString(newAgentUUID)
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
	if err := proto.UnmarshalText(existingAgentInfo, agent1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agents[0] = *agent1

	agent2 := &data.AgentData{}
	if err := proto.UnmarshalText(unhealthyAgentInfo, agent2); err != nil {
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

	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKey(unhealthyAgentUUID))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("anotherhost"))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))

	// Unhealthy agent should no longer have any schemas.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentSchemasKey(unhealthyAgentUUID), clientv3.WithPrefix())
	assert.Equal(t, 0, len(resp.Kvs))
	// Healthy agent should still have a schema.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentSchemasKey(existingAgentUUID), clientv3.WithPrefix())
	assert.Equal(t, 1, len(resp.Kvs))
}

func TestUpdateAgentStateNotLeader(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t, false)
	defer cleanup()

	err := agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKey(""), clientv3.WithPrefix())
	assert.Equal(t, 2, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKey(unhealthyAgentUUID))
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
	if err := proto.UnmarshalText(existingAgentInfo, agent1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsMock[0] = *agent1

	agent2 := &data.AgentData{}
	if err := proto.UnmarshalText(unhealthyAgentInfo, agent2); err != nil {
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
	u1, err := uuid.FromString(existingAgentUUID)
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
	u2, err := uuid.FromString(unhealthyAgentUUID)
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

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*metadatapb.SchemaInfo, 1)

	schema1 := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(schemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	createdProcesses := make([]*metadatapb.ProcessCreated, 2)

	cp1 := new(metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(processCreated1PB, cp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[0] = cp1
	cp2 := new(metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(processCreated2PB, cp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[1] = cp2

	cProcessInfo := make([]*metadatapb.ProcessInfo, 2)

	cpi1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(processInfo1PB, cpi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	cProcessInfo[0] = cpi1
	cpi2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(processInfo2PB, cpi2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	cProcessInfo[1] = cpi2

	mockMds.
		EXPECT().
		UpdateSchemas(u, schemas).
		DoAndReturn(func(u uuid.UUID, e []*metadatapb.SchemaInfo) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		UpdateProcesses(cProcessInfo).
		DoAndReturn(func(p []*metadatapb.ProcessInfo) error {
			wg.Done()
			return nil
		})

	update := &messagespb.AgentUpdateInfo{
		Schema:         schemas,
		ProcessCreated: createdProcesses,
	}

	agtMgr.AddToUpdateQueue(u, update)
}

func TestAgentQueueTerminatedProcesses(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(2)
	defer wg.Wait()

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*metadatapb.SchemaInfo, 1)

	schema1 := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(schemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	terminatedProcesses := make([]*metadatapb.ProcessTerminated, 2)

	tp1 := new(metadatapb.ProcessTerminated)
	if err := proto.UnmarshalText(processTerminated1PB, tp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	terminatedProcesses[0] = tp1
	tp2 := new(metadatapb.ProcessTerminated)
	if err := proto.UnmarshalText(processTerminated2PB, tp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	terminatedProcesses[1] = tp2

	processInfo := make([]*metadatapb.ProcessInfo, 2)
	pi1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(processInfo1PB, pi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	processInfo[0] = pi1
	pi2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(processInfo2PB, pi2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	processInfo[1] = pi2

	updatedInfo := make([]*metadatapb.ProcessInfo, 2)
	upi1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(processInfo1PB, upi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	upi1.StopTimestampNS = 6
	updatedInfo[0] = upi1
	upi2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(processInfo2PB, upi2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	upi2.StopTimestampNS = 10
	updatedInfo[1] = upi2

	mockMds.
		EXPECT().
		UpdateSchemas(u, schemas).
		DoAndReturn(func(u uuid.UUID, e []*metadatapb.SchemaInfo) error {
			wg.Done()
			return nil
		})

	mockMds.
		EXPECT().
		GetProcesses(gomock.Any()).
		Return(processInfo, nil)

	mockMds.
		EXPECT().
		UpdateProcesses(updatedInfo).
		DoAndReturn(func(p []*metadatapb.ProcessInfo) error {
			wg.Done()
			return nil
		})

	update := &messagespb.AgentUpdateInfo{
		Schema:            schemas,
		ProcessTerminated: terminatedProcesses,
	}

	agtMgr.AddToUpdateQueue(u, update)
}

func TestAddToUpdateQueueFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*metadatapb.SchemaInfo, 1)

	schema1 := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(schemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	mockMds.
		EXPECT().
		UpdateSchemas(u, schemas).
		DoAndReturn(func(u uuid.UUID, e []*metadatapb.SchemaInfo) error {
			wg.Done()
			return errors.New("Could not update containers")
		})

	update := &messagespb.AgentUpdateInfo{
		Schema: schemas,
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

	assert.Equal(t, 5, len(updates))

	update1 := updates[0].GetContainerUpdate()
	assert.NotNil(t, update1)
	assert.Equal(t, "0987", update1.CID)

	update2 := updates[1].GetContainerUpdate()
	assert.NotNil(t, update2)
	assert.Equal(t, "2468", update2.CID)

	update3 := updates[2].GetPodUpdate()
	assert.NotNil(t, update3)
	assert.Equal(t, "1234", update3.UID)

	update4 := updates[3].GetPodUpdate()
	assert.NotNil(t, update4)
	assert.Equal(t, "5678", update4.UID)

	update5 := updates[4].GetServiceUpdate()
	assert.NotNil(t, update5)
	assert.Equal(t, "object_md", update5.Name)
}

func TestAgentAddUpdatesToAgentQueue(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t, true)
	defer cleanup()

	u, err := uuid.FromString(newAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
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

	mockMds.
		EXPECT().
		AddUpdatesToAgentQueue(newAgentUUID, []*metadatapb.ResourceUpdate{updatePb1, updatePb2}).
		Return(nil)

	err = agtMgr.AddUpdatesToAgentQueue(u, []*metadatapb.ResourceUpdate{updatePb1, updatePb2})
	assert.Nil(t, err)
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
