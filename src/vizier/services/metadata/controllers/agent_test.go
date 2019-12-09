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

	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	utils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func setupAgentManager(t *testing.T) (*clientv3.Client, controllers.AgentManager, *mock_controllers.MockMetadataStore, func()) {
	etcdClient, cleanup := testingutils.SetupEtcd(t)

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockMds := mock_controllers.NewMockMetadataStore(ctrl)

	CreateAgent(t, testutils.ExistingAgentUUID, etcdClient, testutils.ExistingAgentInfo)
	CreateAgent(t, testutils.UnhealthyAgentUUID, etcdClient, testutils.UnhealthyAgentInfo)
	CreateAgent(t, testutils.UnhealthyKelvinAgentUUID, etcdClient, testutils.UnhealthyKelvinAgentInfo)

	clock := testingutils.NewTestClock(time.Unix(0, testutils.ClockNowNS))
	agtMgr := controllers.NewAgentManagerWithClock(etcdClient, mockMds, clock)

	return etcdClient, agtMgr, mockMds, cleanup
}

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

	if !info.Info.Capabilities.CollectsData {
		_, err = client.Put(context.Background(), controllers.GetKelvinAgentKey(agentID), agentID)
		if err != nil {
			t.Fatal("Unable to add kelvin data to etcd.")
		}
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

func TestRegisterAgent(t *testing.T) {
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(&u)

	mockMds.
		EXPECT().
		GetASID().
		Return(uint32(1), nil)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "localhost",
			},
			AgentID: upb,
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
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
	pb := &agentpb.Agent{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(testutils.ClockNowNS), pb.LastHeartbeatNS)
	assert.Equal(t, int64(testutils.ClockNowNS), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.NewAgentUUID, uid.String())
	assert.Equal(t, "localhost", pb.Info.HostInfo.Hostname)

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("localhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, testutils.NewAgentUUID, string(resp.Kvs[0].Value))
}

func TestRegisterKelvinAgent(t *testing.T) {
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.KelvinAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(&u)

	mockMds.
		EXPECT().
		GetASID().
		Return(uint32(1), nil)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "test",
			},
			AgentID: upb,
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: false,
			},
		},
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
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
	pb := &agentpb.Agent{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)
	uid, err := utils.UUIDFromProto(pb.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.KelvinAgentUUID, uid.String())

	resp, err = etcdClient.Get(context.Background(), controllers.GetKelvinAgentKey(testutils.KelvinAgentUUID))
	if err != nil {
		t.Fatal("Failed to get kelvin info.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, testutils.KelvinAgentUUID, string(resp.Kvs[0].Value))
}

func TestRegisterAgentWithExistingHostname(t *testing.T) {
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(&u)
	u2, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	mockMds.
		EXPECT().
		GetASID().
		Return(uint32(1), nil)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "testhost",
			},
			AgentID: upb,
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
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
	pb := &agentpb.Agent{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(testutils.ClockNowNS), pb.LastHeartbeatNS)
	assert.Equal(t, int64(testutils.ClockNowNS), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.NewAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.Info.HostInfo.Hostname)

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("testhost"))
	if err != nil {
		t.Fatal("Failed to get agent hostname.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	assert.Equal(t, testutils.NewAgentUUID, string(resp.Kvs[0].Value))

	// Check that previous agent has been deleted.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u2))
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestRegisterExistingAgent(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(&u)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "localhost",
			},
			AgentID: upb,
		},
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
	}
	_, err = agtMgr.RegisterAgent(agentInfo)
	assert.NotNil(t, err)

	// Check that correct agent info is in etcd.
	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKeyFromUUID(u))
	if err != nil {
		t.Fatal("Failed to get agent.")
	}
	assert.Equal(t, 1, len(resp.Kvs))
	pb := &agentpb.Agent{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(testutils.HealthyAgentLastHeartbeatNS), pb.LastHeartbeatNS) // 70 seconds in NS.
	assert.Equal(t, int64(0), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.ExistingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.Info.HostInfo.Hostname)
}

func TestUpdateHeartbeat(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.ExistingAgentUUID)
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
	pb := &agentpb.Agent{}
	proto.Unmarshal(resp.Kvs[0].Value, pb)

	assert.Equal(t, int64(testutils.ClockNowNS), pb.LastHeartbeatNS)
	assert.Equal(t, int64(0), pb.CreateTimeNS)
	uid, err := utils.UUIDFromProto(pb.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.ExistingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.Info.HostInfo.Hostname)
}

func TestUpdateHeartbeatForNonExistingAgent(t *testing.T) {
	etcdClient, agtMgr, _, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
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
	etcdClient, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	agents := make([]*agentpb.Agent, 3)

	agent1 := &agentpb.Agent{}
	if err := proto.UnmarshalText(testutils.ExistingAgentInfo, agent1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agents[0] = agent1

	agent2 := &agentpb.Agent{}
	if err := proto.UnmarshalText(testutils.UnhealthyAgentInfo, agent2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agents[1] = agent2

	agent3 := &agentpb.Agent{}
	if err := proto.UnmarshalText(testutils.UnhealthyKelvinAgentInfo, agent3); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agents[2] = agent3

	mockMds.
		EXPECT().
		GetAgents().
		Return(agents, nil)

	err := agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKey(""), clientv3.WithPrefix())
	assert.Equal(t, 1, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKey(testutils.UnhealthyAgentUUID))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetHostnameAgentKey("anotherhost"))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))

	// Unhealthy agent should no longer have any schemas.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentSchemasKey(testutils.UnhealthyAgentUUID), clientv3.WithPrefix())
	assert.Equal(t, 0, len(resp.Kvs))
	// Healthy agent should still have a schema.
	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentSchemasKey(testutils.ExistingAgentUUID), clientv3.WithPrefix())
	assert.Equal(t, 1, len(resp.Kvs))

	// Should have no Kelvin entries.
	resp, err = etcdClient.Get(context.Background(), controllers.GetKelvinAgentKey(""), clientv3.WithPrefix())
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestUpdateAgentStateGetAgentsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	agents := make([]*agentpb.Agent, 0)

	mockMds.
		EXPECT().
		GetAgents().
		Return(agents, errors.New("could not get agents"))

	err := agtMgr.UpdateAgentState()
	assert.NotNil(t, err)
}

func TestGetActiveAgents(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	agentsMock := make([]*agentpb.Agent, 2)

	agent1 := &agentpb.Agent{}
	if err := proto.UnmarshalText(testutils.ExistingAgentInfo, agent1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsMock[0] = agent1

	agent2 := &agentpb.Agent{}
	if err := proto.UnmarshalText(testutils.UnhealthyAgentInfo, agent2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsMock[1] = agent2

	mockMds.
		EXPECT().
		GetAgents().
		Return(agentsMock, nil)

	agents, err := agtMgr.GetActiveAgents()
	assert.Nil(t, err)

	assert.Equal(t, 2, len(agents))

	agent1Info := &agentpb.Agent{
		LastHeartbeatNS: testutils.HealthyAgentLastHeartbeatNS,
		CreateTimeNS:    0,
		Info: &agentpb.AgentInfo{
			AgentID: &uuidpb.UUID{Data: []byte("7ba7b8109dad11d180b400c04fd430c8")},
			HostInfo: &agentpb.HostInfo{
				Hostname: "testhost",
			},
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
	}
	assert.Equal(t, agent1Info, agents[0])

	agent2Info := &agentpb.Agent{
		LastHeartbeatNS: 0,
		CreateTimeNS:    0,
		Info: &agentpb.AgentInfo{
			AgentID: &uuidpb.UUID{Data: []byte("8ba7b8109dad11d180b400c04fd430c8")},
			HostInfo: &agentpb.HostInfo{
				Hostname: "anotherhost",
			},
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
	}
	assert.Equal(t, agent2Info, agents[1])
}

func TestGetActiveAgentsGetAgentsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	agents := make([]*agentpb.Agent, 0)

	mockMds.
		EXPECT().
		GetAgents().
		Return(agents, errors.New("could not get agents"))

	_, err := agtMgr.GetActiveAgents()
	assert.NotNil(t, err)
}

func TestAddToUpdateQueue(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(2)
	defer wg.Wait()

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

	createdProcesses := make([]*metadatapb.ProcessCreated, 2)

	cp1 := new(metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(testutils.ProcessCreated1PB, cp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[0] = cp1
	cp2 := new(metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(testutils.ProcessCreated2PB, cp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[1] = cp2

	cProcessInfo := make([]*metadatapb.ProcessInfo, 2)

	cpi1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo1PB, cpi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	cProcessInfo[0] = cpi1
	cpi2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo2PB, cpi2); err != nil {
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
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(2)
	defer wg.Wait()

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

	terminatedProcesses := make([]*metadatapb.ProcessTerminated, 2)

	tp1 := new(metadatapb.ProcessTerminated)
	if err := proto.UnmarshalText(testutils.ProcessTerminated1PB, tp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	terminatedProcesses[0] = tp1
	tp2 := new(metadatapb.ProcessTerminated)
	if err := proto.UnmarshalText(testutils.ProcessTerminated2PB, tp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	terminatedProcesses[1] = tp2

	processInfo := make([]*metadatapb.ProcessInfo, 2)
	pi1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo1PB, pi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	processInfo[0] = pi1
	pi2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo2PB, pi2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	processInfo[1] = pi2

	updatedInfo := make([]*metadatapb.ProcessInfo, 2)
	upi1 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo1PB, upi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	upi1.StopTimestampNS = 6
	updatedInfo[0] = upi1
	upi2 := new(metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo2PB, upi2); err != nil {
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
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

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
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
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
	if err := proto.UnmarshalText(testutils.EndpointsPb, ep1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	epMock[0] = ep1

	mockMds.
		EXPECT().
		GetNodePods("localhost").
		Return(podMock, nil)

	mockMds.
		EXPECT().
		GetNodeEndpoints("localhost").
		Return(epMock, nil)

	updates, err := agtMgr.GetMetadataUpdates("localhost")
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
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
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
		AddUpdatesToAgentQueue(testutils.NewAgentUUID, []*metadatapb.ResourceUpdate{updatePb1, updatePb2}).
		Return(nil)

	err = agtMgr.AddUpdatesToAgentQueue(u, []*metadatapb.ResourceUpdate{updatePb1, updatePb2})
	assert.Nil(t, err)
}

func TestGetMetadataUpdatesGetPodsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
	defer cleanup()

	mockMds.
		EXPECT().
		GetNodePods("localhost").
		Return(nil, errors.New("Could not get pods"))

	_, err := agtMgr.GetMetadataUpdates("localhost")
	assert.NotNil(t, err)
}

func TestGetMetadataUpdatesGetEndpointsFailed(t *testing.T) {
	_, agtMgr, mockMds, cleanup := setupAgentManager(t)
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
		GetNodePods("localhost").
		Return(podMock, nil)

	mockMds.
		EXPECT().
		GetNodeEndpoints("localhost").
		Return(nil, errors.New("Get endpoints failed"))

	_, err := agtMgr.GetMetadataUpdates("localhost")
	assert.NotNil(t, err)
}
