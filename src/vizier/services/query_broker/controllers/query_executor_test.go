package controllers

import (
	"fmt"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

const agent1Response = `
agent_id: {
  data: "21285cdd1de94ab1ae6a0ba08c8c676c"
}
result {
  query_id {
    data: "11285cdd1de94ab1ae6a0ba08c8c676c"
  }
  query_result {
    tables {
      relation {
        name: "test_table"
      }
    }
  }
}
`

const agent2Response = `
agent_id: {
  data: "31285cdd1de94ab1ae6a0ba08c8c676c"
}
result {
  query_id {
    data: "11285cdd1de94ab1ae6a0ba08c8c676c"
  }
  query_result {
    tables {
      relation {
        name: "test_table"
      }
    }
  }
}
`

const queryIDStr = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const agent1ID = "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const agent2ID = "31285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

func TestExecuteQuery(t *testing.T) {
	// Check that the query is broadcasted to all agents.
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	agentUUIDStrs := [2]string{
		agent1ID,
		agent2ID,
	}

	agentUUIDs := make([]uuid.UUID, 0)
	for _, uid := range agentUUIDStrs {
		u, err := uuid.FromString(uid)
		if err != nil {
			t.Fatal("Could not parse UUID.")
		}
		agentUUIDs = append(agentUUIDs, u)
	}

	e := NewQueryExecutor(nc, queryUUID, &agentUUIDs)

	// Subscribe to each agent channel.
	sub1, err := nc.SubscribeSync(fmt.Sprintf("/agent/%s", agentUUIDStrs[0]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	sub2, err := nc.SubscribeSync(fmt.Sprintf("/agent/%s", agentUUIDStrs[1]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	// Execute a query.
	e.ExecuteQuery("abcd")

	// Check that each agent received the correct message.
	queryUUIDPb, err := utils.ProtoFromUUID(&queryUUID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto.")
	}

	m1, err := sub1.NextMsg(time.Second)
	pb := &messages.VizierMessage{}
	proto.Unmarshal(m1.Data, pb)
	assert.Equal(t, "abcd", pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryStr)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)

	m2, err := sub2.NextMsg(time.Second)
	pb = &messages.VizierMessage{}
	proto.Unmarshal(m2.Data, pb)
	assert.Equal(t, "abcd", pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryStr)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)
}

func TestWaitForCompletion(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	agentUUIDStrs := [2]string{
		agent1ID,
		agent2ID,
	}

	agentUUIDs := make([]uuid.UUID, 0)
	for _, uid := range agentUUIDStrs {
		u, err := uuid.FromString(uid)
		if err != nil {
			t.Fatal("Could not parse UUID.")
		}
		agentUUIDs = append(agentUUIDs, u)
	}

	e := NewQueryExecutor(nc, queryUUID, &agentUUIDs)

	// Add agent results.
	res := new(querybrokerpb.AgentQueryResultRequest)
	if err := proto.UnmarshalText(agent1Response, res); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	e.AddResult(res)

	res = new(querybrokerpb.AgentQueryResultRequest)
	if err = proto.UnmarshalText(agent2Response, res); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	e.AddResult(res)

	// Make sure that WaitForCompletion returns with correct number of results.
	allRes, err := e.WaitForCompletion()
	assert.Equal(t, 2, len(allRes))
}

func TestWaitForCompletionTimeout(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	agentUUIDStrs := [2]string{
		agent1ID,
		agent2ID,
	}

	agentUUIDs := make([]uuid.UUID, 0)
	for _, uid := range agentUUIDStrs {
		u, err := uuid.FromString(uid)
		if err != nil {
			t.Fatal("Could not parse UUID.")
		}
		agentUUIDs = append(agentUUIDs, u)
	}

	e := NewQueryExecutor(nc, queryUUID, &agentUUIDs)

	// Add agent results.
	res := new(querybrokerpb.AgentQueryResultRequest)
	if err := proto.UnmarshalText(agent1Response, res); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	e.AddResult(res)

	// Make sure that WaitForCompletion returns with correct number of results.
	allRes, err := e.WaitForCompletion()
	assert.Equal(t, 1, len(allRes))
}
