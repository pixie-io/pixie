/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package agent_test

import (
	"os"
	"sync"
	"testing"
	"time"

	"github.com/cockroachdb/pebble"
	"github.com/cockroachdb/pebble/vfs"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/shared/bloomfilterpb"
	k8s_metadatapb "px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/shared/metadatapb"
	types "px.dev/pixie/src/shared/types/gotypes"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/controllers/agent"
	"px.dev/pixie/src/vizier/services/metadata/controllers/testutils"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
	"px.dev/pixie/src/vizier/utils/datastore/pebbledb"
)

func setupManager(t *testing.T) (agent.Store, agent.Manager, *nats.Conn, func()) {
	// Setup NATS.
	nc, natsCleanup := testingutils.MustStartTestNATS(t)

	memFS := vfs.NewMem()
	c, err := pebble.Open("test", &pebble.Options{
		FS: memFS,
	})
	if err != nil {
		natsCleanup()
		t.Fatal("failed to initialize a pebbledb")
		os.Exit(1)
	}

	db := pebbledb.New(c, 3*time.Second)
	ads := agent.NewDatastore(db, 1*time.Minute)

	cleanupFn := func() {
		natsCleanup()
		db.Close()
	}

	createAgentInADS(t, testutils.ExistingAgentUUID, ads, testutils.ExistingAgentInfo)
	createAgentInADS(t, testutils.UnhealthyAgentUUID, ads, testutils.UnhealthyAgentInfo)
	createAgentInADS(t, testutils.UnhealthyKelvinAgentUUID, ads, testutils.UnhealthyKelvinAgentInfo)

	agtMgr := agent.NewManager(ads, nil, nc)

	return ads, agtMgr, nc, cleanupFn
}

func createAgentInADS(t *testing.T, agentID string, ads agent.Store, agentPb string) {
	info := new(agentpb.Agent)
	if err := proto.UnmarshalText(agentPb, info); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for %s", agentID)
	}
	agUUID, err := uuid.FromString(agentID)
	if err != nil {
		t.Fatal("Could not convert uuid from string")
	}
	err = ads.CreateAgent(agUUID, info)
	if err != nil {
		t.Fatal("Could not create agent")
	}

	// Add schema info.
	schema := new(storepb.TableInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfoPB, schema); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	err = ads.UpdateSchemas(agUUID, []*storepb.TableInfo{schema})
	if err != nil {
		t.Fatal("Could not add schema for agent")
	}
}

func TestRegisterAgent(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(u)

	agentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "localhost",
				HostIP:   "127.0.0.4",
				Kernel: &agentpb.KernelVersion{
					Version:  5,
					MajorRev: 19,
					MinorRev: 0,
				},
			},
			AgentID: upb,
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
	}

	now := time.Now().UnixNano()
	id, err := agtMgr.RegisterAgent(agentInfo)
	require.NoError(t, err)
	assert.Equal(t, uint32(1), id)

	// Check that agent exists now.
	agt, err := ads.GetAgent(u)
	require.NoError(t, err)
	assert.NotNil(t, agt)

	assert.Greater(t, agt.LastHeartbeatNS, now)
	assert.Greater(t, agt.CreateTimeNS, now)
	agt.LastHeartbeatNS = 0
	agt.CreateTimeNS = 0
	assert.Equal(t, uint32(1), agt.ASID)
	agt.ASID = 0
	assert.Equal(t, agt, agentInfo)

	hostnameID, err := ads.GetAgentIDForHostnamePair(&agent.HostnameIPPair{"", "127.0.0.4"})
	require.NoError(t, err)
	assert.Equal(t, testutils.NewAgentUUID, hostnameID)
}

func TestRegisterKelvinAgent(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.KelvinAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	upb := utils.ProtoFromUUID(u)

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
	}

	id, err := agtMgr.RegisterAgent(agentInfo)
	require.NoError(t, err)
	assert.Equal(t, uint32(1), id)

	// Check that agent exists now.
	agt, err := ads.GetAgent(u)
	require.NoError(t, err)
	assert.NotNil(t, agt)

	hostnameID, err := ads.GetAgentIDForHostnamePair(&agent.HostnameIPPair{"test", "127.0.0.3"})
	require.NoError(t, err)
	assert.Equal(t, testutils.KelvinAgentUUID, hostnameID)
}

func TestRegisterExistingAgent(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	agentInfo := new(agentpb.Agent)
	if err := proto.UnmarshalText(testutils.ExistingAgentInfo, agentInfo); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for existing agent")
	}

	// Erase the ASID so this looks like a fresh registration not a re-registration.
	info := proto.Clone(agentInfo).(*agentpb.Agent)
	info.ASID = 0

	id, err := agtMgr.RegisterAgent(info)
	require.NoError(t, err)
	// We should get back the known ASID for this agent.
	assert.Equal(t, agentInfo.ASID, id)

	// Check that correct agent info is in ads.
	uid, err := utils.UUIDFromProto(agentInfo.Info.AgentID)
	require.NoError(t, err)
	agt, err := ads.GetAgent(uid)
	require.NoError(t, err)
	assert.Equal(t, agentInfo, agt)
}

func TestReregisterPurgedAgent(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	agentInfo := new(agentpb.Agent)
	if err := proto.UnmarshalText(testutils.PurgedAgentInfo, agentInfo); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for purged agent")
	}

	id, err := agtMgr.RegisterAgent(agentInfo)
	require.NoError(t, err)
	assert.Equal(t, agentInfo.ASID, id)

	// Check that correct agent info is in ads.
	uid, err := utils.UUIDFromProto(agentInfo.Info.AgentID)
	require.NoError(t, err)
	agt, err := ads.GetAgent(uid)
	require.NoError(t, err)
	assert.Equal(t, agentInfo, agt)
}

func TestUpdateHeartbeat(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	now := time.Now().UnixNano()
	err = agtMgr.UpdateHeartbeat(u)
	require.NoError(t, err)

	// Check that correct agent info is in ads.
	agt, err := ads.GetAgent(u)
	require.NoError(t, err)
	assert.NotNil(t, agt)

	assert.Greater(t, agt.LastHeartbeatNS, now)
}

func TestUpdateHeartbeatForNonExistingAgent(t *testing.T) {
	_, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.UpdateHeartbeat(u)
	assert.NotNil(t, err)
}

func TestUpdateAgentDelete(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.UnhealthyAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	u2, err := uuid.FromString(testutils.UnhealthyKelvinAgentUUID)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	err = agtMgr.DeleteAgent(u)
	require.NoError(t, err)
	err = agtMgr.DeleteAgent(u2)
	require.NoError(t, err)

	agents, err := ads.GetAgents()
	require.NoError(t, err)
	assert.Len(t, agents, 1)

	agt, err := ads.GetAgent(u)
	require.NoError(t, err)
	assert.Nil(t, agt)

	hostnameID, err := ads.GetAgentIDForHostnamePair(&agent.HostnameIPPair{"", "127.0.0.2"})
	require.NoError(t, err)
	assert.Equal(t, "", hostnameID)
}

func TestGetActiveAgents(t *testing.T) {
	_, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	agents, err := agtMgr.GetActiveAgents()
	require.NoError(t, err)

	assert.Len(t, agents, 3)

	agentInfo := new(agentpb.Agent)
	if err := proto.UnmarshalText(testutils.ExistingAgentInfo, agentInfo); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for existing agent")
	}
	assert.Contains(t, agents, agentInfo)

	agentInfo = new(agentpb.Agent)
	if err = proto.UnmarshalText(testutils.UnhealthyAgentInfo, agentInfo); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for unhealthy agent")
	}
	assert.Contains(t, agents, agentInfo)

	agentInfo = new(agentpb.Agent)
	if err = proto.UnmarshalText(testutils.UnhealthyKelvinAgentInfo, agentInfo); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for unhealthy kelvin agent")
	}
	assert.Contains(t, agents, agentInfo)
}

func TestApplyUpdates(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*storepb.TableInfo, 1)

	schema1 := new(storepb.TableInfo)
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

	agentUpdate := agent.Update{
		UpdateInfo: update,
		AgentID:    u,
	}

	err = agtMgr.ApplyAgentUpdate(&agentUpdate)
	require.NoError(t, err)

	upid1 := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	upid2 := &types.UInt128{
		Low:  uint64(468),
		High: uint64(528280977975),
	}

	pInfos, err := ads.GetProcesses([]*types.UInt128{upid1, upid2})
	require.NoError(t, err)
	assert.Len(t, pInfos, 2)

	assert.Equal(t, cProcessInfo[0], pInfos[0])
	assert.Equal(t, cProcessInfo[1], pInfos[1])

	dataInfos, err := ads.GetAgentsDataInfo()
	require.NoError(t, err)
	assert.NotNil(t, dataInfos)
	dataInfo, present := dataInfos[u]
	assert.True(t, present)
	assert.Equal(t, dataInfo, expectedDataInfo)
}

func TestApplyUpdatesDeleted(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.NewAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*storepb.TableInfo, 1)

	schema1 := new(storepb.TableInfo)
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

	agentUpdate := agent.Update{
		UpdateInfo: update,
		AgentID:    u,
	}

	err = agtMgr.ApplyAgentUpdate(&agentUpdate)
	require.NoError(t, err)

	upid1 := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	upid2 := &types.UInt128{
		Low:  uint64(468),
		High: uint64(528280977975),
	}

	pInfos, err := ads.GetProcesses([]*types.UInt128{upid1, upid2})
	require.NoError(t, err)
	assert.Len(t, pInfos, 2)

	assert.Nil(t, pInfos[0])
	assert.Nil(t, pInfos[1])

	dataInfos, err := ads.GetAgentsDataInfo()
	require.NoError(t, err)
	assert.NotNil(t, dataInfos)
	_, present := dataInfos[u]
	assert.False(t, present)
}

func TestAgentTerminatedProcesses(t *testing.T) {
	ads, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	u, err := uuid.FromString(testutils.ExistingAgentUUID)
	if err != nil {
		t.Fatal("Could not parse UUID from string.")
	}

	schemas := make([]*storepb.TableInfo, 1)

	schema1 := new(storepb.TableInfo)
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

	agentUpdate := agent.Update{
		UpdateInfo: update,
		AgentID:    u,
	}

	err = agtMgr.ApplyAgentUpdate(&agentUpdate)
	require.NoError(t, err)

	update = &messagespb.AgentUpdateInfo{
		Schema:            schemas,
		ProcessTerminated: terminatedProcesses,
	}

	agentUpdate = agent.Update{
		UpdateInfo: update,
		AgentID:    u,
	}

	err = agtMgr.ApplyAgentUpdate(&agentUpdate)
	require.NoError(t, err)

	upid1 := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	upid2 := &types.UInt128{
		Low:  uint64(468),
		High: uint64(528280977975),
	}

	pInfos, err := ads.GetProcesses([]*types.UInt128{upid1, upid2})
	require.NoError(t, err)
	assert.Len(t, pInfos, 2)

	assert.Equal(t, updatedInfo[0], pInfos[0])
	assert.Equal(t, updatedInfo[1], pInfos[1])
}

func TestAgent_GetAgentUpdate(t *testing.T) {
	_, agtMgr, _, cleanup := setupManager(t)
	defer cleanup()

	agUUID0, err := uuid.FromString(testutils.UnhealthyKelvinAgentUUID)
	require.NoError(t, err)
	agUUID1, err := uuid.FromString(testutils.UnhealthyAgentUUID)
	require.NoError(t, err)
	agUUID2, err := uuid.FromString(testutils.ExistingAgentUUID)
	require.NoError(t, err)
	agUUID3, err := uuid.FromString(testutils.NewAgentUUID)
	require.NoError(t, err)

	// Read the initial agent state.
	cursor := agtMgr.NewAgentUpdateCursor()
	updates, schema, err := agtMgr.GetAgentUpdates(cursor)
	require.NoError(t, err)
	assert.Len(t, updates, 3)
	assert.Equal(t, agUUID0, utils.UUIDFromProtoOrNil(updates[0].AgentID))
	assert.NotNil(t, updates[0].GetAgent())
	assert.Equal(t, agUUID2, utils.UUIDFromProtoOrNil(updates[1].AgentID))
	assert.NotNil(t, updates[1].GetAgent())
	assert.Equal(t, agUUID1, utils.UUIDFromProtoOrNil(updates[2].AgentID))
	assert.NotNil(t, updates[2].GetAgent())
	assert.Len(t, schema.Tables, 1)
	assert.Len(t, schema.TableNameToAgentIDs, 1)
	assert.Len(t, schema.TableNameToAgentIDs["a_table"].AgentID, 3)

	newAgentInfo := &agentpb.Agent{
		Info: &agentpb.AgentInfo{
			HostInfo: &agentpb.HostInfo{
				Hostname: "localhost",
				HostIP:   "127.0.0.7",
			},
			AgentID: utils.ProtoFromUUID(agUUID3),
			Capabilities: &agentpb.AgentCapabilities{
				CollectsData: true,
			},
		},
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

	// Register a new agt.
	_, err = agtMgr.RegisterAgent(newAgentInfo)
	require.NoError(t, err)

	schema2 := new(storepb.TableInfo)
	if err := proto.UnmarshalText(testutils.SchemaInfo2PB, schema2); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	// Update data info on agent #2.
	agentUpdate := agent.Update{
		UpdateInfo: &messagespb.AgentUpdateInfo{
			Schema:           []*storepb.TableInfo{schema2},
			ProcessCreated:   []*k8s_metadatapb.ProcessCreated{},
			Data:             oldAgentDataInfo,
			DoesUpdateSchema: true,
		},
		AgentID: agUUID2,
	}
	err = agtMgr.ApplyAgentUpdate(&agentUpdate)
	require.NoError(t, err)

	// Check results of first call to GetAgentUpdates.
	updates, schema, err = agtMgr.GetAgentUpdates(cursor)
	require.NoError(t, err)
	assert.Len(t, updates, 2)
	assert.Equal(t, agUUID3, utils.UUIDFromProtoOrNil(updates[0].AgentID))
	assert.Equal(t, newAgentInfo.Info, updates[0].GetAgent().Info)
	assert.Equal(t, agUUID2, utils.UUIDFromProtoOrNil(updates[1].AgentID))
	assert.Equal(t, oldAgentDataInfo, updates[1].GetDataInfo())
	assert.Len(t, schema.Tables, 2)

	// Update the heartbeat of an agt.
	err = agtMgr.UpdateHeartbeat(agUUID2)
	require.NoError(t, err)

	// Now expire it
	err = agtMgr.DeleteAgent(agUUID0)
	require.NoError(t, err)
	err = agtMgr.DeleteAgent(agUUID1)
	require.NoError(t, err)

	// Check results of second call to GetAgentUpdates.
	updates, schema, err = agtMgr.GetAgentUpdates(cursor)
	require.NoError(t, err)
	assert.Nil(t, schema)
	assert.Len(t, updates, 3)
	assert.Equal(t, agUUID2, utils.UUIDFromProtoOrNil(updates[0].AgentID))
	assert.NotNil(t, updates[0].GetAgent())
	assert.Equal(t, agUUID0, utils.UUIDFromProtoOrNil(updates[1].AgentID))
	assert.True(t, updates[1].GetDeleted())
	assert.Equal(t, agUUID1, utils.UUIDFromProtoOrNil(updates[2].AgentID))
	assert.True(t, updates[2].GetDeleted())

	agtMgr.DeleteAgentUpdateCursor(cursor)
	// This should throw an error because the cursor has been deleted.
	_, _, err = agtMgr.GetAgentUpdates(cursor)
	assert.NotNil(t, err)
}

func TestAgent_UpdateConfig(t *testing.T) {
	_, agtMgr, nc, cleanup := setupManager(t)
	defer cleanup()

	var wg sync.WaitGroup
	wg.Add(1)

	adsub, err := nc.Subscribe("Agent/"+testutils.ExistingAgentUUID, func(msg *nats.Msg) {
		vzMsg := &messagespb.VizierMessage{}
		err := proto.Unmarshal(msg.Data, vzMsg)
		require.NoError(t, err)
		req := vzMsg.GetConfigUpdateMessage().GetConfigUpdateRequest()
		assert.NotNil(t, req)
		assert.Equal(t, "gprof", req.Key)
		assert.Equal(t, "true", req.Value)
		wg.Done()
	})
	require.NoError(t, err)
	defer func() {
		err := adsub.Unsubscribe()
		require.NoError(t, err)
	}()

	err = agtMgr.UpdateConfig("pl", "pem-existing", "gprof", "true")
	require.NoError(t, err)

	defer wg.Wait()
}
