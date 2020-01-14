package controllers_test

import (
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
	"pixielabs.ai/pixielabs/src/shared/types"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/kvstore/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/testutils"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

func TestKVMetadataStore_GetClusterCIDR(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	assert.Equal(t, "", mds.GetClusterCIDR())

	mds.SetClusterInfo(controllers.ClusterInfo{
		CIDR: "1.0.0.24/14",
	})
	assert.Equal(t, "1.0.0.24/14", mds.GetClusterCIDR())
}

// createAgent manually creates the agent keys/values in the cache.
func createAgent(t *testing.T, c *kvstore.Cache, agentID string, agentPb string) {
	info := new(agentpb.Agent)
	if err := proto.UnmarshalText(agentPb, info); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf for %s", agentID)
	}
	i, err := info.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal agentData pb.")
	}
	c.Set("/agent/"+agentID, string(i))
	c.Set("/hostname/"+info.Info.HostInfo.Hostname+"/agent", agentID)
	if !info.Info.Capabilities.CollectsData {
		c.Set("/kelvin/"+agentID, agentID)
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
	c.Set("/agents/"+agentID+"/schema", string(s))
}

func TestKVMetadataStore_GetAgent(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		Get("/agent/"+testutils.NewAgentUUID).
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Add an existing agent in the cache.
	createAgent(t, c, testutils.ExistingAgentUUID, testutils.ExistingAgentInfo)
	existingAgUUID, err := uuid.FromString(testutils.ExistingAgentUUID)
	assert.Nil(t, err)

	// Get existing agent.
	agent, err := mds.GetAgent(existingAgUUID)
	assert.Nil(t, err)
	assert.Equal(t, existingAgUUID, utils.UUIDFromProtoOrNil(agent.Info.AgentID))
	assert.Equal(t, "testhost", agent.Info.HostInfo.Hostname)

	// Get non-existent agent.
	newAgUUID, err := uuid.FromString(testutils.NewAgentUUID)
	assert.Nil(t, err)
	agent, err = mds.GetAgent(newAgUUID)
	assert.Nil(t, err)
	assert.Nil(t, agent)
}

func TestKVMetadataStore_GetAgentIDForHostname(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		Get("/hostname/blah/agent").
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Add an existing agent in the cache.
	createAgent(t, c, testutils.ExistingAgentUUID, testutils.ExistingAgentInfo)

	// Get existing agent hostname.
	agent, err := mds.GetAgentIDForHostname("testhost")
	assert.Nil(t, err)
	assert.Equal(t, testutils.ExistingAgentUUID, agent)

	// Get non-existent agent hostname.
	agent, err = mds.GetAgentIDForHostname("blah")
	assert.Nil(t, err)
	assert.Equal(t, "", agent)
}

func TestKVMetadataStore_DeleteAgent(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		DeleteWithPrefix("/agents/" + testutils.ExistingAgentUUID + "/schema").
		Return(nil)
	mockDs.
		EXPECT().
		Get("/agent/"+testutils.NewAgentUUID).
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Add an existing agent in the cache.
	createAgent(t, c, testutils.ExistingAgentUUID, testutils.ExistingAgentInfo)
	existingAgUUID, err := uuid.FromString(testutils.ExistingAgentUUID)
	assert.Nil(t, err)

	// Delete existing PEM.
	err = mds.DeleteAgent(existingAgUUID)
	assert.Nil(t, err)
	hostnameVal, _ := c.Get("/hostname/testhost/agent")
	assert.Equal(t, []byte(""), hostnameVal)
	agentVal, _ := c.Get("/agent/" + testutils.ExistingAgentUUID)
	assert.Equal(t, []byte(""), agentVal)

	// Delete non-existent agent.
	newAgUUID, err := uuid.FromString(testutils.NewAgentUUID)
	assert.Nil(t, err)
	err = mds.DeleteAgent(newAgUUID)
	assert.Nil(t, err)
}

func TestKVMetadataStore_UpdateAgent(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Add an existing agent in the cache.
	createAgent(t, c, testutils.ExistingAgentUUID, testutils.ExistingAgentInfo)
	existingAgUUID, err := uuid.FromString(testutils.ExistingAgentUUID)
	assert.Nil(t, err)

	info := new(agentpb.Agent)
	if err := proto.UnmarshalText(testutils.ExistingAgentInfo, info); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf")
	}
	info.Info.HostInfo.Hostname = "newName"
	err = mds.UpdateAgent(existingAgUUID, info)
	assert.Nil(t, err)
	agent, err := mds.GetAgent(existingAgUUID)
	assert.Nil(t, err)
	assert.Equal(t, existingAgUUID, utils.UUIDFromProtoOrNil(agent.Info.AgentID))
	assert.Equal(t, "newName", agent.Info.HostInfo.Hostname)
}

func TestKVMetadataStore_CreateAgent(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	existingAgUUID, err := uuid.FromString(testutils.ExistingAgentUUID)
	assert.Nil(t, err)

	info := new(agentpb.Agent)
	if err := proto.UnmarshalText(testutils.ExistingAgentInfo, info); err != nil {
		t.Fatalf("Cannot Unmarshal protobuf")
	}
	err = mds.CreateAgent(existingAgUUID, info)
	assert.Nil(t, err)
	agent, err := mds.GetAgent(existingAgUUID)
	assert.Nil(t, err)
	assert.Equal(t, existingAgUUID, utils.UUIDFromProtoOrNil(agent.Info.AgentID))
	assert.Equal(t, "testhost", agent.Info.HostInfo.Hostname)
}

func TestKVMetadataStore_GetASID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	mockDs.
		EXPECT().
		Get("/asid").
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

func TestKVMetadataStore_UpdateSchemas(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	savedSchema, err := c.Get("/agents/" + testutils.NewAgentUUID + "/schema/a_table")
	assert.Nil(t, err)
	assert.NotNil(t, savedSchema)
	schemaPb := &metadatapb.SchemaInfo{}
	proto.Unmarshal(savedSchema, schemaPb)
	assert.Equal(t, "a_table", schemaPb.Name)

	cSchema, err := c.Get("/computedSchema")
	assert.Nil(t, err)
	assert.NotNil(t, cSchema)
	computedPb := &metadatapb.ComputedSchema{}
	proto.Unmarshal(cSchema, computedPb)
	assert.Equal(t, 1, len(computedPb.Tables))
	assert.Equal(t, "a_table", computedPb.Tables[0].Name)
}

func TestKVMetadataStore_GetComputedSchemas(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Create schemas.
	c1 := &metadatapb.SchemaInfo{
		Name:             "table1",
		StartTimestampNS: 4,
	}

	c2 := &metadatapb.SchemaInfo{
		Name:             "table2",
		StartTimestampNS: 5,
	}

	computedSchema := &metadatapb.ComputedSchema{
		Tables: []*metadatapb.SchemaInfo{
			c1, c2,
		},
	}

	computedSchemaText, err := computedSchema.Marshal()
	if err != nil {
		t.Fatal("Unable to marshal schema pb")
	}

	c.Set("/computedSchema", string(computedSchemaText))

	schemas, err := mds.GetComputedSchemas()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(schemas))

	assert.Equal(t, "table1", (*schemas[0]).Name)
	assert.Equal(t, "table2", (*schemas[1]).Name)
}

func TestKVMetadataStore_UpdateProcesses(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	resp, err := c.Get("/processes/123:567:89101")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	p1Pb := &metadatapb.ProcessInfo{}
	proto.Unmarshal(resp, p1Pb)
	assert.Equal(t, "p1", p1Pb.Name)

	resp, err = c.Get("/processes/123:567:246")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	p2Pb := &metadatapb.ProcessInfo{}
	proto.Unmarshal(resp, p2Pb)
	assert.Equal(t, "p2", p2Pb.Name)

	// Update Process.
	p2Pb.Name = "new name"
	err = mds.UpdateProcesses([]*metadatapb.ProcessInfo{p2Pb})
	assert.Nil(t, err)
	resp, err = c.Get("/processes/123:567:246")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	p2Pb = &metadatapb.ProcessInfo{}
	proto.Unmarshal(resp, p2Pb)
	assert.Equal(t, "new name", p2Pb.Name)
}

func TestKVMetadataStore_GetProcesses(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	mockDs.
		EXPECT().
		Get("/processes/246:369:123").
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	c.Set("/processes/123:567:89101", string(p1Text))
	assert.Nil(t, err)

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
	c.Set("/processes/246:369:135", string(p3Text))
	assert.Nil(t, err)

	processes, err := mds.GetProcesses([]*types.UInt128{upid1, upid2, upid3})
	assert.Nil(t, err)
	assert.Equal(t, 3, len(processes))
	assert.Equal(t, "process1", processes[0].Name)
	assert.Equal(t, (*metadatapb.ProcessInfo)(nil), processes[1])
	assert.Equal(t, "process2", processes[2].Name)
}

func TestKVMetadataStore_GetNodeEndpoints(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/endpoints/").
		Return(nil, nil, nil).
		Times(2)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	c.Set("/endpoints/test/abcd", string(e1Text))
	c.Set("/endpoints/test/efgh", string(e2Text))
	c.Set("/endpoints/test/xyz", string(e3Text))

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

func TestKVMetadataStore_GetAgents(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/agent/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	createAgent(t, c, testutils.ExistingAgentUUID, testutils.ExistingAgentInfo)
	createAgent(t, c, testutils.UnhealthyAgentUUID, testutils.UnhealthyAgentInfo)

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
