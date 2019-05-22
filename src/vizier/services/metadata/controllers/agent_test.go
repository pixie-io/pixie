package controllers_test

import (
	"context"
	"fmt"
	"net/url"
	"testing"
	"time"

	"github.com/coreos/etcd/clientv3"
	"github.com/coreos/etcd/embed"
	"github.com/gogo/protobuf/proto"
	"github.com/phayes/freeport"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"github.com/stretchr/testify/assert"
	utils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	data "pixielabs.ai/pixielabs/src/vizier/services/metadata/datapb"
)

// LastHeartBeatNS is 65 seconds, in NS.
var existingAgentInfo = `
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

var unhealthyAgentInfo = `
agent_id {
	data: "8ba7b8109dad11d180b400c04fd430c8"
}
host_info {
	hostname: "anotherhost"
}
create_time_ns: 0
last_heartbeat_ns: 0
`

var newAgentUUID = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
var existingAgentUUID = "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
var unhealthyAgentUUID = "8ba7b810-9dad-11d1-80b4-00c04fd430c8"

func setupAgentManager(t *testing.T) (*clientv3.Client, controllers.AgentManager, func()) {
	// Find available port.
	port, err := freeport.GetFreePort()
	if err != nil {
		t.Fatal("Could not find free port")
	}

	// Start up etcd server.
	cfg := embed.NewConfig()
	cfg.Dir = "default.etcd"
	lcURL, err := url.Parse(fmt.Sprintf("http://localhost:%d", port))
	if err != nil {
		t.Fatal("Could not parse URL.")
	}
	cfg.LCUrls = []url.URL{*lcURL}
	e, err := embed.StartEtcd(cfg)
	if err != nil {
		t.Fatal("Could not start etcd server.")
	}

	select {
	case <-e.Server.ReadyNotify():
		log.Info("Server is ready.")
	case <-time.After(60 * time.Second):
		e.Server.Stop()
		t.Fatal("Server took too long to start, stopping server.")
	}

	// Add some existing agent data into etcd.
	etcdClient, err := clientv3.New(clientv3.Config{
		Endpoints:   []string{fmt.Sprintf("http://localhost:%d", port)},
		DialTimeout: 5 * time.Second,
	})

	if err != nil {
		t.Fatal("Failed to connect to etcd.")
	}

	_, err = etcdClient.Delete(context.Background(), "", clientv3.WithPrefix())
	if err != nil {
		t.Fatal("Failed to clear etcd data.")
	}

	createAgent(t, existingAgentUUID, etcdClient, existingAgentInfo)
	createAgent(t, unhealthyAgentUUID, etcdClient, unhealthyAgentInfo)

	cleanup := func() {
		e.Close()
		etcdClient.Close()
	}

	clock := testingutils.NewTestClock(time.Unix(0, clockNowNS))
	agtMgr := controllers.NewAgentManagerWithClock(etcdClient, true, clock)

	return etcdClient, agtMgr, cleanup
}

func createAgent(t *testing.T, agentID string, client *clientv3.Client, agentPb string) {
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
}

func TestCreateAgent(t *testing.T) {
	etcdClient, agtMgr, cleanup := setupAgentManager(t)
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
	assert.Equal(t, newAgentUUID, uid.String())
	assert.Equal(t, "localhost", pb.HostInfo.Hostname)
}

func TestCreateExistingAgent(t *testing.T) {
	etcdClient, agtMgr, cleanup := setupAgentManager(t)
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
	assert.Equal(t, existingAgentUUID, uid.String())
	assert.Equal(t, "testhost", pb.HostInfo.Hostname)
}

func TestUpdateHeartbeat(t *testing.T) {
	etcdClient, agtMgr, cleanup := setupAgentManager(t)
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

func TestUpdateAgentState(t *testing.T) {
	etcdClient, agtMgr, cleanup := setupAgentManager(t)
	defer cleanup()

	err := agtMgr.UpdateAgentState()
	assert.Nil(t, err)

	resp, err := etcdClient.Get(context.Background(), controllers.GetAgentKey(""), clientv3.WithPrefix())
	assert.Equal(t, 1, len(resp.Kvs))

	resp, err = etcdClient.Get(context.Background(), controllers.GetAgentKey(unhealthyAgentUUID))
	// Agent should no longer exist in etcd.
	assert.Equal(t, 0, len(resp.Kvs))
}

func TestGetActiveAgents(t *testing.T) {
	_, agtMgr, cleanup := setupAgentManager(t)
	defer cleanup()

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

func TestGetActiveAgentsWithLock(t *testing.T) {
	etcdClient, agtMgr, cleanup := setupAgentManager(t)
	defer cleanup()

	_, err := etcdClient.Put(context.Background(),
		controllers.GetAgentKey("ae6f4648-c06b-470c-a01f-1209a3dfa4bc")+"/487f6ad73ac92645", "")
	if err != nil {
		t.Fatal("Unable to add fake agent key to etcd.")
	}

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
