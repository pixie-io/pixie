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

package tracker_test

import (
	"testing"

	"github.com/gofrs/uuid"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/shared/bloomfilterpb"
	sharedmetadatapb "px.dev/pixie/src/shared/metadatapb"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/messages/messagespb"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/services/query_broker/tracker"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
)

func makeTestAgentIDs(t *testing.T) []*uuidpb.UUID {
	agent1IDStr := "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u1, err := uuid.FromString(agent1IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agent2IDStr := "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
	u2, err := uuid.FromString(agent2IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}

	agent3IDStr := "61123ced-1de9-4ab1-ae6a-0ba08c8c676c"
	u3, err := uuid.FromString(agent3IDStr)
	if err != nil {
		t.Fatal("Could not generate UUID.")
	}
	return []*uuidpb.UUID{
		utils.ProtoFromUUID(u1),
		utils.ProtoFromUUID(u2),
		utils.ProtoFromUUID(u3),
	}
}

func makeTestSchema(t *testing.T) []*distributedpb.SchemaInfo {
	ids := makeTestAgentIDs(t)

	return []*distributedpb.SchemaInfo{
		{
			Name: "table1",
			Relation: &schemapb.Relation{
				Columns: []*schemapb.Relation_ColumnInfo{
					{
						ColumnName: "foo",
					},
				},
			},
			AgentList: []*uuidpb.UUID{
				ids[0],
				ids[2],
			},
		},
	}
}

func makeTestAgents(t *testing.T) []*agentpb.Agent {
	ids := makeTestAgentIDs(t)

	return []*agentpb.Agent{
		{
			LastHeartbeatNS: 10,
			CreateTimeNS:    5,
			Info: &agentpb.AgentInfo{
				AgentID: ids[0],
				HostInfo: &agentpb.HostInfo{
					Hostname: "test_pem1",
					HostIP:   "127.0.0.1",
				},
				Capabilities: &agentpb.AgentCapabilities{
					CollectsData: true,
				},
				IPAddress: "127.0.1.2",
			},
			ASID: 123,
		},
		{
			LastHeartbeatNS: 20,
			CreateTimeNS:    0,
			Info: &agentpb.AgentInfo{
				AgentID: ids[1],
				HostInfo: &agentpb.HostInfo{
					Hostname: "test_kelvin",
					HostIP:   "127.0.0.1",
				},
				Capabilities: &agentpb.AgentCapabilities{
					CollectsData: false,
				},
				IPAddress: "127.0.1.3",
			},
			ASID: 456,
		},
		{
			LastHeartbeatNS: 30,
			CreateTimeNS:    0,
			Info: &agentpb.AgentInfo{
				AgentID: ids[2],
				HostInfo: &agentpb.HostInfo{
					Hostname: "test_pem2",
					HostIP:   "127.0.0.1",
				},
				Capabilities: &agentpb.AgentCapabilities{
					CollectsData: true,
				},
				IPAddress: "127.0.1.4",
			},
			ASID: 789,
		},
	}
}

func makeTestAgentDataInfo() []*messagespb.AgentDataInfo {
	return []*messagespb.AgentDataInfo{
		{
			MetadataInfo: &distributedpb.MetadataInfo{
				MetadataFields: []sharedmetadatapb.MetadataType{
					sharedmetadatapb.CONTAINER_ID,
					sharedmetadatapb.POD_NAME,
				},
				Filter: &distributedpb.MetadataInfo_XXHash64BloomFilter{
					XXHash64BloomFilter: &bloomfilterpb.XXHash64BloomFilter{
						Data:      []byte("1234"),
						NumHashes: 4,
					},
				},
			},
		},
		{
			MetadataInfo: &distributedpb.MetadataInfo{
				MetadataFields: []sharedmetadatapb.MetadataType{
					sharedmetadatapb.CONTAINER_ID,
					sharedmetadatapb.POD_NAME,
				},
				Filter: &distributedpb.MetadataInfo_XXHash64BloomFilter{
					XXHash64BloomFilter: &bloomfilterpb.XXHash64BloomFilter{
						Data:      []byte("5678"),
						NumHashes: 3,
					},
				},
			},
		},
	}
}

func TestAgentsInfo_UpdateAgentsInfo(t *testing.T) {
	// This test tries out various agent state updates together and in a row to make sure
	// that they all interact with each other properly.
	viper.Set("pod_namespace", "pl")
	testSchema := makeTestSchema(t)
	uuidpbs := makeTestAgentIDs(t)
	var uuids []uuid.UUID
	for _, uuidpb := range uuidpbs {
		uuids = append(uuids, utils.UUIDFromProtoOrNil(uuidpb))
	}

	agents := makeTestAgents(t)
	agentDataInfos := makeTestAgentDataInfo()

	agentsInfo := tracker.NewAgentsInfo()
	assert.NotNil(t, agentsInfo)

	// Initial conditions
	assert.Equal(t, 0, len(agentsInfo.DistributedState().SchemaInfo))
	assert.Equal(t, 0, len(agentsInfo.DistributedState().CarnotInfo))

	updates1 := []*metadatapb.AgentUpdate{
		{
			AgentID: uuidpbs[0],
			Update: &metadatapb.AgentUpdate_Agent{
				Agent: agents[0],
			},
		},
		{
			AgentID: uuidpbs[0],
			Update: &metadatapb.AgentUpdate_DataInfo{
				DataInfo: agentDataInfos[0],
			},
		},
		{
			AgentID: uuidpbs[1],
			Update: &metadatapb.AgentUpdate_Agent{
				Agent: agents[1],
			},
		},
	}

	// Update schema
	// Add agents 1,2 and add table metadata for 1 agent.
	err := agentsInfo.UpdateAgentsInfo(&metadatapb.AgentUpdatesResponse{
		AgentUpdates:        updates1,
		AgentSchemas:        testSchema,
		AgentSchemasUpdated: true,
	})

	require.NoError(t, err)
	// Updates shouldn't have been propagated yet until the end of the version.
	assert.Equal(t, 0, len(agentsInfo.DistributedState().SchemaInfo))
	assert.Equal(t, 0, len(agentsInfo.DistributedState().CarnotInfo))

	err = agentsInfo.UpdateAgentsInfo(&metadatapb.AgentUpdatesResponse{
		AgentUpdates:        updates1,
		AgentSchemas:        testSchema,
		AgentSchemasUpdated: true,
		EndOfVersion:        true,
	})
	require.NoError(t, err)
	assert.Equal(t, testSchema, agentsInfo.DistributedState().SchemaInfo)

	expectedPEM1Info := &distributedpb.CarnotInfo{
		QueryBrokerAddress:   "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c",
		AgentID:              uuidpbs[0],
		HasGRPCServer:        false,
		GRPCAddress:          "",
		HasDataStore:         true,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		ASID:                 123,
		MetadataInfo:         agentDataInfos[0].MetadataInfo,
	}

	expectedKelvinInfo := &distributedpb.CarnotInfo{
		QueryBrokerAddress:   "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c",
		AgentID:              uuidpbs[1],
		HasGRPCServer:        true,
		GRPCAddress:          "127.0.1.3",
		HasDataStore:         false,
		ProcessesData:        true,
		AcceptsRemoteSources: true,
		ASID:                 456,
		SSLTargetName:        "kelvin.pl.svc",
	}

	agentsMap := make(map[uuid.UUID]*distributedpb.CarnotInfo)
	for _, carnotInfo := range agentsInfo.DistributedState().CarnotInfo {
		agentsMap[utils.UUIDFromProtoOrNil(carnotInfo.AgentID)] = carnotInfo
	}

	// Expect that the agents should be as expected.
	assert.Equal(t, 2, len(agentsMap))
	assert.Equal(t, expectedPEM1Info, agentsMap[uuids[0]])
	assert.Equal(t, expectedKelvinInfo, agentsMap[uuids[1]])

	// Update agent 1, and add table metadata for another agent,
	// create an agent, and delete an agent.
	newHeartbeat := int64(20)
	agent1Update := &agentpb.Agent{
		LastHeartbeatNS: newHeartbeat,
		CreateTimeNS:    agents[0].CreateTimeNS,
		Info:            agents[0].Info,
		ASID:            agents[0].ASID,
	}
	expectedPEM1Info.MetadataInfo = agentDataInfos[1].MetadataInfo

	expectedPEM2Info := &distributedpb.CarnotInfo{
		QueryBrokerAddress:   "61123ced-1de9-4ab1-ae6a-0ba08c8c676c",
		AgentID:              uuidpbs[2],
		HasGRPCServer:        false,
		GRPCAddress:          "",
		HasDataStore:         true,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		ASID:                 789,
	}

	updates2 := []*metadatapb.AgentUpdate{
		{
			AgentID: uuidpbs[0],
			Update: &metadatapb.AgentUpdate_Agent{
				Agent: agent1Update,
			},
		},
		{
			AgentID: uuidpbs[0],
			Update: &metadatapb.AgentUpdate_DataInfo{
				DataInfo: agentDataInfos[1],
			},
		},
		{
			AgentID: uuidpbs[2],
			Update: &metadatapb.AgentUpdate_Agent{
				Agent: agents[2],
			},
		},
		{
			AgentID: uuidpbs[1],
			Update: &metadatapb.AgentUpdate_Deleted{
				Deleted: true,
			},
		},
	}

	err = agentsInfo.UpdateAgentsInfo(&metadatapb.AgentUpdatesResponse{
		AgentUpdates:        updates2,
		AgentSchemas:        nil,
		AgentSchemasUpdated: false,
		EndOfVersion:        true,
	})
	require.NoError(t, err)
	agentsMap = make(map[uuid.UUID]*distributedpb.CarnotInfo)
	for _, carnotInfo := range agentsInfo.DistributedState().CarnotInfo {
		agentsMap[utils.UUIDFromProtoOrNil(carnotInfo.AgentID)] = carnotInfo
	}

	// Schema should be unchanged.
	assert.Equal(t, testSchema, agentsInfo.DistributedState().SchemaInfo)

	assert.Equal(t, 2, len(agentsMap))
	// Agent 1 should be updated.
	assert.Equal(t, expectedPEM1Info, agentsMap[uuids[0]])
	// Agent 3 should be created.
	assert.Equal(t, expectedPEM2Info, agentsMap[uuids[2]])

	// Test the case where the schema is updated to be fully empty.
	err = agentsInfo.UpdateAgentsInfo(&metadatapb.AgentUpdatesResponse{
		AgentUpdates:        nil,
		AgentSchemas:        []*distributedpb.SchemaInfo{},
		AgentSchemasUpdated: true,
		EndOfVersion:        true,
	})
	require.NoError(t, err)
	assert.Equal(t, 0, len(agentsInfo.DistributedState().SchemaInfo))
}
