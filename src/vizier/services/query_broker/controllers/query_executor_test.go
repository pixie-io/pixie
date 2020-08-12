package controllers_test

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
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	messages "pixielabs.ai/pixielabs/src/vizier/messages/messagespb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
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

	e := controllers.NewQueryExecutor(nc, queryUUID)

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

	e := controllers.NewQueryExecutor(nc, queryUUID)

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

	e := controllers.NewQueryExecutor(nc, queryUUID)

	queryResult, err := e.WaitForCompletion()
	assert.Nil(t, queryResult)
}

func TestWaitForCompletionStreaming(t *testing.T) {
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
	queryIDpb := utils.ProtoFromUUID(&queryUUID)

	agentUUID, err := uuid.FromString(agent1ID)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}
	agentIDpb := utils.ProtoFromUUID(&agentUUID)

	relation1 := &schemapb.Relation{
		Columns: []*schemapb.Relation_ColumnInfo{
			&schemapb.Relation_ColumnInfo{
				ColumnName: "foo",
			},
		},
	}
	relation2 := &schemapb.Relation{
		Columns: []*schemapb.Relation_ColumnInfo{
			&schemapb.Relation_ColumnInfo{
				ColumnName: "bar",
			},
		},
	}
	resultSchema := map[string]*schemapb.Relation{
		"table1": relation1,
		"table2": relation2,
	}
	e := controllers.NewQueryExecutorWithExpectedSchema(nc, queryUUID, resultSchema)

	// Add agent results.
	res := new(querybrokerpb.AgentQueryResultRequest)
	if err := proto.UnmarshalText(kelvinResponse, res); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	t1rb := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, t1rb); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}
	t1rb.Eos = false
	t1rbEOS := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, t1rbEOS); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}
	t2rbEOS := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, t2rbEOS); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	table1Msg := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: t1rb,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
					TableName: "table1",
				},
			},
		},
	}
	table1EOSMsg := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: t1rbEOS,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
					TableName: "table1",
				},
			},
		},
	}
	table2EOSMsg := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: t2rbEOS,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
					TableName: "table2",
				},
			},
		},
	}
	execStats := &queryresultspb.QueryExecutionStats{
		Timing: &queryresultspb.QueryTimingInfo{
			ExecutionTimeNs:   5010,
			CompilationTimeNs: 350,
		},
		BytesProcessed:   4521,
		RecordsProcessed: 4,
	}
	agentStats := []*queryresultspb.AgentExecutionStats{
		&queryresultspb.AgentExecutionStats{
			AgentID:          agentIDpb,
			ExecutionTimeNs:  50,
			BytesProcessed:   4521,
			RecordsProcessed: 4,
		},
	}
	execStatsMsg := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_ExecutionAndTimingInfo{
			ExecutionAndTimingInfo: &carnotpb.TransferResultChunkRequest_QueryExecutionAndTimingInfo{
				ExecutionStats:      execStats,
				AgentExecutionStats: agentStats,
			},
		},
	}

	err = e.AddStreamedResult(table1Msg)
	assert.Nil(t, err, err)
	err = e.AddStreamedResult(table1EOSMsg)
	assert.Nil(t, err, err)
	err = e.AddStreamedResult(table2EOSMsg)
	assert.Nil(t, err, err)
	err = e.AddStreamedResult(execStatsMsg)
	assert.Nil(t, err, err)

	// Make sure that WaitForCompletion returns with correct number of results.
	allRes, err := e.WaitForCompletion()
	assert.Nil(t, err)
	assert.NotNil(t, allRes)

	assert.Equal(t, execStats.Timing, allRes.TimingInfo)
	assert.Equal(t, execStats, allRes.ExecutionStats)
	assert.Equal(t, agentStats, allRes.AgentExecutionStats)

	assert.Equal(t, 2, len(allRes.Tables))
	tables := make(map[string]*schemapb.Table)
	tables[allRes.Tables[0].Name] = allRes.Tables[0]
	tables[allRes.Tables[1].Name] = allRes.Tables[1]

	assert.NotNil(t, tables["table1"])
	assert.NotNil(t, tables["table2"])
	assert.Equal(t, relation1, tables["table1"].Relation)
	assert.Equal(t, relation2, tables["table2"].Relation)
	assert.Equal(t, 2, len(tables["table1"].RowBatches))
	assert.Equal(t, 1, len(tables["table2"].RowBatches))
}

func TestOutputSchemaFromPlan(t *testing.T) {
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

	output := controllers.OutputSchemaFromPlan(planMap)
	assert.Equal(t, 1, len(output))
	assert.NotNil(t, output["out"])
	assert.Equal(t, 3, len(output["out"].Columns))

	assert.Equal(t, &schemapb.Relation_ColumnInfo{
		ColumnName:         "time_",
		ColumnType:         typespb.TIME64NS,
		ColumnSemanticType: typespb.ST_NONE,
	}, output["out"].Columns[0])

	assert.Equal(t, &schemapb.Relation_ColumnInfo{
		ColumnName:         "cpu_cycles",
		ColumnType:         typespb.INT64,
		ColumnSemanticType: typespb.ST_NONE,
	}, output["out"].Columns[1])

	assert.Equal(t, &schemapb.Relation_ColumnInfo{
		ColumnName:         "upid",
		ColumnType:         typespb.UINT128,
		ColumnSemanticType: typespb.ST_UPID,
	}, output["out"].Columns[2])
}
