package controllers_test

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/gogo/protobuf/proto"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

func makeRowBatchResult(t *testing.T, queryID uuid.UUID, tableName string, tableID string,
	eos bool) (*vizierpb.RowBatchData, *carnotpb.TransferResultChunkRequest) {
	rb := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, rb); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}
	rb.Eos = eos

	expected, err := controllers.RowBatchToVizierRowBatch(rb, tableID)
	if err != nil {
		t.Fatalf("Could not convert row batch: %v", err)
	}

	return expected, &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: pbutils.ProtoFromUUID(&queryID),
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: rb,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
					TableName: tableName,
				},
			},
		},
	}
}

func makeExecStatsResult(t *testing.T, queryID uuid.UUID) (*vizierpb.QueryExecutionStats, *carnotpb.TransferResultChunkRequest) {
	execStats := &queryresultspb.QueryExecutionStats{
		Timing: &queryresultspb.QueryTimingInfo{
			ExecutionTimeNs:   5010,
			CompilationTimeNs: 350,
		},
		BytesProcessed:   4521,
		RecordsProcessed: 4,
	}

	expected := controllers.QueryResultStatsToVizierStats(execStats, execStats.Timing.CompilationTimeNs)

	return expected, &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: pbutils.ProtoFromUUID(&queryID),
		Result: &carnotpb.TransferResultChunkRequest_ExecutionAndTimingInfo{
			ExecutionAndTimingInfo: &carnotpb.TransferResultChunkRequest_QueryExecutionAndTimingInfo{
				ExecutionStats:      execStats,
				AgentExecutionStats: nil,
			},
		},
	}
}

func makePlan(t *testing.T) (*distributedpb.DistributedPlan, map[uuid.UUID]*planpb.Plan) {
	// Plan 1 is a valid, populated plan
	plannerResultPB := &distributedpb.LogicalPlannerResult{}
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Could not unmarshal protobuf text for planner result.")
	}

	planPB1 := plannerResultPB.Plan.QbAddressToPlan[agent1ID]
	// Plan 2 is an empty plan.
	planPB2 := plannerResultPB.Plan.QbAddressToPlan[agent2ID]

	planMap := make(map[uuid.UUID]*planpb.Plan)
	uuid1, err := uuid.FromString(agent1ID)
	if err != nil {
		t.Fatalf("Got error: %v", err)
	}
	uuid2, err := uuid.FromString(agent2ID)
	if err != nil {
		t.Fatalf("Got error: %v", err)
	}
	planMap[uuid1] = planPB1
	planMap[uuid2] = planPB2
	return plannerResultPB.Plan, planMap
}

func TestStreamResultsSimple(t *testing.T) {
	queryID := uuid.NewV4()

	f := controllers.NewQueryResultForwarderWithTimeout(1 * time.Second)

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)
	doneCh := make(chan bool)

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-doneCh:
				wg.Done()
				return
			}
		}
	}()
	ctx := context.Background()
	var err error
	var timeout bool

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables))

	go func() {
		timeout, err = f.StreamResults(ctx, queryID, resultCh, 350, nil)
		assert.False(t, timeout)
		close(doneCh)
	}()

	expected0, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected1, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	expected2, in2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	expected3, in3 := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(in0))
	assert.Nil(t, f.ForwardQueryResult(in1))
	assert.Nil(t, f.ForwardQueryResult(in2))
	assert.Nil(t, f.ForwardQueryResult(in3))
	wg.Wait()

	assert.Nil(t, err)
	assert.Equal(t, 4, len(results))

	for _, result := range results {
		assert.Equal(t, queryID.String(), result.QueryID)
	}
	assert.Equal(t, expected0, results[0].GetData().Batch)
	assert.Equal(t, expected1, results[1].GetData().Batch)
	assert.Equal(t, expected2, results[2].GetData().Batch)
	assert.Equal(t, expected3, results[3].GetData().ExecutionStats)
}

func TestStreamResultsAgentCancel(t *testing.T) {
	queryID := uuid.NewV4()

	f := controllers.NewQueryResultForwarderWithTimeout(1 * time.Second)

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)
	doneCh := make(chan bool)

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-doneCh:
				wg.Done()
				return
			}
		}
	}()
	ctx := context.Background()
	var err error
	var timeout bool

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables))

	go func() {
		timeout, err = f.StreamResults(ctx, queryID, resultCh, 350, nil)
		assert.False(t, timeout)

		// Forwarding after stream is done should fail.
		_, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
		forwardErr := f.ForwardQueryResult(in1)
		assert.NotNil(t, forwardErr)
		assert.Equal(t,
			fmt.Errorf("error in ForwardQueryResult: Query %s is not registered in query forwarder", queryID.String()),
			forwardErr,
		)

		close(doneCh)
	}()

	_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	assert.Nil(t, f.ForwardQueryResult(in0))
	f.OptionallyCancelClientStream(queryID)
	// Make sure it's safe to call cancel twice.
	f.OptionallyCancelClientStream(queryID)

	wg.Wait()

	assert.Equal(t, 1, len(results))
	assert.NotNil(t, err)
	assert.Equal(t, fmt.Errorf("Client stream cancelled by agent result stream for query %s", queryID.String()), err)
}

func TestStreamResultsClientContextCancel(t *testing.T) {
	queryID := uuid.NewV4()

	f := controllers.NewQueryResultForwarderWithTimeout(1 * time.Second)

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)
	doneCh := make(chan bool)

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-doneCh:
				wg.Done()
				return
			}
		}
	}()

	ctx, cancel := context.WithCancel(context.Background())
	var err error
	var timeout bool

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables))

	go func() {
		timeout, err = f.StreamResults(ctx, queryID, resultCh, 350, nil)
		assert.False(t, timeout)
		close(doneCh)
	}()

	_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	_, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	_, in2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	_, in3 := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(in0))
	assert.Nil(t, f.ForwardQueryResult(in1))
	cancel()

	// It's ok if these error but they should not hang.
	f.ForwardQueryResult(in2)
	f.ForwardQueryResult(in3)
	wg.Wait()

	assert.Nil(t, err)
}

func TestStreamResultsQueryPlan(t *testing.T) {
	queryID := uuid.NewV4()

	f := controllers.NewQueryResultForwarderWithTimeout(1 * time.Second)

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)
	doneCh := make(chan bool)

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-doneCh:
				wg.Done()
				return
			}
		}
	}()
	ctx := context.Background()
	var err error
	var timeout bool

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables))

	plan, planMap := makePlan(t)

	queryPlanOpts := &controllers.QueryPlanOpts{
		TableID: "query_plan_table_id",
		Plan:    plan,
		PlanMap: planMap,
	}

	go func() {
		timeout, err = f.StreamResults(ctx, queryID, resultCh, 350, queryPlanOpts)
		assert.False(t, timeout)
		close(doneCh)
	}()

	expected0, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected1, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	expected2, in2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	expected4, in3 := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(in0))
	assert.Nil(t, f.ForwardQueryResult(in1))
	assert.Nil(t, f.ForwardQueryResult(in2))
	assert.Nil(t, f.ForwardQueryResult(in3))
	wg.Wait()

	assert.Nil(t, err)
	assert.Equal(t, 5, len(results))

	for _, result := range results {
		assert.Equal(t, queryID.String(), result.QueryID)
	}

	// Check the query plan.
	assert.Equal(t, "query_plan_table_id", results[3].GetData().Batch.TableID)
	assert.True(t, results[3].GetData().Batch.Eos)
	assert.True(t, results[3].GetData().Batch.Eow)
	assert.Equal(t, int64(1), results[3].GetData().Batch.NumRows)
	assert.Equal(t, 1, len(results[3].GetData().Batch.Cols))
	assert.NotNil(t, results[3].GetData().Batch.Cols[0].GetStringData())
	strData := results[3].GetData().Batch.Cols[0].GetStringData().Data
	assert.Equal(t, 1, len(strData))
	assert.Contains(t, strData[0], agent1ID)
	assert.Contains(t, strData[0], agent2ID)

	// Check the other results.
	assert.Equal(t, expected0, results[0].GetData().Batch)
	assert.Equal(t, expected1, results[1].GetData().Batch)
	assert.Equal(t, expected2, results[2].GetData().Batch)
	assert.Equal(t, expected4, results[4].GetData().ExecutionStats)
}

func TestStreamResultsWrongQueryID(t *testing.T) {
	queryID := uuid.NewV4()
	otherQueryID := uuid.NewV4()

	f := controllers.NewQueryResultForwarderWithTimeout(1 * time.Second)

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)
	doneCh := make(chan bool)

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-doneCh:
				wg.Done()
				return
			}
		}
	}()
	ctx := context.Background()
	var err error
	var timeout bool

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables))

	go func() {
		timeout, err = f.StreamResults(ctx, queryID, resultCh, 350, nil)
		assert.False(t, timeout)
		close(doneCh)
	}()

	expected0, goodInput := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	_, badInput := makeRowBatchResult(t, otherQueryID, "bar", "456" /*eos*/, true)

	assert.Nil(t, f.ForwardQueryResult(goodInput))
	assert.NotNil(t, f.ForwardQueryResult(badInput))
	f.OptionallyCancelClientStream(queryID)
	assert.Nil(t, err)
	wg.Wait()

	assert.Equal(t, err.Error(), fmt.Sprintf(
		"Client stream cancelled by agent result stream for query %s", queryID.String()))
	assert.Equal(t, 1, len(results))
	assert.Equal(t, queryID.String(), results[0].QueryID)
	assert.Equal(t, expected0, results[0].GetData().Batch)
}
