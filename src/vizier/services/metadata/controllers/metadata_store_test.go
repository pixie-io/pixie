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

	mds.SetClusterCIDR("1.2.3.4/18")
	assert.Equal(t, "1.2.0.0/18", mds.GetClusterCIDR())
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

func TestKVMetadataStore_GetPods(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/pod/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	c.Set("/pod/test/abcd-pod", string(pod1Text))
	c.Set("/pod/test/efgh-pod", string(pod2Text))

	pods, err := mds.GetPods()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(pods))

	assert.Equal(t, pod1.Metadata.Name, (*pods[0]).Metadata.Name)
	assert.Equal(t, pod2.Metadata.Name, (*pods[1]).Metadata.Name)
}

func TestKVMetadataStore_UpdatePod(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	expectedPb := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdatePod(expectedPb, false)
	if err != nil {
		t.Fatal("Could not update pod.")
	}

	// Check that correct pod info is in etcd.
	resp, err := c.Get("/pod/default/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb := &metadatapb.Pod{}
	proto.Unmarshal(resp, pb)

	assert.Equal(t, expectedPb, pb)

	// Test that deletion timestamp gets set.
	err = mds.UpdatePod(expectedPb, true)
	if err != nil {
		t.Fatal("Could not update pod.")
	}

	// Check that correct pod info is in etcd.
	resp, err = c.Get("/pod/default/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb = &metadatapb.Pod{}
	proto.Unmarshal(resp, pb)

	assert.NotEqual(t, int64(0), pb.Metadata.DeletionTimestampNS)

	// Test that deletion timestamp is not set again if it is already set.
	expectedPb.Metadata.DeletionTimestampNS = 10
	err = mds.UpdatePod(expectedPb, true)
	if err != nil {
		t.Fatal("Could not update pod.")
	}

	// Check that correct pod info is in etcd.
	resp, err = c.Get("/pod/default/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb = &metadatapb.Pod{}
	proto.Unmarshal(resp, pb)
	assert.Equal(t, int64(10), pb.Metadata.DeletionTimestampNS)
}

func TestKVMetadataStore_GetContainers(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/containers/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	c.Set("/containers/container_id_1/info", string(c1Text))
	c.Set("/containers/container_id_2/info", string(c2Text))

	containers, err := mds.GetContainers()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(containers))

	assert.Equal(t, c1.UID, (*containers[0]).UID)
	assert.Equal(t, c2.UID, (*containers[1]).UID)
}

func TestKVMetadataStore_UpdateContainer(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)
	expectedPb := &metadatapb.ContainerInfo{}
	if err := proto.UnmarshalText(testutils.ContainerInfoPB, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateContainer(expectedPb)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in cache.
	resp, err := c.Get("/containers/container1/info")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb := &metadatapb.ContainerInfo{}
	proto.Unmarshal(resp, pb)

	assert.Equal(t, expectedPb, pb)
}

func TestKVMetadataStore_UpdateContainersFromPod(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	podInfo := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PodPbWithContainers, podInfo); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateContainersFromPod(podInfo, false)
	assert.Nil(t, err)

	containerResp, err := c.Get("/containers/test/info")
	assert.Nil(t, err)
	assert.NotNil(t, containerResp)
	containerPb := &metadatapb.ContainerInfo{}
	proto.Unmarshal(containerResp, containerPb)
	assert.Equal(t, "container1", containerPb.Name)
	assert.Equal(t, "test", containerPb.UID)
	assert.Equal(t, "ijkl", containerPb.PodUID)

	// Test that deletion timestamp gets set.
	err = mds.UpdateContainersFromPod(podInfo, true)
	assert.Nil(t, err)

	containerResp, err = c.Get("/containers/test/info")
	assert.Nil(t, err)
	assert.NotNil(t, containerResp)
	containerPb = &metadatapb.ContainerInfo{}
	proto.Unmarshal(containerResp, containerPb)
	assert.NotEqual(t, int64(0), containerPb.StopTimestampNS)
}

func TestKVMetadataStore_UpdateContainersFromPendingPod(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/containers/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	podInfo := &metadatapb.Pod{}
	if err := proto.UnmarshalText(testutils.PendingPodPb, podInfo); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateContainersFromPod(podInfo, false)
	assert.Nil(t, err)

	_, v, err := c.GetWithPrefix("/containers/")
	assert.Nil(t, err)
	assert.NotNil(t, v)
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

func TestKVMetadataStore_GetEndpoints(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/endpoints/").
		Return(nil, nil, nil).
		Times(1)

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

	c.Set("/endpoints/test/abcd", string(e1Text))
	c.Set("/endpoints/test/efgh", string(e2Text))

	eps, err := mds.GetEndpoints()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(eps))

	assert.Equal(t, e1.Metadata.Name, (*eps[0]).Metadata.Name)
	assert.Equal(t, e2.Metadata.Name, (*eps[1]).Metadata.Name)
}

func TestKVMetadataStore_UpdateEndpoints(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	expectedPb := &metadatapb.Endpoints{}
	if err := proto.UnmarshalText(testutils.EndpointsPb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateEndpoints(expectedPb, false)
	if err != nil {
		t.Fatal("Could not update endpoints.")
	}

	// Check that correct endpoint info is in the cache.
	resp, err := c.Get("/endpoints/a_namespace/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb := &metadatapb.Endpoints{}
	proto.Unmarshal(resp, pb)

	mapResp, err := c.Get("/services/a_namespace/object_md/pods")
	assert.Nil(t, err)
	assert.NotNil(t, mapResp)
	assert.Equal(t, "abcd,efgh", string(mapResp))

	// Test that deletion timestamp gets set.
	err = mds.UpdateEndpoints(expectedPb, true)
	if err != nil {
		t.Fatal("Could not update endpoints.")
	}

	// Check that correct endpoint info is in etcd.
	resp, err = c.Get("/endpoints/a_namespace/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb = &metadatapb.Endpoints{}
	proto.Unmarshal(resp, pb)
	assert.NotEqual(t, int64(0), pb.Metadata.DeletionTimestampNS)

	// Test that deletion timestamp is not set again if it is already set.
	expectedPb.Metadata.DeletionTimestampNS = 10
	err = mds.UpdateEndpoints(expectedPb, true)
	if err != nil {
		t.Fatal("Could not update endpoints.")
	}

	// Check that correct endpoint info is in etcd.
	resp, err = c.Get("/endpoints/a_namespace/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb = &metadatapb.Endpoints{}
	proto.Unmarshal(resp, pb)
	assert.Equal(t, int64(10), pb.Metadata.DeletionTimestampNS)
}

func TestKVMetadataStore_GetServices(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/service/").
		Return(nil, nil, nil).
		Times(1)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

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

	c.Set("/service/test/abcd-service", string(s1Text))
	c.Set("/service/test/efgh-service", string(s2Text))

	services, err := mds.GetServices()
	assert.Nil(t, err)
	assert.Equal(t, 2, len(services))

	assert.Equal(t, s1.Metadata.Name, (*services[0]).Metadata.Name)
	assert.Equal(t, s2.Metadata.Name, (*services[1]).Metadata.Name)
}

func TestKVMetadataStore_UpdateService(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	expectedPb := &metadatapb.Service{}
	if err := proto.UnmarshalText(testutils.ServicePb, expectedPb); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	err = mds.UpdateService(expectedPb, false)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in etcd.
	resp, err := c.Get("/service/a_namespace/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb := &metadatapb.Service{}
	proto.Unmarshal(resp, pb)

	assert.Equal(t, expectedPb, pb)

	// Test that deletion timestamp gets set.
	err = mds.UpdateService(expectedPb, true)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in etcd.
	resp, err = c.Get("/service/a_namespace/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb = &metadatapb.Service{}
	proto.Unmarshal(resp, pb)
	assert.NotEqual(t, int64(0), pb.Metadata.DeletionTimestampNS)

	// Test that deletion timestamp is not set again if it is already set.
	expectedPb.Metadata.DeletionTimestampNS = 10
	err = mds.UpdateService(expectedPb, true)
	if err != nil {
		t.Fatal("Could not update service.")
	}

	// Check that correct service info is in etcd.
	resp, err = c.Get("/service/a_namespace/ijkl")
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	pb = &metadatapb.Service{}
	proto.Unmarshal(resp, pb)
	assert.Equal(t, int64(10), pb.Metadata.DeletionTimestampNS)
}

func TestKVMetadataStore_GetServiceCIDR(t *testing.T) {
	// Test setup.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	// Main test cases:

	// Before any information, we return empty string.
	assert.Equal(t, "", mds.GetServiceCIDR())

	// First Service turns into the service CIDR.
	s1 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "s1",
			Namespace: "test",
			UID:       "s1-service",
		},
		Spec: &metadatapb.ServiceSpec{
			ClusterIP: "10.64.3.1",
		},
	}
	err = mds.UpdateService(s1, false)
	if err != nil {
		t.Fatal("Could not update service.")
	}
	assert.Equal(t, "10.64.3.1/32", mds.GetServiceCIDR())

	// Next service should expand the mask.
	s4 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "s4",
			Namespace: "test",
			UID:       "s4-service",
		},
		Spec: &metadatapb.ServiceSpec{
			ClusterIP: "10.64.3.7",
		},
	}
	err = mds.UpdateService(s4, false)
	if err != nil {
		t.Fatal("Could not update service.")
	}
	assert.Equal(t, "10.64.3.0/29", mds.GetServiceCIDR())

	// This one shouldn't expand the mask, because it's already within the same range.
	s2 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "s2",
			Namespace: "test",
			UID:       "s2-service",
		},
		Spec: &metadatapb.ServiceSpec{
			ClusterIP: "10.64.3.2",
		},
	}
	err = mds.UpdateService(s2, false)
	if err != nil {
		t.Fatal("Could not update service.")
	}
	assert.Equal(t, "10.64.3.0/29", mds.GetServiceCIDR())

	// Another range expansion.
	s3 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "s3",
			Namespace: "test",
			UID:       "s3-service",
		},
		Spec: &metadatapb.ServiceSpec{
			ClusterIP: "10.64.4.1",
		},
	}
	err = mds.UpdateService(s3, false)
	if err != nil {
		t.Fatal("Could not update service.")
	}
	assert.Equal(t, "10.64.0.0/21", mds.GetServiceCIDR())

	// Test on Services that do not have ClusterIP.
	s5 := &metadatapb.Service{
		Metadata: &metadatapb.ObjectMetadata{
			Name:      "s5",
			Namespace: "test",
			UID:       "s5-service",
		},
		Spec: &metadatapb.ServiceSpec{
			// No ClusterIP.
		},
	}
	err = mds.UpdateService(s5, false)
	if err != nil {
		t.Fatal("Could not update service.")
	}
	assert.Equal(t, "10.64.0.0/21", mds.GetServiceCIDR())
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

func TestKVMetadataStore_GetAgentsForHostnames(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		Get("/hostname/test5/agent").
		Return(nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	c.Set("/hostname/test/agent", "agent1")
	c.Set("/hostname/test2/agent", "agent2")
	c.Set("/hostname/test3/agent", "agent3")
	c.Set("/hostname/test4/agent", "agent4")

	hostnames := []string{"test", "test2", "test3", "test4", "test5"}

	agents, err := mds.GetAgentsForHostnames(&hostnames)
	assert.Nil(t, err)

	assert.Equal(t, 4, len(agents))
	assert.Equal(t, "agent1", (agents)[0])
	assert.Equal(t, "agent2", (agents)[1])
	assert.Equal(t, "agent3", (agents)[2])
	assert.Equal(t, "agent4", (agents)[3])
}

func TestKVMetadataStore_GetKelvinIDs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/kelvin/").
		Return(nil, nil, nil)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	c.Set("/kelvin/test", "test")
	c.Set("/kelvin/test2", "test2")
	c.Set("/kelvin/test3", "test3")
	c.Set("/kelvin/test4", "test4")

	kelvins, err := mds.GetKelvinIDs()
	assert.Nil(t, err)

	assert.Equal(t, 4, len(kelvins))
	assert.Equal(t, "test", kelvins[0])
	assert.Equal(t, "test2", kelvins[1])
	assert.Equal(t, "test3", kelvins[2])
	assert.Equal(t, "test4", kelvins[3])
}

func TestKVMetadataStore_AddResourceVersion(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)

	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)

	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	updatePb := &metadatapb.ResourceUpdate{
		ResourceVersion: "1",
		Update: &metadatapb.ResourceUpdate_ServiceUpdate{
			ServiceUpdate: &metadatapb.ServiceUpdate{
				UID:              "ijkl",
				Name:             "object_md",
				Namespace:        "a_namespace",
				StartTimestampNS: 4,
				StopTimestampNS:  6,
				PodIDs:           []string{"abcd", "efgh"},
			},
		},
	}

	b, err := updatePb.Marshal()
	assert.Nil(t, err)

	err = mds.AddResourceVersion("1234", updatePb)
	assert.Nil(t, err)

	rvUpdate, _ := c.Get("/resourceVersionUpdate/1234")
	assert.Equal(t, b, rvUpdate)

}

func TestKVMetadataStore_GetMetadataUpdates(t *testing.T) {
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

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockDs := mock_kvstore.NewMockKeyValueStore(ctrl)
	mockDs.
		EXPECT().
		GetWithPrefix("/pod/").
		Return(nil, nil, nil).
		Times(1)
	mockDs.
		EXPECT().
		GetWithPrefix("/endpoints/").
		Return(nil, nil, nil).
		Times(1)
	clock := testingutils.NewTestClock(time.Unix(2, 0))
	c := kvstore.NewCacheWithClock(mockDs, clock)
	mds, err := controllers.NewKVMetadataStore(c)
	assert.Nil(t, err)

	err = mds.UpdatePod(pod1, false)
	assert.Nil(t, err)
	err = mds.UpdatePod(pod2, false)
	assert.Nil(t, err)

	err = mds.UpdateEndpoints(ep1, false)
	assert.Nil(t, err)

	updates, err := mds.GetMetadataUpdates("")
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
