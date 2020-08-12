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
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	mock_carnotpb "pixielabs.ai/pixielabs/src/carnotpb/mock"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/tracker"
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
	analyze: true
}
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
						op_type: MEMORY_SINK_OPERATOR
						mem_sink_op: {
							name: "out"
							column_types: TIME64NS
							column_types: INT64
							column_types: UINT128
							column_names: "time_"
							column_names: "cpu_cycles"
							column_names: "upid"
						}
					}
				}
			}
		}
	}
	qb_address_to_dag_id: {
		key: "agent1"
		value: 0
	}
	dag: {
		nodes: {
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

// TestPlannerErrorResult makes sure that compiler error handling is done well.
func TestPlannerErrorResult(t *testing.T) {
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

	createExecutorMock := func(_ *nats.Conn, _ uuid.UUID) controllers.Executor {
		mc := mock_controllers.NewMockExecutor(ctrl)
		return mc
	}

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	agentsInfo := tracker.NewTestAgentsInfo(plannerStatePB.DistributedState)

	at := fakeAgentsTracker{
		agentsInfo: agentsInfo,
	}
	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	badPlannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(failedPlannerResult, badPlannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf failedPlannerResult", failedPlannerResult)
	}

	compilerErrorGroupPB := new(compilerpb.CompilerErrorGroup)
	if err := proto.UnmarshalText(compilerErrorGroupTxt, compilerErrorGroupPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	queryRequest := &plannerpb.QueryRequest{
		QueryStr: badQuery,
	}
	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, queryRequest).
		Return(badPlannerResultPB, nil)

	s, err := controllers.NewServerWithExecutor(env, &at, nil, nil, nc, createExecutorMock)
	queryID := uuid.NewV4()
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)
	_, status, err := s.ExecuteQueryWithPlanner(ctx, queryRequest,
		queryID, planner, &planpb.PlanOptions{Analyze: true})

	if !assert.Nil(t, err) {
		t.Fatal("Cannot execute query.")
	}

	agentRespPB := new(querybrokerpb.VizierQueryResponse)
	if err := proto.UnmarshalText(failedPlannerResult, agentRespPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, agentRespPB.Status, status)
}

func TestErrorInStatusResult(t *testing.T) {
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
	createExecutorMock := func(_ *nats.Conn, _ uuid.UUID) controllers.Executor {
		mc := mock_controllers.NewMockExecutor(ctrl)
		return mc
	}

	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	if !assert.Equal(t, 1, len(plannerStatePB.DistributedState.CarnotInfo)) {
		t.FailNow()
	}

	badPlannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(failedPlannerResultFromStatus, badPlannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	planner := mock_controllers.NewMockPlanner(ctrl)
	queryRequest := &plannerpb.QueryRequest{
		QueryStr: badQuery,
	}

	planner.EXPECT().
		Plan(plannerStatePB, queryRequest).
		Return(badPlannerResultPB, nil)

	s, err := controllers.NewServerWithExecutor(env, &at, nil, nil, nc, createExecutorMock)

	queryID := uuid.NewV4()
	auth := authcontext.New()
	ctx := authcontext.NewContext(context.Background(), auth)
	_, status, err := s.ExecuteQueryWithPlanner(ctx, queryRequest, queryID, planner, &planpb.PlanOptions{Analyze: true})

	if !assert.Nil(t, err) {
		t.Fatal("Error while executing query.")
	}
	assert.Equal(t, status, badPlannerResultPB.Status)
}

type fakeResultForwarder struct {
	ClientStreamClosed bool
	ReceivedResults    []*carnotpb.TransferResultChunkRequest
}

// ForwardQueryResult forwards the agent result to the client result stream.
func (f *fakeResultForwarder) ForwardQueryResult(msg *carnotpb.TransferResultChunkRequest) error {
	if f.ClientStreamClosed {
		return fmt.Errorf("Client stream for query has been closed")
	}
	f.ReceivedResults = append(f.ReceivedResults, msg)
	return nil
}

// CloseQueryResultStream closes both the client and agent side streams if either side terminates.
func (f *fakeResultForwarder) OptionallyCancelClientStream(queryID uuid.UUID) error {
	f.ClientStreamClosed = true
	return nil
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
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithExecutor(env, &at, &rf, nil, nc, controllers.NewQueryExecutor)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(&queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: sv,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
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
	assert.Equal(t, 0, len(rf.ReceivedResults))

	err = s.TransferResultChunk(srv)
	if !assert.Nil(t, err) {
		t.Fatal("Error while transferring result chunk", err)
	}

	assert.True(t, rf.ClientStreamClosed)
	assert.Equal(t, 2, len(rf.ReceivedResults))
	assert.Equal(t, msg1, rf.ReceivedResults[0])
	assert.Equal(t, msg2, rf.ReceivedResults[1])
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
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithExecutor(env, &at, &rf, nil, nc, controllers.NewQueryExecutor)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(&queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: sv,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
					TableName: "output_table_1",
				},
			},
		},
	}

	srv.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	srv.EXPECT().Recv().Return(msg1, nil)
	srv.EXPECT().Recv().Return(nil, fmt.Errorf("Agent error"))

	assert.False(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedResults))

	err = s.TransferResultChunk(srv)
	assert.NotNil(t, err)

	res, ok := status.FromError(err)
	require.True(t, ok)
	assert.Equal(t, codes.Internal, res.Code())

	assert.True(t, rf.ClientStreamClosed)
	assert.Equal(t, 1, len(rf.ReceivedResults))
	assert.Equal(t, msg1, rf.ReceivedResults[0])
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
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := controllers.NewServerWithExecutor(env, &at, &rf, nil, nc, controllers.NewQueryExecutor)
	srv := mock_carnotpb.NewMockResultSinkService_TransferResultChunkServer(ctrl)

	sv := new(schemapb.RowBatchData)
	if err := proto.UnmarshalText(rowBatchPb, sv); err != nil {
		t.Fatalf("Cannot unmarshal proto %v", err)
	}

	queryID := uuid.NewV4()
	queryIDpb := pbutils.ProtoFromUUID(&queryID)

	msg1 := &carnotpb.TransferResultChunkRequest{
		Address: "foo",
		QueryID: queryIDpb,
		Result: &carnotpb.TransferResultChunkRequest_RowBatchResult{
			RowBatchResult: &carnotpb.TransferResultChunkRequest_ResultRowBatch{
				RowBatch: sv,
				Destination: &carnotpb.TransferResultChunkRequest_ResultRowBatch_TableName{
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

	assert.Equal(t, 0, len(rf.ReceivedResults))

	err = s.TransferResultChunk(srv)
	assert.Nil(t, err)

	assert.True(t, rf.ClientStreamClosed)
	assert.Equal(t, 0, len(rf.ReceivedResults))
}
