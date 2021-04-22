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
	"io"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	public_vizierapipb "px.dev/pixie/src/api/proto/vizierapipb"
	mock_public_vizierapipb "px.dev/pixie/src/api/proto/vizierapipb/mock"
	"px.dev/pixie/src/carnot/carnotpb"
	mock_carnotpb "px.dev/pixie/src/carnot/carnotpb/mock"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/queryresultspb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
	"px.dev/pixie/src/vizier/services/query_broker/controllers"
	mock_controllers "px.dev/pixie/src/vizier/services/query_broker/controllers/mock"
	"px.dev/pixie/src/vizier/services/query_broker/querybrokerenv"
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
	ClientResultsToSend []*public_vizierapipb.ExecuteScriptResponse
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
func (f *fakeResultForwarder) RegisterQuery(queryID uuid.UUID, tableIDMap map[string]string) error {
	f.QueryRegistered = queryID
	f.TableIDMap = tableIDMap
	return nil
}

// DeleteQuery deletes a query/
func (f *fakeResultForwarder) DeleteQuery(queryID uuid.UUID) {
}

// StreamResults streams the results to the resultCh.
func (f *fakeResultForwarder) StreamResults(ctx context.Context, queryID uuid.UUID,
	resultCh chan *public_vizierapipb.ExecuteScriptResponse,
	compilationTimeNs int64,
	queryPlanOpts *controllers.QueryPlanOpts) error {
	f.StreamedQueryPlanOpts = queryPlanOpts
	f.QueryStreamed = queryID

	for _, expectedResult := range f.ClientResultsToSend {
		resultCh <- expectedResult
	}
	return f.Error
}

// ForwardQueryResult forwards the agent result to the client result stream.
func (f *fakeResultForwarder) ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error {
	if f.ClientStreamClosed {
		return f.ClientStreamError
	}
	f.ReceivedAgentResults = append(f.ReceivedAgentResults, msg)
	return nil
}

// CloseQueryResultStream closes both the client and agent side streams if either side terminates.
func (f *fakeResultForwarder) OptionallyCancelClientStream(queryID uuid.UUID, err error) {
	f.ClientStreamError = err
	f.ClientStreamClosed = true
}

func TestCheckHealth_Success(t *testing.T) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	queryID := uuid.Must(uuid.NewV4())
	fakeResult := &public_vizierapipb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &public_vizierapipb.ExecuteScriptResponse_Data{
			Data: &public_vizierapipb.QueryData{
				Batch: &public_vizierapipb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*public_vizierapipb.Column{
						{
							ColData: &public_vizierapipb.Column_StringData{
								StringData: &public_vizierapipb.StringColumn{
									Data: []string{
										"foo",
									},
								},
							},
						},
					},
					NumRows: 1,
					Eow:     true,
					Eos:     true,
				},
			},
		},
	}

	rf := &fakeResultForwarder{
		ClientResultsToSend: []*public_vizierapipb.ExecuteScriptResponse{
			fakeResult,
		},
	}

	// Not the actual health check query, but that's okay for the test since the planner and
	// query execution are mocked out.
	plannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(plannerResultPB, nil)

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nil, nc, planner)
	require.NoError(t, err)
	err = s.CheckHealth(context.Background())
	// Should pass.
	require.NoError(t, err)
}

func TestCheckHealth_CompilationError(t *testing.T) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	rf := &fakeResultForwarder{
		ClientResultsToSend: []*public_vizierapipb.ExecuteScriptResponse{},
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(nil, fmt.Errorf("some compiler error"))

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nil, nc, planner)
	require.NoError(t, err)
	err = s.CheckHealth(context.Background())
	// Should not pass.
	assert.NotNil(t, err)
}

func TestHealthCheck_ExecutionError(t *testing.T) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	// Receiving no results should cause an error.
	rf := &fakeResultForwarder{
		ClientResultsToSend: []*public_vizierapipb.ExecuteScriptResponse{},
	}

	// Not the actual health check query, but that's okay for the test since the planner and
	// query execution are mocked out.
	plannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(plannerResultPB, nil)

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nil, nc, planner)
	require.NoError(t, err)

	err = s.CheckHealth(context.Background())
	// Should not pass.
	assert.NotNil(t, err)
}

func TestExecuteScript_Success(t *testing.T) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)

	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	queryID := uuid.Must(uuid.NewV4())

	fakeBatch := new(public_vizierapipb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, fakeBatch); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	fakeResult1 := &public_vizierapipb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &public_vizierapipb.ExecuteScriptResponse_Data{
			Data: &public_vizierapipb.QueryData{
				Batch: fakeBatch,
			},
		},
	}
	fakeResult2 := &public_vizierapipb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &public_vizierapipb.ExecuteScriptResponse_Data{
			Data: &public_vizierapipb.QueryData{
				ExecutionStats: &public_vizierapipb.QueryExecutionStats{
					Timing: &public_vizierapipb.QueryTimingInfo{
						ExecutionTimeNs:   5010,
						CompilationTimeNs: 350,
					},
					BytesProcessed:   4521,
					RecordsProcessed: 4,
				},
			},
		},
	}

	rf := &fakeResultForwarder{
		ClientResultsToSend: []*public_vizierapipb.ExecuteScriptResponse{
			fakeResult1,
			fakeResult2,
		},
	}

	plannerResultPB := &distributedpb.LogicalPlannerResult{}
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(plannerResultPB, nil)

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nil, nc, planner)
	require.NoError(t, err)

	srv := mock_public_vizierapipb.NewMockVizierService_ExecuteScriptServer(ctrl)
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)

	srv.EXPECT().Context().Return(ctx).AnyTimes()

	var resps []*public_vizierapipb.ExecuteScriptResponse
	srv.EXPECT().
		Send(gomock.Any()).
		DoAndReturn(func(arg *public_vizierapipb.ExecuteScriptResponse) error {
			resps = append(resps, arg)
			return nil
		}).
		AnyTimes()

	err = s.ExecuteScript(&public_vizierapipb.ExecuteScriptRequest{
		QueryStr: testQuery,
	}, srv)

	require.NoError(t, err)
	assert.Equal(t, 4, len(resps))

	assert.Equal(t, 2, len(rf.TableIDMap))
	assert.NotEqual(t, 0, len(rf.TableIDMap["agent1_table"]))
	assert.NotEqual(t, 0, len(rf.TableIDMap["agent2_table"]))

	// Make sure the relation is sent with the metadata.
	actualSchemaResults := make(map[string]*public_vizierapipb.ExecuteScriptResponse)
	actualSchemaResults[resps[0].GetMetaData().Name] = resps[0]
	actualSchemaResults[resps[1].GetMetaData().Name] = resps[1]
	assert.Equal(t, 2, len(actualSchemaResults))
	assert.NotNil(t, actualSchemaResults["agent1_table"])
	assert.NotNil(t, actualSchemaResults["agent2_table"])

	assert.Equal(t, fakeResult1, resps[2])
	assert.Equal(t, fakeResult2, resps[3])
}

// TestExecuteScript_PlannerErrorResult makes sure that compiler error handling is done well.
func TestExecuteScript_PlannerErrorResult(t *testing.T) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)

	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	rf := &fakeResultForwarder{}

	badPlannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(failedPlannerResult, badPlannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf failedPlannerResult", failedPlannerResult)
	}
	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(badPlannerResultPB, nil)

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nil, nc, planner)
	require.NoError(t, err)

	srv := mock_public_vizierapipb.NewMockVizierService_ExecuteScriptServer(ctrl)
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)

	srv.EXPECT().Context().Return(ctx).AnyTimes()

	var resp *public_vizierapipb.ExecuteScriptResponse
	srv.EXPECT().
		Send(gomock.Any()).
		DoAndReturn(func(arg *public_vizierapipb.ExecuteScriptResponse) error {
			resp = arg
			return nil
		})

	err = s.ExecuteScript(&public_vizierapipb.ExecuteScriptRequest{
		QueryStr: badQuery,
	}, srv)

	require.NoError(t, err)
	assert.Equal(t, int32(3), resp.Status.Code)
	assert.Equal(t, "", resp.Status.Message)
	assert.Equal(t, 2, len(resp.Status.ErrorDetails))
	assert.Equal(t, &public_vizierapipb.ErrorDetails{
		Error: &public_vizierapipb.ErrorDetails_CompilerError{
			CompilerError: &public_vizierapipb.CompilerError{
				Line:    1,
				Column:  2,
				Message: "Error ova here.",
			},
		},
	}, resp.Status.ErrorDetails[0])
	assert.Equal(t, &public_vizierapipb.ErrorDetails{
		Error: &public_vizierapipb.ErrorDetails_CompilerError{
			CompilerError: &public_vizierapipb.CompilerError{
				Line:    20,
				Column:  19,
				Message: "Error ova there.",
			},
		},
	}, resp.Status.ErrorDetails[1])
}

func TestExecuteScript_ErrorInStatusResult(t *testing.T) {
	// Start NATS.
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)

	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	rf := &fakeResultForwarder{}

	badPlannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(failedPlannerResultFromStatus, badPlannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(badPlannerResultPB, nil)

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nil, nc, planner)
	require.NoError(t, err)

	srv := mock_public_vizierapipb.NewMockVizierService_ExecuteScriptServer(ctrl)
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)

	srv.EXPECT().Context().Return(ctx).AnyTimes()

	var resp *public_vizierapipb.ExecuteScriptResponse
	srv.EXPECT().
		Send(gomock.Any()).
		DoAndReturn(func(arg *public_vizierapipb.ExecuteScriptResponse) error {
			resp = arg
			return nil
		})

	err = s.ExecuteScript(&public_vizierapipb.ExecuteScriptRequest{
		QueryStr: badQuery,
	}, srv)

	require.NoError(t, err)
	assert.Equal(t, int32(3), resp.Status.Code)
	assert.Equal(t, "failure failure failure", resp.Status.Message)
	assert.Equal(t, 0, len(resp.Status.ErrorDetails))
}

func TestTransferResultChunk_AgentStreamComplete(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nil, nc, nil)
	require.NoError(t, err)

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}
	msg2 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_ExecutionAndTimingInfo{
			ExecutionAndTimingInfo: &carnotpb.TransferResultChunkRequest_QueryExecutionAndTimingInfo{
				ExecutionStats: &queryresultspb.QueryExecutionStats{
					Timing: &queryresultspb.QueryTimingInfo{
						ExecutionTimeNs:   5010,
						CompilationTimeNs: 350,
					},
					BytesProcessed:   4521,
					RecordsProcessed: 4,
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(msg2, nil)
	srv.EXPECT().Recv().Return(nil, io.EOF)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: true,
		Message: "",
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 2, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
	assert.Equal(t, msg2, rf.ReceivedAgentResults[1])
}

func TestTransferResultChunk_AgentClosedPrematurely(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nil, nc, nil)
	require.NoError(t, err)

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}
	sv.Eos = false

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	errorMsg := fmt.Sprintf(
		"Agent stream was unexpectedly closed for table %s of query %s before the results completed",
		"output_table_1", queryID.String(),
	)

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(nil, io.EOF)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: errorMsg,
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.NotNil(t, rf.ClientStreamError)
	assert.Equal(t, rf.ClientStreamError.Error(), errorMsg)
	assert.Equal(t, 1, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
}

func TestTransferResultChunk_AgentStreamFailed(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nil, nc, nil)
	require.NoError(t, err)

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(nil, fmt.Errorf("Agent error"))
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: "Agent error",
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.NotNil(t, rf.ClientStreamError)
	assert.Equal(t, rf.ClientStreamError.Error(), "Agent error")
	assert.Equal(t, 1, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
}

func TestTransferResultChunk_ClientStreamCancelled(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	require.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo))
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{
		ClientStreamClosed: true,
		ClientStreamError:  fmt.Errorf("An error"),
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname", "test")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nil, nc, nil)
	require.NoError(t, err)

	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.Must(uuid.NewV4())
	queryIDpb := utils.ProtoFromUUID(queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_QueryResult{
			QueryResult: &carnotpb.TransferResultChunkRequest_SinkResult{
				ResultContents: &carnotpb.TransferResultChunkRequest_SinkResult_RowBatch{
					RowBatch: sv,
				},
				Destination: &carnotpb.TransferResultChunkRequest_SinkResult_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: "An error",
	}).Return(nil)

	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	require.NoError(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.NotNil(t, rf.ClientStreamError)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))
}
