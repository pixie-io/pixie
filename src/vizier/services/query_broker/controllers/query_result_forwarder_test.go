/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers_test

import (
	"context"
	"fmt"
	"sync"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
)

func makeInitiateConnectionRequest(queryID uuid.UUID) *carnotpb.TransferResultChunkRequest {
	return &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: utils.ProtoFromUUID(queryID),
		Result: &carnotpb.TransferResultChunkRequest_InitiateConn{
			InitiateConn: &carnotpb.TransferResultChunkRequest_InitiateConnection{},
		},
	}
}

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
		QueryID: utils.ProtoFromUUID(queryID),
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: rb,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: tableName,
				},
			},
		},
	}
}

func makeExecStatsResult(t *testing.T, queryID uuid.UUID) (*vizierpb.QueryExecutionStats,
	*carnotpb.TransferResultChunkRequest) {
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
		QueryID: utils.ProtoFromUUID(queryID),
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
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	expected0, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected1, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	expected2, in2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	expected3, in3 := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in1))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in2))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in3))
	wg.Wait()

	require.NoError(t, err)
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
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	firstResultCh := make(chan struct{})
	firstResultOnce := sync.Once{}

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
				firstResultOnce.Do(func() { close(firstResultCh) })
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))

	// Wait for the consumer to receive its first result before cancelling the query.
	timer := time.NewTimer(time.Second)
	select {
	case <-firstResultCh:
	case <-timer.C:
		assert.Fail(t, "Test timedout waiting for consumer to receive a result.")
	}

	f.ProducerCancelStream(queryID, fmt.Errorf("An error 1"))

	wg.Wait()
	// Forwarding after stream is done should fail.
	_, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	forwardErr := f.ForwardQueryResult(producerCtx, in1)
	assert.NotNil(t, forwardErr)
	assert.Equal(t,
		fmt.Errorf("error in ForwardQueryResult: Query %s is not registered in query forwarder", queryID.String()),
		forwardErr,
	)

	assert.NotNil(t, err)
	assert.Equal(t, fmt.Errorf("An error 1"), err)
}

func TestStreamResultsClientContextCancel(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	errCh := make(chan error)

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err := f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
		errCh <- err
	}()

	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	_, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	_, in2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	_, in3 := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in1))
	cancelConsumer()

	// It's ok if these error but they should not hang.
	fwdErr := f.ForwardQueryResult(producerCtx, in2)
	_ = fwdErr
	fwdErr = f.ForwardQueryResult(producerCtx, in3)
	_ = fwdErr
	wg.Wait()

	err := <-errCh
	require.NoError(t, err)
}

func TestStreamResultsQueryPlan(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	plan, planMap := makePlan(t)

	queryPlanOpts := &controllers.QueryPlanOpts{
		TableID: "query_plan_table_id",
		Plan:    plan,
		PlanMap: planMap,
	}
	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, queryPlanOpts, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	expected0, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected1, in1 := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	expected2, in2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	expected4, in3 := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in1))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in2))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in3))
	wg.Wait()

	require.NoError(t, err)
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
	assert.Contains(t, string(strData[0]), agent1ID)
	assert.Contains(t, string(strData[0]), agent2ID)

	// Check the other results.
	assert.Equal(t, expected0, results[0].GetData().Batch)
	assert.Equal(t, expected1, results[1].GetData().Batch)
	assert.Equal(t, expected2, results[2].GetData().Batch)
	assert.Equal(t, expected4, results[4].GetData().ExecutionStats)
}

func TestStreamResultsWrongQueryID(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())
	otherQueryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	firstResultCh := make(chan struct{})
	firstResultOnce := sync.Once{}

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
				firstResultOnce.Do(func() { close(firstResultCh) })
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	expected0, goodInput := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	_, badInput := makeRowBatchResult(t, otherQueryID, "bar", "456" /*eos*/, true)

	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, goodInput))

	// Make sure StreamResults has started before cancelling the query.
	timer := time.NewTimer(time.Second)
	select {
	case <-firstResultCh:
	case <-timer.C:
		assert.Fail(t, "Test timedout waiting for consumer to receive first result")
	}

	assert.NotNil(t, f.ForwardQueryResult(producerCtx, badInput))
	f.ProducerCancelStream(queryID, fmt.Errorf("An error"))
	wg.Wait()

	assert.NotNil(t, err)
	assert.Equal(t, err.Error(), "An error")
	assert.Equal(t, 1, len(results))
	assert.Equal(t, queryID.String(), results[0].QueryID)
	assert.Equal(t, expected0, results[0].GetData().Batch)
}

func TestStreamResultsResultsBeforeInitialization(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	_, in1 := makeExecStatsResult(t, queryID)
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in1))
	wg.Wait()

	assert.Nil(t, err)
	assert.Equal(t, 2, len(results))
}

func TestStreamResultsNeverInitializedTable(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	expected0, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))
	wg.Wait()

	assert.NotNil(t, err)
	assert.Equal(t, err.Error(), fmt.Sprintf("Query %s failed to initialize all result tables "+
		"within the deadline, missing: bar", queryID.String()))
	assert.Equal(t, 1, len(results))
	assert.Equal(t, queryID.String(), results[0].QueryID)
	assert.Equal(t, expected0, results[0].GetData().Batch)
}

func TestStreamResultsProducerTimeout(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithProducerTimeout(100 * time.Millisecond))

	var wg sync.WaitGroup
	wg.Add(1)
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	testTimedout := false

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	go func() {
		// Add timeout in case the producer timeout fails.
		t := time.NewTimer(30 * time.Second)
		for {
			select {
			case <-consumerCtx.Done():
				wg.Done()
				return
			case <-t.C:
				testTimedout = true
				wg.Done()
				return
			case msg := <-resultCh:
				results = append(results, msg)
				if !t.Stop() {
					<-t.C
				}
				t.Reset(30 * time.Second)
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)

	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, in0))
	// We never send EOS so we should get a producer timeout here.
	wg.Wait()

	require.False(t, testTimedout)
	assert.Error(t, err)
	assert.Equal(t, err.Error(), fmt.Sprintf("Query %s timedout waiting for producers", queryID.String()))
}

func TestStreamResultsNewConsumer(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results1 []*vizierpb.ExecuteScriptResponse
	var results2 []*vizierpb.ExecuteScriptResponse
	resultCh1 := make(chan *vizierpb.ExecuteScriptResponse)
	resultCh2 := make(chan *vizierpb.ExecuteScriptResponse)

	consumer1Ctx, cancelConsumer1 := context.WithCancel(context.Background())
	defer cancelConsumer1()
	consumer2Ctx, cancelConsumer2 := context.WithCancel(context.Background())
	defer cancelConsumer2()
	producerCtx, cancelProducer := context.WithCancel(context.Background())
	defer cancelProducer()

	firstResultCh := make(chan struct{})
	firstResultOnce := sync.Once{}

	wg.Add(1)
	go func() {
		for {
			select {
			case msg := <-resultCh1:
				results1 = append(results1, msg)
				firstResultOnce.Do(func() { close(firstResultCh) })
			case <-consumer1Ctx.Done():
				wg.Done()
				return
			}
		}
	}()
	var consumer1Err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		consumer1Err = f.StreamResults(consumer1Ctx, queryID, resultCh1)
		cancelConsumer1()
	}()

	expected0, goodInput := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected1, goodInput2 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected2, eosFoo := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	expected3, eosBar := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	expected4, execStats := makeExecStatsResult(t, queryID)

	assert.Nil(t, f.ForwardQueryResult(producerCtx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, goodInput))

	var consumer2Err error

	// Wait to ensure consumer1 registers before consumer2.
	timer := time.NewTimer(time.Second)
	select {
	case <-firstResultCh:
	case <-timer.C:
		assert.Fail(t, "Test timedout waiting for consumer1 to receive its first result.")
	}

	go func() {
		consumer2Err = f.StreamResults(consumer2Ctx, queryID, resultCh2)
		cancelConsumer2()
	}()

	// Wait for the first consumer to be cancelled.
	wg.Wait()

	wg.Add(1)
	go func() {
		for {
			select {
			case msg := <-resultCh2:
				results2 = append(results2, msg)
			case <-consumer2Ctx.Done():
				wg.Done()
				return
			}
		}
	}()

	assert.Nil(t, f.ForwardQueryResult(producerCtx, goodInput2))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, eosFoo))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, eosBar))
	assert.Nil(t, f.ForwardQueryResult(producerCtx, execStats))

	wg.Wait()

	// The first consumer should be cancelled without error.
	assert.Nil(t, consumer1Err)
	// The second consumer should finish without error.
	assert.Nil(t, consumer2Err)

	assert.Equal(t, 1, len(results1))
	assert.Equal(t, 4, len(results2))
	assert.Equal(t, queryID.String(), results1[0].QueryID)
	assert.Equal(t, queryID.String(), results2[0].QueryID)
	assert.Equal(t, expected0, results1[0].GetData().Batch)
	assert.Equal(t, expected1, results2[0].GetData().Batch)
	assert.Equal(t, expected2, results2[1].GetData().Batch)
	assert.Equal(t, expected3, results2[2].GetData().Batch)
	assert.Equal(t, expected4, results2[3].GetData().ExecutionStats)
}

func TestStreamResultsMultipleProducers(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producer1Ctx, cancelProducer1 := context.WithCancel(context.Background())
	defer cancelProducer1()
	producer2Ctx, cancelProducer2 := context.WithCancel(context.Background())
	defer cancelProducer2()

	wg.Add(1)
	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		cancelConsumer()
	}()

	expected0, goodInput := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	expected1, eosFoo := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, true)
	expected2, eosBar := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)
	expected3, execStats := makeExecStatsResult(t, queryID)

	globalProducerCtx, err := f.GetProducerCtx(queryID)
	assert.Nil(t, err)

	assert.Nil(t, f.ForwardQueryResult(producer1Ctx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producer2Ctx, goodInput))
	assert.Nil(t, f.ForwardQueryResult(producer2Ctx, eosFoo))
	assert.Nil(t, f.ForwardQueryResult(producer1Ctx, eosBar))
	assert.Nil(t, f.ForwardQueryResult(producer1Ctx, execStats))

	wg.Wait()

	timer := time.NewTimer(time.Second)
	select {
	case <-globalProducerCtx.Done():
	case <-timer.C:
		assert.Fail(t, "Producer context was not cancelled at end of query.")
	}

	assert.Nil(t, err)
	assert.Equal(t, 4, len(results))
	assert.Equal(t, queryID.String(), results[0].QueryID)
	assert.Equal(t, expected0, results[0].GetData().Batch)
	assert.Equal(t, expected1, results[1].GetData().Batch)
	assert.Equal(t, expected2, results[2].GetData().Batch)
	assert.Equal(t, expected3, results[3].GetData().ExecutionStats)
}

func TestStreamResultsReceiveExecutionError(t *testing.T) {
	tests := []struct {
		name                string
		createTestFunctions func(t *testing.T, queryID uuid.UUID) (func(context.Context, controllers.QueryResultForwarder), func(results []*vizierpb.ExecuteScriptResponse, err error))
	}{
		{
			name: "error_before_init",
			createTestFunctions: func(t *testing.T, queryID uuid.UUID) (func(context.Context, controllers.QueryResultForwarder), func(results []*vizierpb.ExecuteScriptResponse, err error)) {
				errorStatus := &statuspb.Status{
					ErrCode: statuspb.INTERNAL,
					Msg:     "error sending tables",
				}
				execError := &carnotpb.TransferResultChunkRequest{
					Address: "foo",
					QueryID: utils.ProtoFromUUID(queryID),
					Result: &carnotpb.TransferResultChunkRequest_ExecutionError{
						ExecutionError: errorStatus,
					},
				}
				sendRequests := func(ctx context.Context, f controllers.QueryResultForwarder) {
					assert.Nil(t, f.ForwardQueryResult(ctx, execError))
				}

				checkResults := func(results []*vizierpb.ExecuteScriptResponse, err error) {
					// We expect an error.
					assert.Regexp(t, "error sending tables", err.Error())

					for _, result := range results {
						assert.Equal(t, queryID.String(), result.QueryID)
					}

					assert.Equal(t, 1, len(results))
					assert.Equal(t, controllers.StatusToVizierStatus(errorStatus), results[0].GetStatus())
				}
				return sendRequests, checkResults
			},
		},
		{
			name: "error_after_data",
			createTestFunctions: func(t *testing.T, queryID uuid.UUID) (func(context.Context, controllers.QueryResultForwarder), func(results []*vizierpb.ExecuteScriptResponse, err error)) {
				expected0, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)

				errorStatus := &statuspb.Status{
					ErrCode: statuspb.INTERNAL,
					Msg:     "error sending tables",
				}
				execError := &carnotpb.TransferResultChunkRequest{
					Address: "foo",
					QueryID: utils.ProtoFromUUID(queryID),
					Result: &carnotpb.TransferResultChunkRequest_ExecutionError{
						ExecutionError: errorStatus,
					},
				}
				sendRequests := func(ctx context.Context, f controllers.QueryResultForwarder) {
					assert.Nil(t, f.ForwardQueryResult(ctx, makeInitiateConnectionRequest(queryID)))
					assert.Nil(t, f.ForwardQueryResult(ctx, in0))
					assert.Nil(t, f.ForwardQueryResult(ctx, execError))
				}

				checkResults := func(results []*vizierpb.ExecuteScriptResponse, err error) {
					// We expect an error.
					assert.Regexp(t, "error sending tables", err.Error())

					for _, result := range results {
						assert.Equal(t, queryID.String(), result.QueryID)
					}

					assert.Equal(t, 2, len(results))
					assert.Equal(t, expected0, results[0].GetData().Batch)
					assert.Equal(t, controllers.StatusToVizierStatus(errorStatus), results[1].GetStatus())
				}
				return sendRequests, checkResults
			},
		},
		{
			name: "data_after_error",
			createTestFunctions: func(t *testing.T, queryID uuid.UUID) (func(context.Context, controllers.QueryResultForwarder), func(results []*vizierpb.ExecuteScriptResponse, err error)) {
				_, in0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)

				errorStatus := &statuspb.Status{
					ErrCode: statuspb.INTERNAL,
					Msg:     "error sending tables",
				}
				execError := &carnotpb.TransferResultChunkRequest{
					Address: "foo",
					QueryID: utils.ProtoFromUUID(queryID),
					Result: &carnotpb.TransferResultChunkRequest_ExecutionError{
						ExecutionError: errorStatus,
					},
				}
				sendRequests := func(ctx context.Context, f controllers.QueryResultForwarder) {
					assert.Nil(t, f.ForwardQueryResult(ctx, makeInitiateConnectionRequest(queryID)))
					assert.Nil(t, f.ForwardQueryResult(ctx, execError))
					// This call has a chance of failing based on a race, so we don't check the result.
					// This is because during an error we don't really care what is sent over for the query ID anymore.
					// The ultimate effect is that the sender ie a Carnot instance will receive an error on their side.
					_ = f.ForwardQueryResult(ctx, in0)
				}

				checkResults := func(results []*vizierpb.ExecuteScriptResponse, err error) {
					// We expect an error.
					assert.Regexp(t, "error sending tables", err.Error())

					for _, result := range results {
						assert.Equal(t, queryID.String(), result.QueryID)
					}

					assert.Equal(t, 1, len(results))
					assert.Equal(t, controllers.StatusToVizierStatus(errorStatus), results[0].GetStatus())
				}
				return sendRequests, checkResults
			},
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			queryID := uuid.Must(uuid.NewV4())

			f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

			var wg sync.WaitGroup
			wg.Add(1)
			expectedTables := make(map[string]string)
			expectedTables["foo"] = "123"

			var results []*vizierpb.ExecuteScriptResponse
			resultCh := make(chan *vizierpb.ExecuteScriptResponse)

			consumerCtx, cancelConsumer := context.WithCancel(context.Background())
			defer cancelConsumer()
			producerCtx, cancelProducer := context.WithCancel(context.Background())
			defer cancelProducer()

			go func() {
				for {
					select {
					case msg := <-resultCh:
						results = append(results, msg)
					case <-consumerCtx.Done():
						wg.Done()
						return
					}
				}
			}()
			var err error

			assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

			go func() {
				err = f.StreamResults(consumerCtx, queryID, resultCh)
				assert.Regexp(t, "error sending tables", err.Error())
				cancelConsumer()
			}()

			sendRequests, checkResults := test.createTestFunctions(t, queryID)

			sendRequests(producerCtx, f)
			wg.Wait()
			checkResults(results, err)
		})
	}
}

func TestStreamErrorWithMultipleProducers(t *testing.T) {
	queryID := uuid.Must(uuid.NewV4())

	f := controllers.NewQueryResultForwarderWithOptions(controllers.WithResultSinkTimeout(1 * time.Second))

	var wg sync.WaitGroup
	expectedTables := make(map[string]string)
	expectedTables["foo"] = "123"
	expectedTables["bar"] = "456"

	var results []*vizierpb.ExecuteScriptResponse
	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	consumerCtx, cancelConsumer := context.WithCancel(context.Background())
	defer cancelConsumer()
	producer1Ctx, cancelProducer1 := context.WithCancel(context.Background())
	defer cancelProducer1()
	producer2Ctx, cancelProducer2 := context.WithCancel(context.Background())
	defer cancelProducer2()

	wg.Add(1)
	go func() {
		for {
			select {
			case msg := <-resultCh:
				results = append(results, msg)
			case <-consumerCtx.Done():
				wg.Done()
				return
			}
		}
	}()
	var err error

	assert.Nil(t, f.RegisterQuery(queryID, expectedTables, 350, nil, ""))

	go func() {
		err = f.StreamResults(consumerCtx, queryID, resultCh)
		assert.Regexp(t, "error sending tables", err.Error())
		cancelConsumer()
	}()

	errorStatus := &statuspb.Status{
		ErrCode: statuspb.INTERNAL,
		Msg:     "error sending tables",
	}
	execError := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: utils.ProtoFromUUID(queryID),
		Result: &carnotpb.TransferResultChunkRequest_ExecutionError{
			ExecutionError: errorStatus,
		},
	}

	expected0, foo0 := makeRowBatchResult(t, queryID, "foo", "123" /*eos*/, false)
	_, eosBar := makeRowBatchResult(t, queryID, "bar", "456" /*eos*/, true)

	globalProducerCtx, err := f.GetProducerCtx(queryID)
	assert.Nil(t, err)

	assert.Nil(t, f.ForwardQueryResult(producer1Ctx, makeInitiateConnectionRequest(queryID)))
	assert.Nil(t, f.ForwardQueryResult(producer2Ctx, foo0))
	assert.Nil(t, f.ForwardQueryResult(producer2Ctx, execError))
	// Don't check because the race might close producer before anything else.
	_ = f.ForwardQueryResult(producer1Ctx, eosBar)

	wg.Wait()

	timer := time.NewTimer(time.Second)
	select {
	case <-globalProducerCtx.Done():
	case <-timer.C:
		assert.Fail(t, "Producer context was not cancelled at end of query.")
	}

	assert.Regexp(t, "error sending tables", err.Error())
	assert.Equal(t, 2, len(results))
	assert.Equal(t, queryID.String(), results[0].QueryID)
	assert.Equal(t, expected0, results[0].GetData().Batch)
	assert.Equal(t, controllers.StatusToVizierStatus(errorStatus), results[1].GetStatus())
}
