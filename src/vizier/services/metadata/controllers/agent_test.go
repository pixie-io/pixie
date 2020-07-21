package controllers_test

import (
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	distributedpb "pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	bloomfilterpb "pixielabs.ai/pixielabs/src/shared/bloomfilterpb"
	k8s_metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	metadatapb "pixielabs.ai/pixielabs/src/shared/metadatapb"
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

func setupAgentManager(t *testing.T) (controllers.MetadataStore, controllers.AgentManager) {
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

func createAgentInMDS(t *testing.T, agentID string, mds controllers.MetadataStore, agentPb string) {
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
	schema := new(k8s_metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	err = mds.UpdateSchemas(agUUID, []*k8s_metadatapb.SchemaInfo{schema})
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
				HostIP:   "127.0.0.4",
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

	hostnameID, err := mds.GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"", "127.0.0.4"})
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
				HostIP:   "127.0.0.3",
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

	hostnameID, err := mds.GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"test", "127.0.0.3"})
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
				HostIP:   "127.0.0.1",
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

	hostnameID, err := mds.GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"", "127.0.0.1"})
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
				HostIP:   "127.0.0.1",
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

	hostnameID, err := mds.GetAgentIDForHostnamePair(&controllers.HostnameIPPair{"", "127.0.0.2"})
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
				HostIP:   "127.0.0.3",
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
				HostIP:   "127.0.0.1",
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
				HostIP:   "127.0.0.2",
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

	schemas := make([]*k8s_metadatapb.SchemaInfo, 1)

	schema1 := new(k8s_metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	createdProcesses := make([]*k8s_metadatapb.ProcessCreated, 2)

	cp1 := new(k8s_metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(testutils.ProcessCreated1PB, cp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[0] = cp1
	cp2 := new(k8s_metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(testutils.ProcessCreated2PB, cp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[1] = cp2

	cProcessInfo := make([]*k8s_metadatapb.ProcessInfo, 2)

	cpi1 := new(k8s_metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo1PB, cpi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	cProcessInfo[0] = cpi1
	cpi2 := new(k8s_metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo2PB, cpi2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	cProcessInfo[1] = cpi2

	expectedDataInfo := &messagespb.AgentDataInfo{
		MetadataInfo: &distributedpb.MetadataInfo{
			MetadataFields: []metadatapb.MetadataType{
				metadatapb.CONTAINER_ID,
				metadatapb.POD_NAME,
			},
			Filter: &distributedpb.MetadataInfo_XXHash64BloomFilter{
				XXHash64BloomFilter: &bloomfilterpb.XXHash64BloomFilter{
					Data:      []byte("1234"),
					NumHashes: 4,
				},
			},
		},
	}

	update := &messagespb.AgentUpdateInfo{
		Schema:         schemas,
		ProcessCreated: createdProcesses,
		Data:           expectedDataInfo,
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

	dataInfos, err := mds.GetAgentsDataInfo()
	assert.Nil(t, err)
	assert.NotNil(t, dataInfos)
	dataInfo, present := dataInfos[u]
	assert.True(t, present)
	assert.Equal(t, dataInfo, expectedDataInfo)
}

func TestAgentQueueTerminatedProcesses(t *testing.T) {
	mds, agtMgr := setupAgentManager(t)

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*k8s_metadatapb.SchemaInfo, 1)

	schema1 := new(k8s_metadatapb.SchemaInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	schemas[0] = schema1

	terminatedProcesses := make([]*k8s_metadatapb.ProcessTerminated, 2)

	tp1 := new(k8s_metadatapb.ProcessTerminated)
	if err := proto.UnmarshalText(testutils.ProcessTerminated1PB, tp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	terminatedProcesses[0] = tp1
	tp2 := new(k8s_metadatapb.ProcessTerminated)
	if err := proto.UnmarshalText(testutils.ProcessTerminated2PB, tp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	terminatedProcesses[1] = tp2

	createdProcesses := make([]*k8s_metadatapb.ProcessCreated, 2)

	cp1 := new(k8s_metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(testutils.ProcessCreated1PB, cp1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[0] = cp1
	cp2 := new(k8s_metadatapb.ProcessCreated)
	if err := proto.UnmarshalText(testutils.ProcessCreated2PB, cp2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	createdProcesses[1] = cp2

	updatedInfo := make([]*k8s_metadatapb.ProcessInfo, 2)
	upi1 := new(k8s_metadatapb.ProcessInfo)
	if err := proto.UnmarshalText(testutils.ProcessInfo1PB, upi1); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	upi1.StopTimestampNS = 6
	updatedInfo[0] = upi1
	upi2 := new(k8s_metadatapb.ProcessInfo)
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

func TestAgent_AddToFrontOfAgentQueue(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	u1 := uuid.NewV4()
	upb := utils.ProtoFromUUID(&u1)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "localhost",
				HostIP:   "127.0.0.1",
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

	updatePb1 := &k8s_metadatapb.ResourceUpdate{
		Update: &k8s_metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &k8s_metadatapb.PodUpdate{
				UID:  "podUid1",
				Name: "podName",
			},
		},
	}

	err = agtMgr.AddUpdatesToAgentQueue(u1.String(), []*k8s_metadatapb.ResourceUpdate{updatePb1})
	assert.Nil(t, err)

	updatePb2 := &k8s_metadatapb.ResourceUpdate{
		Update: &k8s_metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &k8s_metadatapb.PodUpdate{
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

	updatePb1 := &k8s_metadatapb.ResourceUpdate{
		Update: &k8s_metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &k8s_metadatapb.PodUpdate{
				UID:  "podUid",
				Name: "podName",
			},
		},
	}

	updatePb2 := &k8s_metadatapb.ResourceUpdate{
		Update: &k8s_metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &k8s_metadatapb.PodUpdate{
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
				HostIP:   "127.0.0.1",
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
	err = agtMgr.AddUpdatesToAgentQueue(u1.String(), []*k8s_metadatapb.ResourceUpdate{updatePb1, updatePb2})
	assert.Nil(t, err)

	resp, err := agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 2, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)
	assert.Equal(t, "podUid2", resp[1].GetPodUpdate().UID)
}

func TestAgent_GetFromAgentQueue(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	updatePb := &k8s_metadatapb.ResourceUpdate{
		Update: &k8s_metadatapb.ResourceUpdate_PodUpdate{
			PodUpdate: &k8s_metadatapb.PodUpdate{
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
				HostIP:   "127.0.0.1",
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
	err = agtMgr.AddUpdatesToAgentQueue(u1.String(), []*k8s_metadatapb.ResourceUpdate{updatePb})
	assert.Nil(t, err)

	resp, err := agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp))
	assert.Equal(t, "podUid", resp[0].GetPodUpdate().UID)

	resp, err = agtMgr.GetFromAgentQueue(u1.String())
	assert.Nil(t, err)
	assert.Equal(t, 0, len(resp))
}

func TestAgent_HandleUpdate(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	update := &controllers.UpdateMessage{
		Hostnames:    []*controllers.HostnameIPPair{&controllers.HostnameIPPair{"", "127.0.0.1"}},
		NodeSpecific: false,
		Message: &k8s_metadatapb.ResourceUpdate{
			Update: &k8s_metadatapb.ResourceUpdate_PodUpdate{
				PodUpdate: &k8s_metadatapb.PodUpdate{
					UID:  "podUid1",
					Name: "podName",
				},
			},
		},
	}
	agtMgr.HandleUpdate(update)

	resp, err := agtMgr.GetFromAgentQueue(testutils.ExistingAgentUUID)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp))
	assert.Equal(t, "podUid1", resp[0].GetPodUpdate().UID)

	resp, err = agtMgr.GetFromAgentQueue(testutils.UnhealthyKelvinAgentUUID)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(resp))
	assert.Equal(t, "podUid1", resp[0].GetPodUpdate().UID)

	resp, err = agtMgr.GetFromAgentQueue(testutils.UnhealthyAgentUUID)
	assert.Nil(t, err)
	assert.Equal(t, 0, len(resp))
}

func TestAgent_GetAgentUpdate(t *testing.T) {
	_, agtMgr := setupAgentManager(t)

	agUUID0, err := uuid.FromString(testutils.UnhealthyKelvinAgentUUID)
	assert.Nil(t, err)
	agUUID1, err := uuid.FromString(testutils.UnhealthyAgentUUID)
	assert.Nil(t, err)
	agUUID2, err := uuid.FromString(testutils.ExistingAgentUUID)
	assert.Nil(t, err)
	agUUID3, err := uuid.FromString(testutils.NewAgentUUID)
	assert.Nil(t, err)

	newAgentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "localhost",
				HostIP:   "127.0.0.7",
			},
			AgentID: utils.ProtoFromUUID(&agUUID3),
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
		LastHeartbeatNS: 3,
		CreateTimeNS:    4,
	}

	oldAgentDataInfo := &messagespb.AgentDataInfo{
		MetadataInfo: &distributedpb.MetadataInfo{
			MetadataFields: []metadatapb.MetadataType{
				metadatapb.CONTAINER_ID,
				metadatapb.POD_NAME,
			},
			Filter: &distributedpb.MetadataInfo_XXHash64BloomFilter{
				XXHash64BloomFilter: &bloomfilterpb.XXHash64BloomFilter{
					Data:      []byte("1234"),
					NumHashes: 4,
				},
			},
		},
	}

	// Register a new agent.
	_, err = agtMgr.RegisterAgent(newAgentInfo)
	assert.Equal(t, nil, err)

	// Update data info on agent #2.
	agentUpdate := controllers.AgentUpdate{
		UpdateInfo: &messagespb.AgentUpdateInfo{
			Schema:         []*k8s_metadatapb.SchemaInfo{},
			ProcessCreated: []*k8s_metadatapb.ProcessCreated{},
			Data:           oldAgentDataInfo,
		},
		AgentID: agUUID2,
	}
	agtMgr.ApplyAgentUpdate(&agentUpdate)

	// Check results of first call to GetAgentUpdates.
	agents, agentsDataInfo, deletedAgents, err := agtMgr.GetAgentUpdates()
	assert.Nil(t, err)
	assert.NotNil(t, agents)
	assert.NotNil(t, agentsDataInfo)
	assert.Nil(t, deletedAgents)

	// agents array
	assert.Equal(t, 1, len(agents))
	assert.NotNil(t, agents[0].Info.AgentID)
	assert.Equal(t, utils.ProtoFromUUID(&agUUID3), agents[0].Info.AgentID)

	// agents data info
	assert.Equal(t, 1, len(agentsDataInfo))
	assert.Equal(t, oldAgentDataInfo, agentsDataInfo[agUUID2])

	// Update the heartbeat of an agent.
	err = agtMgr.UpdateHeartbeat(agUUID2)
	assert.Nil(t, err)

	// Update the data info on an agent that we are about to expire.
	badAgentUpdate := controllers.AgentUpdate{
		UpdateInfo: &messagespb.AgentUpdateInfo{
			Schema:         []*k8s_metadatapb.SchemaInfo{},
			ProcessCreated: []*k8s_metadatapb.ProcessCreated{},
			Data:           oldAgentDataInfo,
		},
		AgentID: agUUID1,
	}
	agtMgr.ApplyAgentUpdate(&badAgentUpdate)
	// Now expire it
	err = agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	// Check results of second call to GetAgentUpdates.
	agents, agentsDataInfo, deletedAgents, err = agtMgr.GetAgentUpdates()
	assert.Nil(t, err)
	assert.NotNil(t, agents)
	assert.NotNil(t, agentsDataInfo)
	assert.NotNil(t, deletedAgents)

	// agents array
	ag2Info := &agentpb.Agent{}
	err = proto.UnmarshalText(testutils.ExistingAgentInfo, ag2Info)
	assert.Nil(t, err)
	assert.Equal(t, 1, len(agents))
	assert.Equal(t, int64(testutils.ClockNowNS), agents[0].LastHeartbeatNS)

	// agents data info
	assert.Equal(t, 0, len(agentsDataInfo))

	// deleted agents array
	assert.Equal(t, 2, len(deletedAgents))
	hasUUID0 := false
	hasUUID1 := false
	for _, agUUID := range deletedAgents {
		if agUUID == agUUID0 {
			hasUUID0 = true
		}
		if agUUID == agUUID1 {
			hasUUID1 = true
		}
	}
	assert.True(t, hasUUID0)
	assert.True(t, hasUUID1)
}
