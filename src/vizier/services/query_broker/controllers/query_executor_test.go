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
	"testing"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/vizier/services/metadata/metadatapb"

	"px.dev/pixie/src/common/base/statuspb"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
	mock_controllers "px.dev/pixie/src/vizier/services/query_broker/controllers/mock"
	"px.dev/pixie/src/vizier/services/query_broker/tracker"
)

const singleAgentDistributedState = `
distributed_state: {
	carnot_info: {
		query_broker_address: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
		agent_id {
			high_bits:  0x21285cdd1de94ab1
			low_bits: 0xae6a0ba08c8c676c
		}
		has_data_store: true
		processes_data: true
		asid: 123
		metadata_info {
			metadata_fields: CONTAINER_ID
			metadata_fields: SERVICE_NAME
			xxhash64_bloom_filter {
				num_hashes: 2
				data: "1234"
			}
		}
	}
	schema_info {
		name: "perf_and_http"
		relation: {
				columns: {
					column_name: "_time"
				}
		}
		agent_list {
			high_bits: 0x21285cdd1de94ab1
			low_bits: 0xae6a0ba08c8c676c
		}
	}
}
plan_options: {
	explain: false
	analyze: false
	max_output_rows_per_table: 10000
}
result_address: "qb_address"
result_ssl_targetname: "qb_hostname"
debug_info: {
	otel_debug_attributes {
		name: "px.cloud.address"
		value: ""
	}
}
`

const testQuery = `
df = dataframe(table='perf_and_http', select=['_time'])
display(df, 'out')
`

const badQuery = `
df = dataframe(table='', select=['_time'])
display(df, 'out')
`

const expectedPlannerResult = `
status: {}
plan: {
	qb_address_to_plan: {
		key: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
		value: {
			dag: {
				nodes: {
					id: 1
				}
			}
			nodes: {
				id: 1
				dag: {
					nodes: {
						id: 3
						sorted_children: 0
					}
					nodes: {
						sorted_parents: 3
					}
				}
				nodes: {
					id: 3
					op: {
						op_type: MEMORY_SOURCE_OPERATOR
						mem_source_op: {
							name: "table1"
							column_idxs: 0
							column_idxs: 1
							column_idxs: 2
							column_names: "time_"
							column_names: "cpu_cycles"
							column_names: "upid"
							column_types: TIME64NS
							column_types: INT64
							column_types: UINT128
							tablet: "1"
						}
					}
				}
				nodes: {
					op: {
						op_type: GRPC_SINK_OPERATOR
						grpc_sink_op: {
							address: "foo"
							output_table {
								table_name: "agent1_table"
								column_types: TIME64NS
								column_types: INT64
								column_types: UINT128
								column_names: "time_"
								column_names: "cpu_cycles"
								column_names: "upid"
								column_semantic_types: ST_NONE
								column_semantic_types: ST_NONE
								column_semantic_types: ST_UPID
							}
						}
					}
				}
			}
		}
	}
	qb_address_to_plan: {
		key: "31285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
		value: {
			dag: {
				nodes: {
					id: 1
				}
			}
			nodes: {
				id: 1
				dag: {
					nodes: {
						id: 3
						sorted_children: 0
					}
					nodes: {
						sorted_parents: 3
					}
				}
				nodes: {
					id: 3
					op: {
						op_type: MEMORY_SOURCE_OPERATOR
						mem_source_op: {
							name: "table1"
							column_idxs: 0
							column_names: "time_"
							column_types: TIME64NS
							tablet: "1"
						}
					}
				}
				nodes: {
					op: {
						op_type: GRPC_SINK_OPERATOR
						grpc_sink_op: {
							address: "bar"
							output_table {
								table_name: "agent2_table"
								column_types: TIME64NS
								column_names: "time_"
								column_semantic_types: ST_NONE
							}
						}
					}
				}
			}
		}
	}
	qb_address_to_dag_id: {
		key: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
		value: 0
	}
	qb_address_to_dag_id: {
		key: "31285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
		value: 1
	}
	dag: {
		nodes: {
			id: 0
		}
		nodes: {
			id: 1
		}
	}
}
`

const failedPlannerResult = `
status{
	err_code: INVALID_ARGUMENT
	context {
		[type.googleapis.com/px.carnot.planner.compilerpb.CompilerErrorGroup] {
			errors {
				line_col_error {
					line: 1
					column: 2
					message: "Error ova here."
				}
			}
			errors {
				line_col_error {
					line: 20
					column: 19
					message: "Error ova there."
				}
			}
		}
	}
}`

const failedPlannerResultFromStatus = `
status{
	msg: "failure failure failure"
	err_code: INVALID_ARGUMENT
}`

type fakeAgentsTracker struct {
	agentsInfo tracker.AgentsInfo
}

func (f *fakeAgentsTracker) GetAgentInfo() tracker.AgentsInfo {
	return f.agentsInfo
}

type fakeResultForwarder struct {
	// Variables to pass in for ExecuteScript testing.
	ClientResultsToSend []*vizierpb.ExecuteScriptResponse
	Error               error

	// Variables that will get set during ExecuteScriptTesting.
	QueryRegistered       uuid.UUID
	TableIDMap            map[string]string
	QueryDeleted          uuid.UUID
	QueryStreamed         uuid.UUID
	StreamedQueryPlanOpts *controllers.QueryPlanOpts

	// Variables to set/use for TransferResultChunk testing.
	ClientStreamClosed   bool
	ClientStreamError    error
	ReceivedAgentResults []*carnotpb.TransferResultChunkRequest
}

// RegisterQuery registers a query.
func (f *fakeResultForwarder) RegisterQuery(queryID uuid.UUID, tableIDMap map[string]string,
	compilationTimeNs int64,
	queryPlanOpts *controllers.QueryPlanOpts, queryName string) error {
	f.QueryRegistered = queryID
	f.TableIDMap = tableIDMap
	f.StreamedQueryPlanOpts = queryPlanOpts
	return nil
}

// StreamResults streams the results to the resultCh.
func (f *fakeResultForwarder) StreamResults(ctx context.Context, queryID uuid.UUID,
	resultCh chan<- *vizierpb.ExecuteScriptResponse) error {
	f.QueryStreamed = queryID

	for _, expectedResult := range f.ClientResultsToSend {
		select {
		case <-ctx.Done():
			return nil
		case resultCh <- expectedResult:
		}
	}
	return f.Error
}

// GetProducerCtx returns the producer context for a queryID.
func (f *fakeResultForwarder) GetProducerCtx(queryID uuid.UUID) (context.Context, error) {
	return context.Background(), nil
}

// ForwardQueryResult forwards the agent result to the client result stream.
func (f *fakeResultForwarder) ForwardQueryResult(ctx context.Context, msg *carnotpb.TransferResultChunkRequest) error {
	if f.ClientStreamClosed {
		return f.ClientStreamError
	}
	f.ReceivedAgentResults = append(f.ReceivedAgentResults, msg)
	return nil
}

// ProducerCancelStream cancels the query when the producer side encounters an error.
func (f *fakeResultForwarder) ProducerCancelStream(queryID uuid.UUID, err error) {
	f.ClientStreamError = err
	f.ClientStreamClosed = true
}

type queryExecTestCase struct {
	Name                       string
	Req                        *vizierpb.ExecuteScriptRequest
	ResultForwarderResps       []*vizierpb.ExecuteScriptResponse
	ExpectedResps              []*vizierpb.ExecuteScriptResponse
	TableNames                 []string
	PlannerState               *distributedpb.LogicalPlannerState
	ExpectedPlannerResult      *distributedpb.LogicalPlannerResult
	QueryExecExpectedRunError  error
	QueryExecExpectedWaitError error
	ConsumeErrs                []error
	StreamResultsErr           error
	StreamResultsCallExpected  bool
	MutExecFactory             controllers.MutationExecFactory
}

type testConsumer struct {
	errorsToReturn []error
	results        []*vizierpb.ExecuteScriptResponse
	i              int32
}

func newTestConsumer(errorsToReturn []error) *testConsumer {
	return &testConsumer{
		results:        make([]*vizierpb.ExecuteScriptResponse, 0),
		errorsToReturn: errorsToReturn,
		i:              0,
	}
}

func (c *testConsumer) Consume(result *vizierpb.ExecuteScriptResponse) error {
	defer func() { c.i++ }()
	c.results = append(c.results, result)
	if c.errorsToReturn != nil {
		return c.errorsToReturn[c.i]
	}
	return nil
}

type fakeDataPrivacy struct {
	Options *distributedpb.RedactionOptions
}

// RedactionOptions returns RedactionOptions proto message that was set on the fakeDataPrivacy struct.
func (fdp *fakeDataPrivacy) RedactionOptions(_ context.Context) (*distributedpb.RedactionOptions, error) {
	return fdp.Options, nil
}

func runTestCase(t *testing.T, test *queryExecTestCase) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	agentsInfo := tracker.NewTestAgentsInfo(test.PlannerState.DistributedState)

	at := &fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	rf := &fakeResultForwarder{
		ClientResultsToSend: test.ResultForwarderResps,
		Error:               test.StreamResultsErr,
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	if test.Req.QueryID == "" {
		planner.EXPECT().
			Plan(gomock.Any()).
			Return(test.ExpectedPlannerResult, nil)
	}

	dp := &fakeDataPrivacy{}
	queryExec := controllers.NewQueryExecutor("qb_address", "qb_hostname", at, dp, nc, nil, nil, rf, planner, test.MutExecFactory)
	consumer := newTestConsumer(test.ConsumeErrs)

	assert.Equal(t, test.QueryExecExpectedRunError, queryExec.Run(context.Background(), test.Req, consumer))
	assert.Equal(t, test.QueryExecExpectedWaitError, queryExec.Wait())

	require.Equalf(t, len(test.ExpectedResps)+len(test.TableNames), len(consumer.results), "query executor sent incorrect number of results to consumer")

	actualTableNames := make(map[string]bool)
	for _, result := range consumer.results {
		if result.GetMetaData() != nil {
			actualTableNames[result.GetMetaData().Name] = true
		}
	}
	for _, tableName := range test.TableNames {
		_, ok := actualTableNames[tableName]
		assert.True(t, ok)
	}

	idx := 0
	for _, result := range consumer.results {
		if result.GetData() != nil {
			// Ignore resp.QueryID field
			assert.Equal(t, test.ExpectedResps[idx].Status, result.Status)
			assert.Equal(t, test.ExpectedResps[idx].Result, result.Result)
			idx++
		}
	}

	if test.StreamResultsCallExpected {
		assert.NotEqualf(t, uuid.Nil, rf.QueryStreamed, "Expected StreamResults to be called but it wasn't")
	} else {
		assert.Equalf(t, uuid.Nil, rf.QueryStreamed, "Expected StreamResults not to be called but it was")
	}
}

func TestQueryExecutor(t *testing.T) {
	tests := []queryExecTestCase{
		buildSimpleSuccessTestCase(t),
		buildPlannerErrorTestCase(t),
		buildPlannerErrorTestCaseStatusMessage(t),
		buildPlanInvalidUUIDTestCase(t),
		buildConsumeErrorTestCase(t),
		buildStreamResultErrorTestCase(t),
		buildResumeQueryTestCase(t),
		buildResumeQueryBadQueryIDTestCase(t),
		buildMutationFailedQueryTestCase(t),
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) { runTestCase(t, &test) })
	}
}

func buildPlannerState(t *testing.T, plannerStateStr string) *distributedpb.LogicalPlannerState {
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(plannerStateStr, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	return plannerStatePB
}

func buildPlannerResult(t *testing.T, resultStr string) *distributedpb.LogicalPlannerResult {
	plannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(resultStr, plannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	return plannerResultPB
}

func buildSimpleSuccessTestCase(t *testing.T) queryExecTestCase {
	fakeBatch := new(vizierpb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, fakeBatch); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: fakeBatch,
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				ExecutionStats: &vizierpb.QueryExecutionStats{
					Timing: &vizierpb.QueryTimingInfo{
						ExecutionTimeNs:   5010,
						CompilationTimeNs: 350,
					},
					BytesProcessed:   4521,
					RecordsProcessed: 4,
				},
			},
		},
	}
	return queryExecTestCase{
		Name: "successful execute",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: testQuery,
		},
		ResultForwarderResps: []*vizierpb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
		ExpectedResps: []*vizierpb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
		TableNames:                 []string{"agent1_table", "agent2_table"},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, expectedPlannerResult),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: nil,
		StreamResultsCallExpected:  true,
	}
}

func buildPlannerErrorTestCase(t *testing.T) queryExecTestCase {
	errResp := &vizierpb.ExecuteScriptResponse{
		Status: &vizierpb.Status{
			Code:    int32(statuspb.INVALID_ARGUMENT),
			Message: "",
			ErrorDetails: []*vizierpb.ErrorDetails{
				{
					Error: &vizierpb.ErrorDetails_CompilerError{
						CompilerError: &vizierpb.CompilerError{
							Line:    1,
							Column:  2,
							Message: "Error ova here.",
						},
					},
				},
				{
					Error: &vizierpb.ErrorDetails_CompilerError{
						CompilerError: &vizierpb.CompilerError{
							Line:    20,
							Column:  19,
							Message: "Error ova there.",
						},
					},
				},
			},
		},
	}
	return queryExecTestCase{
		Name: "planner error",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: badQuery,
		},
		ResultForwarderResps:       []*vizierpb.ExecuteScriptResponse{},
		ExpectedResps:              []*vizierpb.ExecuteScriptResponse{errResp},
		TableNames:                 []string{},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, failedPlannerResult),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: controllers.VizierStatusToError(errResp.Status),
		StreamResultsCallExpected:  false,
	}
}

func buildPlannerErrorTestCaseStatusMessage(t *testing.T) queryExecTestCase {
	errResp := &vizierpb.ExecuteScriptResponse{
		Status: &vizierpb.Status{
			Code:    int32(statuspb.INVALID_ARGUMENT),
			Message: "failure failure failure",
		},
	}
	return queryExecTestCase{
		Name: "planner status message error",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: badQuery,
		},
		ResultForwarderResps:       []*vizierpb.ExecuteScriptResponse{},
		ExpectedResps:              []*vizierpb.ExecuteScriptResponse{errResp},
		TableNames:                 []string{},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, failedPlannerResultFromStatus),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: controllers.VizierStatusToError(errResp.Status),
		StreamResultsCallExpected:  false,
	}
}

func buildPlanInvalidUUIDTestCase(t *testing.T) queryExecTestCase {
	badPlannerResult := &distributedpb.LogicalPlannerResult{
		Status: &statuspb.Status{ErrCode: statuspb.OK},
		Plan: &distributedpb.DistributedPlan{
			QbAddressToPlan: map[string]*planpb.Plan{
				"not a uuid": {},
			},
		},
	}

	return queryExecTestCase{
		Name: "invalid plan carnotID",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: badQuery,
		},
		ResultForwarderResps:       []*vizierpb.ExecuteScriptResponse{},
		ExpectedResps:              []*vizierpb.ExecuteScriptResponse{},
		TableNames:                 []string{},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      badPlannerResult,
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: fmt.Errorf("uuid: incorrect UUID length 10 in string \"not a uuid\""),
		StreamResultsCallExpected:  false,
	}
}

func buildConsumeErrorTestCase(t *testing.T) queryExecTestCase {
	consumeErr := fmt.Errorf("error in consume")

	return queryExecTestCase{
		Name: "consume error",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: testQuery,
		},
		ResultForwarderResps: []*vizierpb.ExecuteScriptResponse{
			// These responses should not be received by the consumer.
			{},
			{},
		},
		ExpectedResps:              []*vizierpb.ExecuteScriptResponse{},
		TableNames:                 []string{"agent1_table", "agent2_table"},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, expectedPlannerResult),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: consumeErr,
		ConsumeErrs:                []error{nil, consumeErr},
		StreamResultsCallExpected:  true,
	}
}

func buildStreamResultErrorTestCase(t *testing.T) queryExecTestCase {
	fakeBatch := new(vizierpb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, fakeBatch); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: fakeBatch,
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				ExecutionStats: &vizierpb.QueryExecutionStats{
					Timing: &vizierpb.QueryTimingInfo{
						ExecutionTimeNs:   5010,
						CompilationTimeNs: 350,
					},
					BytesProcessed:   4521,
					RecordsProcessed: 4,
				},
			},
		},
	}
	err := fmt.Errorf("stream results error")
	return queryExecTestCase{
		Name: "stream results error",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: testQuery,
		},
		ResultForwarderResps: []*vizierpb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
		ExpectedResps: []*vizierpb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
		TableNames:                 []string{"agent1_table", "agent2_table"},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, expectedPlannerResult),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: err,
		StreamResultsErr:           err,
		StreamResultsCallExpected:  true,
	}
}

func buildResumeQueryTestCase(t *testing.T) queryExecTestCase {
	fakeBatch := new(vizierpb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, fakeBatch); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: fakeBatch,
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				ExecutionStats: &vizierpb.QueryExecutionStats{
					Timing: &vizierpb.QueryTimingInfo{
						ExecutionTimeNs:   5010,
						CompilationTimeNs: 350,
					},
					BytesProcessed:   4521,
					RecordsProcessed: 4,
				},
			},
		},
	}
	queryID, err := uuid.NewV4()
	require.Nil(t, err)

	return queryExecTestCase{
		Name: "resume by queryID",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryID: queryID.String(),
		},
		ResultForwarderResps: []*vizierpb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
		ExpectedResps: []*vizierpb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
		// Resuming should not send table relation responses again.
		TableNames:                 []string{},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, expectedPlannerResult),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: nil,
		StreamResultsCallExpected:  true,
	}
}

func buildResumeQueryBadQueryIDTestCase(t *testing.T) queryExecTestCase {
	return queryExecTestCase{
		Name: "resume query bad query ID",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryID: "Not a UUID but is 36 chars longXXXXX",
		},
		ResultForwarderResps: []*vizierpb.ExecuteScriptResponse{
			// This should not be sent.
			{},
		},
		ExpectedResps: []*vizierpb.ExecuteScriptResponse{},
		// Resuming should not send table relation responses again.
		TableNames:                 []string{},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, expectedPlannerResult),
		QueryExecExpectedRunError:  fmt.Errorf("uuid: incorrect UUID format in string \"Not a UUID but is 36 chars longXXXXX\""),
		QueryExecExpectedWaitError: nil,
		StreamResultsCallExpected:  false,
	}
}

type fakeMutationExecutor struct {
	MutInfo       *vizierpb.MutationInfo
	ExecuteStatus *statuspb.Status
	ExecuteError  error
}

func (m *fakeMutationExecutor) Execute(ctx context.Context, req *vizierpb.ExecuteScriptRequest, planOpts *planpb.PlanOptions) (*statuspb.Status, error) {
	return m.ExecuteStatus, m.ExecuteError
}

func (m *fakeMutationExecutor) MutationInfo(ctx context.Context) (*vizierpb.MutationInfo, error) {
	return m.MutInfo, nil
}

func buildMutationFailedQueryTestCase(t *testing.T) queryExecTestCase {
	err := fmt.Errorf("fail query no schema")
	mutInfo := &vizierpb.MutationInfo{Status: &vizierpb.Status{Code: 0}}
	return queryExecTestCase{
		Name: "mutation failed query",
		Req: &vizierpb.ExecuteScriptRequest{
			QueryStr: testQuery,
			Mutation: true,
		},
		ResultForwarderResps: []*vizierpb.ExecuteScriptResponse{},
		ExpectedResps: []*vizierpb.ExecuteScriptResponse{
			{
				Status:       &vizierpb.Status{Code: 0},
				MutationInfo: mutInfo,
			},
		},
		TableNames:                 []string{"agent1_table", "agent2_table"},
		PlannerState:               buildPlannerState(t, singleAgentDistributedState),
		ExpectedPlannerResult:      buildPlannerResult(t, expectedPlannerResult),
		QueryExecExpectedRunError:  nil,
		QueryExecExpectedWaitError: err,
		StreamResultsErr:           err,
		StreamResultsCallExpected:  true,
		MutExecFactory: func(planner controllers.Planner, client metadatapb.MetadataTracepointServiceClient, client2 metadatapb.MetadataConfigServiceClient, state *distributedpb.DistributedState) controllers.MutationExecutor {
			return &fakeMutationExecutor{
				MutInfo:       mutInfo,
				ExecuteStatus: nil,
				ExecuteError:  nil,
			}
		},
	}
}
