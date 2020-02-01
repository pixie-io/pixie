package controllers

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	compilerpb "pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/compiler/distributedpb"
	plannerpb "pixielabs.ai/pixielabs/src/carnot/compiler/plannerpb"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	mock_metadatapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb/mock"
	mock_controllers "pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
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
schema: {
  relation_map: {
    key: "perf_and_http"
    value: {
      columns: {
        column_name: "_time"
      }
    }
  }
}
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
schema: {
  relation_map: {
    key: "perf_and_http"
    value: {
      columns: {
        column_name: "_time"
      }
    }
  }
}
distributed_state: {
  carnot_info: {
    query_broker_address: "21285cdd-1de9-4ab1-ae6a-0ba08c8c676c"
    has_data_store: true
		processes_data: true
		asid: 123
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
		[type.googleapis.com/pl.carnot.compiler.compilerpb.CompilerErrorGroup] {
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

func TestServerExecuteQueryTimeout(t *testing.T) {
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

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	getAgentsPB := new(metadatapb.AgentInfoResponse)
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

	getSchemaPB := new(metadatapb.SchemaResponse)
	if err := proto.UnmarshalText(getSchemaResponse, getSchemaPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetSchemas(context.Background(), &metadatapb.SchemaRequest{}).
		Return(getSchemaPB, nil)

		// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	plannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(expectedPlannerResult, plannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	queryRequest := &plannerpb.QueryRequest{
		QueryStr: testQuery,
	}
	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, queryRequest).
		Return(plannerResultPB, nil)

	s, err := NewServer(env, mds, nc)
	queryID := uuid.NewV4()
	queryResult, err := s.ExecuteQueryWithPlanner(context.Background(), queryRequest, queryID, planner, &planpb.PlanOptions{Analyze: true})
	if err != nil {
		t.Fatal("Failed to return results from ExecuteQuery.")
	}
	assert.Nil(t, queryResult.QueryResult)
}

func TestExecuteQueryInvalidFlags(t *testing.T) {
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

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	createExecutorMock := func(_ *nats.Conn, _ uuid.UUID, agentList *[]uuid.UUID) Executor {
		mc := mock_controllers.NewMockExecutor(ctrl)
		return mc
	}

	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := newServer(env, mds, nc, createExecutorMock)

	queryRequest := &plannerpb.QueryRequest{
		QueryStr: invalidFlagQuery,
	}

	result, err := s.ExecuteQuery(context.Background(), queryRequest)
	assert.Nil(t, err)
	assert.Equal(t, "thisisnotaflag is not a valid flag", result.Status.Msg)
}

func TestReceiveAgentQueryResult(t *testing.T) {
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

	m := mock_controllers.NewMockExecutor(ctrl)
	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	req := new(querybrokerpb.AgentQueryResultRequest)
	if err := proto.UnmarshalText(kelvinResponse, req); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	m.
		EXPECT().
		AddResult(req)

	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := NewServer(env, mds, nc)
	if err != nil {
		t.Fatal("Creating server failed.")
	}

	queryUUID, err := uuid.FromString(queryIDStr)
	if err != nil {
		t.Fatal("Could not parse UUID.")
	}

	// Add mock executor as an executor.
	s.executors[queryUUID] = m

	expectedResp := &querybrokerpb.AgentQueryResultResponse{}

	resp, err := s.ReceiveAgentQueryResult(context.Background(), req)
	assert.Equal(t, expectedResp, resp)
}

func TestGetAgentInfo(t *testing.T) {
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

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)
	getAgentsPB := &metadatapb.AgentInfoResponse{}
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

		// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := NewServer(env, mds, nc)

	if err != nil {
		t.Fatal("Creating server failed.")
	}

	getAgentsRespPB := &querybrokerpb.AgentInfoResponse{}
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsRespPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	resp, err := s.GetAgentInfo(context.Background(), &querybrokerpb.AgentInfoRequest{})
	assert.Equal(t, getAgentsRespPB, resp)
}

func TestGetMultipleAgentInfo(t *testing.T) {
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

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)
	getAgentsPB := &metadatapb.AgentInfoResponse{}
	if err := proto.UnmarshalText(getMultipleAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

		// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	s, err := NewServer(env, mds, nc)

	if err != nil {
		t.Fatal("Creating server failed.")
	}

	getAgentsRespPB := &querybrokerpb.AgentInfoResponse{}
	if err := proto.UnmarshalText(getMultipleAgentsResponse, getAgentsRespPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	resp, err := s.GetAgentInfo(context.Background(), &querybrokerpb.AgentInfoRequest{})
	assert.Equal(t, getAgentsRespPB, resp)
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

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	getAgentsPB := new(metadatapb.AgentInfoResponse)
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

	getSchemaPB := new(metadatapb.SchemaResponse)
	if err := proto.UnmarshalText(getSchemaResponse, getSchemaPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetSchemas(context.Background(), &metadatapb.SchemaRequest{}).
		Return(getSchemaPB, nil)

	createExecutorMock := func(_ *nats.Conn, _ uuid.UUID, agentList *[]uuid.UUID) Executor {
		mc := mock_controllers.NewMockExecutor(ctrl)
		return mc
	}

	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	badPlannerResultPB := new(distributedpb.LogicalPlannerResult)
	if err := proto.UnmarshalText(failedPlannerResult, badPlannerResultPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	compilerErrorGroupPB := new(compilerpb.CompilerErrorGroup)
	if err := proto.UnmarshalText(compilerErrorGroupTxt, compilerErrorGroupPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	queryRequest := &plannerpb.QueryRequest{
		QueryStr: badQuery,
	}
	planner := mock_controllers.NewMockPlanner(ctrl)
	planner.EXPECT().
		Plan(plannerStatePB, queryRequest).
		Return(badPlannerResultPB, nil)

	s, err := newServer(env, mds, nc, createExecutorMock)
	queryID := uuid.NewV4()
	result, err := s.ExecuteQueryWithPlanner(context.Background(), queryRequest,
		queryID, planner, &planpb.PlanOptions{Analyze: true})

	if !assert.Nil(t, err) {
		t.Fatal("Cannot execute query.")
	}

	agentRespPB := new(querybrokerpb.VizierQueryResponse)
	if err := proto.UnmarshalText(failedPlannerResult, agentRespPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}
	assert.Equal(t, agentRespPB.Status, result.Status)
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

	mds := mock_metadatapb.NewMockMetadataServiceClient(ctrl)

	getAgentsPB := new(metadatapb.AgentInfoResponse)
	if err := proto.UnmarshalText(getAgentsResponse, getAgentsPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetAgentInfo(context.Background(), &metadatapb.AgentInfoRequest{}).
		Return(getAgentsPB, nil)

	getSchemaPB := new(metadatapb.SchemaResponse)
	if err := proto.UnmarshalText(getSchemaResponse, getSchemaPB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	mds.
		EXPECT().
		GetSchemas(context.Background(), &metadatapb.SchemaRequest{}).
		Return(getSchemaPB, nil)

	createExecutorMock := func(_ *nats.Conn, _ uuid.UUID, agentList *[]uuid.UUID) Executor {
		mc := mock_controllers.NewMockExecutor(ctrl)
		return mc
	}

	// Set up server.
	env, err := querybrokerenv.New()
	if err != nil {
		t.Fatal("Failed to create api environment.")
	}

	// The state passes in multiple agents.
	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
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

	s, err := newServer(env, mds, nc, createExecutorMock)

	queryID := uuid.NewV4()
	result, err := s.ExecuteQueryWithPlanner(context.Background(), queryRequest, queryID, planner, &planpb.PlanOptions{Analyze: true})

	if !assert.Nil(t, err) {
		t.Fatal("Error while executing query.")
	}
	assert.Equal(t, result.Status, badPlannerResultPB.Status)
}
