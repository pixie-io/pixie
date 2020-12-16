package controllers_test

import (
	"context"
	"fmt"
	"io"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	carnotpb "pixielabs.ai/pixielabs/src/carnotpb"
	mock_carnotpb "pixielabs.ai/pixielabs/src/carnotpb/mock"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/tracker"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
	mock_vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb/mock"
)

const getAgentsResponse = `
info {
	agent {
		info {
			agent_id {
				data: "21285cdd1de94ab1ae6a0ba08c8c676c"
			}
			host_info {
				hostname: "test_host"
			}
			capabilities {
				collects_data: true
			}
		}
		asid: 123
	}
	status {
		state: 1
	}
}
`

const getAgentTableMetadataResponse = `
metadata_by_agent {
	agent_id {
		data: "21285cdd1de94ab1ae6a0ba08c8c676c"
	}
	schema {
		relation_map {
			key: "perf_and_http"
			value {
				columns {
					column_name: "_time"
				}
			}
		}
	}
	data_info {
		metadata_info {
			metadata_fields: CONTAINER_ID
			metadata_fields: SERVICE_NAME
			xxhash64_bloom_filter {
				num_hashes: 2
				data: "1234"
			}
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
		data: "21285cdd1de94ab1ae6a0ba08c8c676c"
	}
}
`

const getSchemaResponse = `
schema {
	relation_map {
		key: "perf_and_http"
		value {
			columns {
				column_name: "_time"
			}
		}
	}
}
`

const multipleAgentDistributedState = `
distributed_state: {
	carnot_info: {
    	query_broker_address: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
    	has_data_store: true
		processes_data: true
		asid: 123
  	}
  	carnot_info: {
    	query_broker_address: "31285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
    	has_data_store: true
		processes_data: true
		asid: 456
  	}
}
plan_options: {
	explain: false
	analyze: false
}
result_address: "qb_address"
result_ssl_targetname: "qb_hostname"
`

const singleAgentDistributedState = `
distributed_state: {
	carnot_info: {
		query_broker_address: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
		agent_id {
			data:  "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
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
			data: "21285cdd1de94ab1ae6a0ba08c8c676c"
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

const getMultipleAgentsResponse = `
info {
	agent {
		info {
			agent_id {
				data: "21285cdd1de94ab1ae6a0ba08c8c676c"
			}
			host_info {
				hostname: "test_host"
			}
			capabilities {
				collects_data: true
			}
		}
		asid: 123
	}
	status {
		state: 1
	}
}
info {
	agent {
		info {
			agent_id {
				data: "31285cdd1de94ab1ae6a0ba08c8c676c"
			}
			host_info {
				hostname: "another_host"
			}
			capabilities {
				collects_data: true
			}
		}
		asid: 456
	}
	status {
		state: 1
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
	     	}
	     	name: "test"
    	}
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

const invalidFlagQuery = `
#px:set thisisnotaflag=true
df = dataframe(table='perf_and_http', select=['_time'])
display(df, 'out')
`

const testLogicalPlan = `
dag: {
	nodes: {
		id: 1
	}
}
nodes: {
	id: 1
	dag: {
		nodes: {
		}
	}
	nodes: {
		op: {
			op_type: MEMORY_SOURCE_OPERATOR
			mem_source_op: {
			}
		}
	}
}
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
		[type.googleapis.com/pl.carnot.planner.compilerpb.CompilerErrorGroup] {
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

const compilerErrorGroupTxt = `
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
	resultCh chan *vizierpb.ExecuteScriptResponse,
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
		return fmt.Errorf("Client stream for query has been closed")
	}
	f.ReceivedAgentResults = append(f.ReceivedAgentResults, msg)
	return nil
}

// CloseQueryResultStream closes both the client and agent side streams if either side terminates.
func (f *fakeResultForwarder) OptionallyCancelClientStream(queryID uuid.UUID) {
	f.ClientStreamClosed = true
}

func TestCheckHealth_Success(t *testing.T) {
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

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	queryID := uuid.NewV4()
	fakeResult := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: &vizierpb.RowBatchData{
					TableID: "health_check_unused",
					Cols: []*vizierpb.Column{
						&vizierpb.Column{
							ColData: &vizierpb.Column_StringData{
								StringData: &vizierpb.StringColumn{
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
		ClientResultsToSend: []*vizierpb.ExecuteScriptResponse{
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

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nc, planner)
	err = s.CheckHealth(context.Background())
	// Should pass.
	assert.Nil(t, err)
}

func TestCheckHealth_CompilationError(t *testing.T) {
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

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	rf := &fakeResultForwarder{
		ClientResultsToSend: []*vizierpb.ExecuteScriptResponse{},
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, gomock.Any()).
		Return(nil, fmt.Errorf("some compiler error"))

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nc, planner)
	err = s.CheckHealth(context.Background())
	// Should not pass.
	assert.NotNil(t, err)
}

func TestHealthCheck_ExecutionError(t *testing.T) {
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

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	// Receiving no results should cause an error.
	rf := &fakeResultForwarder{
		ClientResultsToSend: []*vizierpb.ExecuteScriptResponse{},
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

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nc, planner)
	err = s.CheckHealth(context.Background())
	// Should not pass.
	assert.NotNil(t, err)
}

func TestExecuteScript_Mutation(t *testing.T) {
	// TODO(zasgar, philkuz, nserrino): Add unit tests for mutation execution.
}

func TestExecuteScript_MutationError(t *testing.T) {
	// TODO(zasgar, philkuz, nserrino): Add unit tests for mutation execution.
}

func TestExecuteScript_Success(t *testing.T) {
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

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)

	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	queryID := uuid.NewV4()

	fakeBatch := new(vizierpb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, fakeBatch); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	fakeResult1 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: &vizierpb.QueryData{
				Batch: fakeBatch,
			},
		},
	}
	fakeResult2 := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
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

	rf := &fakeResultForwarder{
		ClientResultsToSend: []*vizierpb.ExecuteScriptResponse{
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

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nc, planner)

	srv := mock_vizierpb.NewMockVizierService_ExecuteScriptServer(ctrl)
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)

	srv.EXPECT().Context().Return(ctx).AnyTimes()

	var resps []*vizierpb.ExecuteScriptResponse
	srv.EXPECT().
		Send(gomock.Any()).
		DoAndReturn(func(arg *vizierpb.ExecuteScriptResponse) error {
			resps = append(resps, arg)
			return nil
		}).
		AnyTimes()

	err = s.ExecuteScript(&vizierpb.ExecuteScriptRequest{
		QueryStr: testQuery,
	}, srv)

	assert.Nil(t, err)
	assert.Equal(t, 4, len(resps))

	assert.Equal(t, 2, len(rf.TableIDMap))
	assert.NotEqual(t, 0, len(rf.TableIDMap["agent1_table"]))
	assert.NotEqual(t, 0, len(rf.TableIDMap["agent2_table"]))

	// Make sure the relation is sent with the metadata.
	actualSchemaResults := make(map[string]*vizierpb.ExecuteScriptResponse)
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
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

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
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
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

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nc, planner)

	srv := mock_vizierpb.NewMockVizierService_ExecuteScriptServer(ctrl)
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)

	srv.EXPECT().Context().Return(ctx).AnyTimes()

	var resp *vizierpb.ExecuteScriptResponse
	srv.EXPECT().
		Send(gomock.Any()).
		DoAndReturn(func(arg *vizierpb.ExecuteScriptResponse) error {
			resp = arg
			return nil
		})

	err = s.ExecuteScript(&vizierpb.ExecuteScriptRequest{
		QueryStr: badQuery,
	}, srv)

	assert.Nil(t, err)
	assert.Equal(t, int32(3), resp.Status.Code)
	assert.Equal(t, "", resp.Status.Message)
	assert.Equal(t, 2, len(resp.Status.ErrorDetails))
	assert.Equal(t, &vizierpb.ErrorDetails{
		Error: &vizierpb.ErrorDetails_CompilerError{
			CompilerError: &vizierpb.CompilerError{
				Line:    1,
				Column:  2,
				Message: "Error ova here.",
			},
		},
	}, resp.Status.ErrorDetails[0])
	assert.Equal(t, &vizierpb.ErrorDetails{
		Error: &vizierpb.ErrorDetails_CompilerError{
			CompilerError: &vizierpb.CompilerError{
				Line:    20,
				Column:  19,
				Message: "Error ova there.",
			},
		},
	}, resp.Status.ErrorDetails[1])
}

func TestExecuteScript_ErrorInStatusResult(t *testing.T) {
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

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)

	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
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

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, rf, nil, nc, planner)

	srv := mock_vizierpb.NewMockVizierService_ExecuteScriptServer(ctrl)
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)

	srv.EXPECT().Context().Return(ctx).AnyTimes()

	var resp *vizierpb.ExecuteScriptResponse
	srv.EXPECT().
		Send(gomock.Any()).
		DoAndReturn(func(arg *vizierpb.ExecuteScriptResponse) error {
			resp = arg
			return nil
		})

	err = s.ExecuteScript(&vizierpb.ExecuteScriptRequest{
		QueryStr: badQuery,
	}, srv)

	assert.Nil(t, err)
	assert.Equal(t, int32(3), resp.Status.Code)
	assert.Equal(t, "failure failure failure", resp.Status.Message)
	assert.Equal(t, 0, len(resp.Status.ErrorDetails))
}

func TestTransferResultChunk_AgentStreamComplete(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	if !assert.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo)) {
		t.FailNow()
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nc, nil)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(queryID)

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
	if !assert.Nil(t, err) {
		t.Fatal("Error while transferring result chunk", err)
	}

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 2, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
	assert.Equal(t, msg2, rf.ReceivedAgentResults[1])
}

func TestTransferResultChunk_MultipleTransferResultChunkStreams(t *testing.T) {
	// TODO(nserrino): Write a test for the case where two streams send to the same forwarder.
	// Or modify the _Success test to use multiple streams.
}

func TestTransferResultChunk_AgentClosedPrematurely(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	if !assert.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo)) {
		t.FailNow()
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nc, nil)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}
	sv.Eos = false

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(queryID)

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
	srv.EXPECT().Recv().Return(nil, io.EOF)
	srv.EXPECT().SendAndClose(&carnotpb.TransferResultChunkResponse{
		Success: false,
		Message: fmt.Sprintf(
			"agent stream was unxpectedly closed for table %s of query %s before the results completed",
			"output_table_1", queryID.String(),
		),
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	if !assert.Nil(t, err) {
		t.Fatal("Error while transferring result chunk", err)
	}

	assert.True(t, rf.ClientStreamClosed)
	assert.Equal(t, 1, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
}

func TestTransferResultChunk_AgentStreamFailed(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	if !assert.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo)) {
		t.FailNow()
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nc, nil)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(queryID)

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
		Message: "Error reading TransferResultChunk stream: Agent error",
	}).Return(nil)

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	assert.Nil(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.Equal(t, 1, len(rf.ReceivedAgentResults))
	assert.Equal(t, msg1, rf.ReceivedAgentResults[0])
}

func TestTransferResultChunk_ClientStreamCancelled(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	nc, err := nats.Connect(testingutils.GetNATSURL(port))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	if !assert.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo)) {
		t.FailNow()
	}
	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)
	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	rf := fakeResultForwarder{
		ClientStreamClosed: true,
	}

	// Set up server.
	env, err := querybrokerenv.New("qb_address", "qb_hostname")
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithForwarderAndPlanner(env, &at, &rf, nil, nc, nil)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(queryID)

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
		Message: "Client stream for query has been closed",
	}).Return(nil)

	assert.Equal(t, 0, len(rf.ReceivedAgentResults))

	err = s.TransferResultChunk(srv)
	assert.Nil(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedAgentResults))
}
