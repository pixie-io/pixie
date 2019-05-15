package controllers

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

const getAgentsResponse = `
info {
	info {
		agent_id {
			data: "21285cdd1de94ab1ae6a0ba08c8c676c"
		}
		host_info {
			hostname: "test_host"
		}
	}
}
`

const responseByAgent = `
agent_id {
	data: "21285cdd1de94ab1ae6a0ba08c8c676c"
}
response {
  query_result {
    tables {
      relation {
        name: "test_table"
      }
    }
  }
}
`

func TestServerExecuteQuery(t *testing.T) {
	// Start NATS.
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	getAgentsPB := new(metadatapb.AgentInfoResponse)
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

	createExecutorMock := func(_ *nats.Conn, _ uuid.UUID, _ *[]uuid.UUID) Executor {
		mc := mock_controllers.NewMockExecutor(ctrl)
		mc.
			EXPECT().
			ExecuteQuery("abcd")

		mc.
			EXPECT().
			GetQueryID()

		var a []*querybrokerpb.VizierQueryResponse_ResponseByAgent

		agentRespPB := new(querybrokerpb.VizierQueryResponse_ResponseByAgent)
		if err := proto.UnmarshalText(responseByAgent, agentRespPB); err != nil {
			t.Fatal("Cannot Unmarshal protobuf.")
		}

		a = append(a, agentRespPB)
		mc.
			EXPECT().
			WaitForCompletion().
			Return(a, nil)

		return mc
	}

	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := newServer(env, mds, nc, createExecutorMock)

	results, err := s.ExecuteQuery(context.Background(), &querybrokerpb.QueryRequest{
		QueryStr: "abcd",
	})
	assert.Equal(t, 1, len(results.Responses))
}

func TestServerExecuteQueryTimeout(t *testing.T) {
	// Start NATS.
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	getAgentsPB := new(metadatapb.AgentInfoResponse)
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

		// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := NewServer(env, mds, nc)

	results, err := s.ExecuteQuery(context.Background(), &querybrokerpb.QueryRequest{
		QueryStr: "abcd",
	})
	assert.Equal(t, 0, len(results.Responses))
}

func TestReceiveAgentQueryResult(t *testing.T) {
	// Start NATS.
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	m := mock_controllers.NewMockExecutor(ctrl)
	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	req := new(querybrokerpb.AgentQueryResultRequest)
	if err := proto.UnmarshalText(agent1Response, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	m.
		EXPECT().
		AddResult(req)

		// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := NewServer(env, mds, nc)
	if err != nil {
		t.Fatal("Creating server failed.")
	}

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	// Add mock executor as an executor.
	s.executors[queryUUID] = m

	expectedResp := &querybrokerpb.AgentQueryResultResponse{}

	resp, err := s.ReceiveAgentQueryResult(context.Background(), req)
	assert.Equal(t, expectedResp, resp)
}
