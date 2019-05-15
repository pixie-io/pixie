package controllers_test

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

const agent1StatusPB = `
info {
  agent_id {
    data: "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
  }
  host_info {
    hostname: "test_host"
  }
}
last_heartbeat_ns: 10
state: 1
`

const agent2StatusPB = `
info {
  agent_id {
    data: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
  }
  host_info {
    hostname: "another_host"
  }
}
last_heartbeat_ns: 20
state: 1
`

func TestGetAgentInfo(t *testing.T) {
	// Set up mock.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockAgtMgr := mock_controllers.NewMockAgentManager(ctrl)

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

	agents := []controllers.AgentInfo{
		controllers.AgentInfo{
			LastHeartbeatNS: 10,
			CreateTimeNS:    5,
			AgentID:         u1,
			Hostname:        "test_host",
		},
		controllers.AgentInfo{
			LastHeartbeatNS: 20,
			CreateTimeNS:    0,
			AgentID:         u2,
			Hostname:        "another_host",
		},
	}

	mockAgtMgr.
		EXPECT().
		GetActiveAgents().
		Return(agents, nil)

	// Set up server.
	env, err := metadataenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServer(env, mockAgtMgr)

	req := metadatapb.AgentInfoRequest{}

	resp, err := s.GetAgentInfo(context.Background(), &req)

	assert.Equal(t, 2, len(resp.Info))

	agentResp := new(metadatapb.AgentStatus)
	if err := proto.UnmarshalText(agent1StatusPB, agentResp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, agentResp, resp.Info[0])

	agentResp = new(metadatapb.AgentStatus)
	if err = proto.UnmarshalText(agent2StatusPB, agentResp); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, agentResp, resp.Info[1])
}
