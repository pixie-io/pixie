package controllers_test

import (
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	metadatapb "pixielabs.ai/pixielabs/src/shared/k8s/metadatapb"
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
