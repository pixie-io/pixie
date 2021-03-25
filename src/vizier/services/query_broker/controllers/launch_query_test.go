package controllers_test

import (
	"fmt"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
)

const queryIDStr = "11285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const agent1ID = "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
const agent2ID = "31285cdd-1de9-4ab1-ae6a-0ba08c8c676c"

func TestLaunchQuery(t *testing.T) {
	// Check that the query is broadcasted to all agents.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

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

	// Subscribe to each agent channel.
	sub1, err := nc.SubscribeSync(fmt.Sprintf("Agent/%s", agentUUIDStrs[0]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	sub2, err := nc.SubscribeSync(fmt.Sprintf("Agent/%s", agentUUIDStrs[1]))
	if err != nil {
		t.Fatal("Could not subscribe to NATS.")
	}

	// Plan 1 is a valid, populated plan
	plannerResultPB := &distributedpb.LogicalPlannerResult{}
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Could not unmarshal protobuf text for planner result.")
	}

	planPB1 := plannerResultPB.Plan.QbAddressToPlan[agent1ID]
	planPB2 := plannerResultPB.Plan.QbAddressToPlan[agent2ID]

	planMap := make(map[uuid.UUID]*planpb.Plan)
	planMap[agentUUIDs[0]] = planPB1
	planMap[agentUUIDs[1]] = planPB2

	// Execute a query.
	err = controllers.LaunchQuery(queryUUID, nc, planMap, false)
	if !assert.NoError(t, err) {
		t.Fatal("Query couldn't execute properly.")
	}

	// Check that each agent received the correct message.
	queryUUIDPb := utils.ProtoFromUUID(queryUUID)
	if err != nil {
		t.Fatal("Could not convert UUID to proto.")
	}
	m1, err := sub1.NextMsg(time.Second)
	assert.NoError(t, err)
	pb := &messages.VizierMessage{}
	proto.Unmarshal(m1.Data, pb)
	assert.Equal(t, planPB1, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.Plan)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)

	m2, err := sub2.NextMsg(time.Second)
	assert.NoError(t, err)
	pb = &messages.VizierMessage{}
	proto.Unmarshal(m2.Data, pb)
	assert.Equal(t, planPB2, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.Plan)
	assert.Equal(t, queryUUIDPb, pb.Msg.(*messages.VizierMessage_ExecuteQueryRequest).ExecuteQueryRequest.QueryID)
}

func TestLaunchQueryNoPlans(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	planMap := make(map[uuid.UUID]*planpb.Plan)

	err = controllers.LaunchQuery(queryUUID, nc, planMap, false)

	assert.NotNil(t, err)
	assert.Regexp(t, fmt.Sprintf("Received no agent plans for query %s", queryIDStr), err)
}
