package controllers

import (
	"fmt"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

const kelvinResponse = `
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
      }
    }
  }
}
`

const queryIDStr = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const kelvinID = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
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
			t.Fatal(err)
		}
		agentUUIDs = append(agentUUIDs, u)
	}

	e := NewQueryExecutor(nc, queryUUID)

	// Subscribe to each agent channel.
	sub1, err := nc.SubscribeSync(fmt.Sprintf("/agent/%s", agentUUIDStrs[0]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	sub2, err := nc.SubscribeSync(fmt.Sprintf("/agent/%s", agentUUIDStrs[1]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	// Plan 1 is a valid, populated plan
	plannerResultPB := &distributedpb.LogicalPlannerResult{}
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Could not unmarshal protobuf text for planner result.")
	}

	planPB1 := plannerResultPB.Plan.QbAddressToPlan[agent1ID]
	// Plan 2 is an empty plan.
	planPB2 := plannerResultPB.Plan.QbAddressToPlan[agent2ID]

	planMap := make(map[uuid.UUID]*planpb.Plan)
	planMap[agentUUIDs[0]] = planPB1
	planMap[agentUUIDs[1]] = planPB2

	// Execute a query.
	err = e.ExecuteQuery(planMap, false)
	if !assert.NoError(t, err) {
		t.Fatal("Query couldn't execute properly.")
	}

	// Check that each agent received the correct message.
	queryUUIDPb := utils.ProtoFromUUID(&queryUUID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto.")
	}
	m1, err := sub1.NextMsg(time.Second)
	pb := &messages.VizierMessage{}
	proto.Unmarshal(m1.Data, pb)
	assert.Equal(t, planPB1, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.Plan)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)

	m2, err := sub2.NextMsg(time.Second)
	pb = &messages.VizierMessage{}
	proto.Unmarshal(m2.Data, pb)
	assert.Equal(t, planPB2, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.Plan)
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

	e := NewQueryExecutor(nc, queryUUID)

	// Add agent results.
	res := new(querybrokerpb.AgentQueryResultRequest)
	if err := proto.UnmarshalText(kelvinResponse, res); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	e.AddResult(res)

	// Make sure that WaitForCompletion returns with correct number of results.
	allRes, err := e.WaitForCompletion()
	assert.NotNil(t, allRes)
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

	e := NewQueryExecutor(nc, queryUUID)

	queryResult, err := e.WaitForCompletion()
	assert.Nil(t, queryResult)
}
