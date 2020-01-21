package controllers_test

import (
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	utils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messagespb "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func setupAgentManager(t *testing.T) (controllers.NewMetadataStore, controllers.AgentManager) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		Get(gomock.Any()).
		Return(nil, nil).
		AnyTimes()
	mockDs.
		EXPECT().
		DeleteWithPrefix(gomock.Any()).
		Return(nil).
		AnyTimes()
	mockDs.
		EXPECT().
		GetWithPrefix(gomock.Any()).
		Return(nil, nil, nil).
		AnyTimes()

	clock := testingutils.NewTestClock(time.Unix(0, testutils.ClockNowNS))
	c := kvstore.NewCache(mockDs)
	mds, err := controllers.NewKVMetadataStore(c)
	if err != nil {
		t.Fatalf("Could not create metadata store")
	}

	createAgentInMDS(t, testutils.ExistingAgentUUID, mds, testutils.ExistingAgentInfo)
	createAgentInMDS(t, testutils.UnhealthyAgentUUID, mds, testutils.UnhealthyAgentInfo)
	createAgentInMDS(t, testutils.UnhealthyKelvinAgentUUID, mds, testutils.UnhealthyKelvinAgentInfo)

	agtMgr := controllers.NewAgentManagerWithClock(mds, clock)

	return mds, agtMgr
}

func createAgentInMDS(t *testing.T, agentID string, mds controllers.NewMetadataStore, agentPb string) {
	info := new(agentpb.Agent)
	if err := proto.UnmarshalText(agentPb, info); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for %s", agentID)
	}
	agUUID, err := uuid.FromString(agentID)
	if err != nil {
		t.Fatalf("Could not convert uuid from string")
	}
	_, _ = mds.GetASID()
	err = mds.CreateAgent(agUUID, info)

	// Add schema info.
	schema := new(metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	err = mds.UpdateSchemas(agUUID, []*metadatapb.SchemaInfo{schema})
	if err != nil {
		t.Fatalf("Could not add schema for agent")
	}
}

func TestRegisterAgent(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

	u, err := uuid.FromString(testutils.NewAgentUUID)
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
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
		LastHeartbeatNS: 1,
		CreateTimeNS:    4,
	}

	id, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)
	assert.Equal(t, uint32(4), id)

	// Check that agent exists now.
	agent, err := mds.GetAgent(u)
	assert.Nil(t, err)
	assert.NotNil(t, agent)

	assert.Equal(t, int64(testutils.ClockNowNS), agent.LastHeartbeatNS)
	assert.Equal(t, int64(testutils.ClockNowNS), agent.CreateTimeNS)
	uid, err := utils.UUIDFromProto(agent.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.NewAgentUUID, uid.String())
	assert.Equal(t, "localhost", agent.Info.HostInfo.Hostname)
	assert.Equal(t, uint32(4), agent.ASID)

	hostnameID, err := mds.GetAgentIDForHostname("localhost")
	assert.Nil(t, err)
	assert.Equal(t, testutils.NewAgentUUID, hostnameID)
}

func TestRegisterKelvinAgent(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

	u, err := uuid.FromString(testutils.KelvinAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(&u)

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

	kelvins, err := mds.GetKelvinIDs()
	assert.Nil(t, err)
	assert.Equal(t, 1, len(kelvins))

	id, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)
	assert.Equal(t, uint32(4), id)

	// Check that agent exists now.
	agent, err := mds.GetAgent(u)
	assert.Nil(t, err)
	assert.NotNil(t, agent)

	assert.Equal(t, int64(testutils.ClockNowNS), agent.LastHeartbeatNS)
	assert.Equal(t, int64(testutils.ClockNowNS), agent.CreateTimeNS)
	uid, err := utils.UUIDFromProto(agent.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.KelvinAgentUUID, uid.String())
	assert.Equal(t, "test", agent.Info.HostInfo.Hostname)
	assert.Equal(t, uint32(4), agent.ASID)

	hostnameID, err := mds.GetAgentIDForHostname("test")
	assert.Nil(t, err)
	assert.Equal(t, testutils.KelvinAgentUUID, hostnameID)

	kelvins, err = mds.GetKelvinIDs()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(kelvins))
}

func TestRegisterAgentWithExistingHostname(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(&u)
	u2, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

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
	assert.Equal(t, uint32(4), id)

	// Check that correct agent info is in MDS.
	agent, err := mds.GetAgent(u)
	assert.Nil(t, err)
	assert.NotNil(t, agent)

	assert.Equal(t, int64(testutils.ClockNowNS), agent.LastHeartbeatNS)
	assert.Equal(t, int64(testutils.ClockNowNS), agent.CreateTimeNS)
	uid, err := utils.UUIDFromProto(agent.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.NewAgentUUID, uid.String())
	assert.Equal(t, "testhost", agent.Info.HostInfo.Hostname)
	assert.Equal(t, uint32(4), agent.ASID)

	hostnameID, err := mds.GetAgentIDForHostname("testhost")
	assert.Nil(t, err)
	assert.Equal(t, testutils.NewAgentUUID, hostnameID)

	// Check that previous agent has been deleted.
	agent, err = mds.GetAgent(u2)
	assert.Nil(t, err)
	assert.Nil(t, agent)
}

func TestRegisterExistingAgent(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

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

	// Check that correct agent info is in MDS.
	agent, err := mds.GetAgent(u)
	assert.Nil(t, err)
	assert.NotNil(t, agent)
	assert.Equal(t, int64(testutils.HealthyAgentLastHeartbeatNS), agent.LastHeartbeatNS) // 70 seconds in NS.
	assert.Equal(t, int64(0), agent.CreateTimeNS)
	uid, err := utils.UUIDFromProto(agent.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.ExistingAgentUUID, uid.String())
	assert.Equal(t, "testhost", agent.Info.HostInfo.Hostname)
	assert.Equal(t, uint32(123), agent.ASID)
}

func TestUpdateHeartbeat(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)
	u, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.UpdateHeartbeat(u)
	assert.Nil(t, err)

	// Check that correct agent info is in etcd.
	agent, err := mds.GetAgent(u)
	assert.Nil(t, err)
	assert.NotNil(t, agent)

	assert.Equal(t, int64(testutils.ClockNowNS), agent.LastHeartbeatNS)
	assert.Equal(t, int64(0), agent.CreateTimeNS)
	uid, err := utils.UUIDFromProto(agent.Info.AgentID)
	assert.Equal(t, nil, err)
	assert.Equal(t, testutils.ExistingAgentUUID, uid.String())
	assert.Equal(t, "testhost", agent.Info.HostInfo.Hostname)
}

func TestUpdateHeartbeatForNonExistingAgent(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.UpdateHeartbeat(u)
	assert.NotNil(t, err)
}

func TestUpdateAgentState(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

	err := agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	agents, err := mds.GetAgents()
	assert.Equal(t, 1, len(agents))

	u, err := uuid.FromString(testutils.UnhealthyAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	agent, err := mds.GetAgent(u)
	assert.Nil(t, err)
	assert.Nil(t, agent)

	hostnameID, err := mds.GetAgentIDForHostname("anotherhost")
	assert.Nil(t, err)
	assert.Equal(t, "", hostnameID)

	// Should have no Kelvin entries.
	kelvins, err := mds.GetKelvinIDs()
	assert.Nil(t, err)
	assert.Equal(t, 0, len(kelvins))
}

func TestGetActiveAgents(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	agents, err := agtMgr.GetActiveAgents()
	assert.Nil(t, err)

	assert.Equal(t, 3, len(agents))

	agent0Info := &agentpb.Agent{
		LastHeartbeatNS: 0,
		CreateTimeNS:    0,
		Info: &agentpb.AgentInfo{
			AgentID: &uuidpb.UUID{Data: []byte("5ba7b8109dad11d180b400c04fd430c8")},
			HostInfo: &agentpb.HostInfo{
				Hostname: "abcd",
			},
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: false,
			},
		},
		ASID: 789,
	}
	assert.Equal(t, agent0Info, agents[0])

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
		ASID: 123,
	}
	assert.Equal(t, agent1Info, agents[1])

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
		ASID: 456,
	}
	assert.Equal(t, agent2Info, agents[2])
}

func TestAddToUpdateQueue(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

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

	update := &messagespb.AgentUpdateInfo{
		Schema:         schemas,
		ProcessCreated: createdProcesses,
	}

	agentUpdate := controllers.AgentUpdate{
		UpdateInfo: update,
		AgentID:    u,
	}

	agtMgr.ApplyAgentUpdate(&agentUpdate)

	upid1 := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	upid2 := &types.UInt128{
		Low:  uint64(468),
		High: uint64(528280977975),
	}

	pInfos, err := mds.GetProcesses([]*types.UInt128{upid1, upid2})
	assert.Nil(t, err)
	assert.Equal(t, 2, len(pInfos))

	assert.Equal(t, cProcessInfo[0], pInfos[0])
	assert.Equal(t, cProcessInfo[1], pInfos[1])
}

func TestAgentQueueTerminatedProcesses(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

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

	update := &messagespb.AgentUpdateInfo{
		Schema:         schemas,
		ProcessCreated: createdProcesses,
	}

	agentUpdate := controllers.AgentUpdate{
		UpdateInfo: update,
		AgentID:    u,
	}

	agtMgr.ApplyAgentUpdate(&agentUpdate)

	update = &messagespb.AgentUpdateInfo{
		Schema:            schemas,
		ProcessTerminated: terminatedProcesses,
	}

	agentUpdate = controllers.AgentUpdate{
		UpdateInfo: update,
		AgentID:    u,
	}

	agtMgr.ApplyAgentUpdate(&agentUpdate)

	upid1 := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	upid2 := &types.UInt128{
		Low:  uint64(468),
		High: uint64(528280977975),
	}

	pInfos, err := mds.GetProcesses([]*types.UInt128{upid1, upid2})
	assert.Nil(t, err)
	assert.Equal(t, 2, len(pInfos))

	assert.Equal(t, updatedInfo[0], pInfos[0])
	assert.Equal(t, updatedInfo[1], pInfos[1])
}

func TestGetMetadataUpdates(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

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
	pod2 := &metadatapb.Pod{
		Metadata: &metadatapb.ObjectMetadata{
			Name: "efgh",
			UID:  "5678",
		},
		Status: &metadatapb.PodStatus{},
	}

	ep1 := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, ep1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	ep1.Metadata.DeletionTimestampNS = 0

	err := mds.UpdatePod(pod1, false)
	assert.Nil(t, err)
	err = mds.UpdatePod(pod2, false)
	assert.Nil(t, err)

	err = mds.UpdateEndpoints(ep1, false)
	assert.Nil(t, err)

	updates, err := agtMgr.GetMetadataUpdates("")
	assert.Nil(t, err)

	assert.Equal(t, 6, len(updates))

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

	update6 := updates[5].GetServiceUpdate()
	assert.NotNil(t, update6)
	assert.Equal(t, "object_md", update6.Name)
}

func TestAgent_AddToFrontOfAgentQueue(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	u1 := uuid.NewV4()
	upb := utils.ProtoFromUUID(&u1)

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

	_, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)

	updatePb1 := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid1",
				Name: "podName",
			},
		},
	}

	err = agtMgr.AddUpdatesToAgentQueue(u1.String(), []*metadatapb.ResourceUpdate{updatePb1})
	assert.Nil(t, err)

	updatePb2 := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid2",
				Name: "podName",
			},
		},
	}

	err = agtMgr.AddToFrontOfAgentQueue(u1.String(), updatePb2)
	assert.Nil(t, err)

	resp, err := agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 2, len(resp))
	assert.Equal(t, "podUid2", resp[0].GetPodUpdate().UID)
	assert.Equal(t, "podUid1", resp[1].GetPodUpdate().UID)

}

func TestAgent_AddUpdatesToAgentQueue(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

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

	u1 := uuid.NewV4()
	upb := utils.ProtoFromUUID(&u1)

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

	_, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)
	err = agtMgr.AddUpdatesToAgentQueue(u1.String(), []*metadatapb.ResourceUpdate{updatePb1, updatePb2})
	assert.Nil(t, err)

	resp, err := agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 2, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)
	assert.Equal(t, "podUid2", resp[1].GetPodUpdate().UID)
}

func TestAgent_GetFromAgentQueue(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	updatePb := &metadatapb.ResourceUpdate{
		Update: &metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	u1 := uuid.NewV4()
	upb := utils.ProtoFromUUID(&u1)

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

	_, err := agtMgr.RegisterAgent(agentInfo)
	assert.Equal(t, nil, err)
	err = agtMgr.AddUpdatesToAgentQueue(u1.String(), []*metadatapb.ResourceUpdate{updatePb})
	assert.Nil(t, err)

	resp, err := agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)

	resp, err = agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 0, len(resp))
}
