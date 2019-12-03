package controller_test

import (
	"context"
	"testing"
	"time"

	"pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/errors"
	"github.com/graph-gophers/graphql-go/gqltesting"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	qrpb "pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/env"
	jwtutils "pixielabs.ai/pixielabs/src/shared/services/utils"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	schemapb_types "pixielabs.ai/pixielabs/src/table_store/proto/types"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/api/apienv"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller/schema"
	service "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
	mock_proto "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb/mock"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// Impl is an implementation of the APIEnv interface
type FakeAPIEnv struct {
	*env.BaseEnv
	client *mock_proto.MockQueryBrokerServiceClient
}

// NewServer creates a new api env.
func NewFakeAPIEnv(c *gomock.Controller) *FakeAPIEnv {
	return &FakeAPIEnv{
		client: mock_proto.NewMockQueryBrokerServiceClient(c),
	}
}

func (c *FakeAPIEnv) QueryBrokerClient() service.QueryBrokerServiceClient {
	return c.client
}

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = jwtutils.GenerateJWTForUser("test", "6test", "test@test.com", time.Now())

	return authcontext.NewContext(context.Background(), sCtx)
}

func LoadSchema(env apienv.APIEnv) *graphql.Schema {
	schemaData := schema.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &controller.QueryResolver{Env: env}, opts...)
	return gqlSchema
}

func TestVizierResolverWorksOnGoodRequest(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	env := NewFakeAPIEnv(ctrl)
	ctx := CreateTestContext()

	req := service.AgentInfoRequest{}
	resp := service.AgentInfoResponse{}

	agentID := "1b975238-93f7-4c24-a442-a0abde89b8c2"
	resp.Info = append(resp.Info, &service.AgentMetadata{
		Agent: &agentpb.Agent{
			Info: &agentpb.AgentInfo{
				AgentID: &uuidpb.UUID{
					Data: []byte(agentID),
				},
				HostInfo: &agentpb.HostInfo{
					Hostname: "abcd",
				},
			},
		},
		Status: &agentpb.AgentStatus{
			State:                agentpb.AGENT_STATE_HEALTHY,
			NSSinceLastHeartbeat: 121312321321312,
		},
	})

	env.client.EXPECT().
		GetAgentInfo(gomock.Any(), gomock.Eq(&req)).
		Return(&resp, nil).
		AnyTimes()

	gqlSchema := LoadSchema(env)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
					query {
					  vizier {
						agents{
						  info {
							id
							hostInfo{
							  hostname
							}
						  }
						  state
						  lastHeartbeatMs
						}
					  }
					}
					`,
			ExpectedResult: `
				{
					"vizier": {
					  "agents": [
						{
						  "info": {
							"id": "1b975238-93f7-4c24-a442-a0abde89b8c2",
							"hostInfo": {
							  "hostname": "abcd"
							}
						  },
						  "state": "AGENT_STATE_HEALTHY",
						  "lastHeartbeatMs": 121312321
						}
					  ]
					}
				}`,
		},
	})
}

func TestVizierResolverProducesErrorOnVizierError(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	env := NewFakeAPIEnv(ctrl)
	ctx := CreateTestContext()

	req := service.AgentInfoRequest{}

	errStatus := status.Error(codes.DeadlineExceeded, "Failed")
	env.client.EXPECT().
		GetAgentInfo(gomock.Any(), gomock.Eq(&req)).
		Return(nil, errStatus).
		AnyTimes()

	gqlSchema := LoadSchema(env)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
					query {
					  vizier {
						agents {
						  info {
							id
						  }
						}
					  }
					}
					`,
			ExpectedResult: `{"vizier": {"agents": null}}`,
			ExpectedErrors: []*errors.QueryError{
				{
					Message:       errStatus.Error(),
					Path:          []interface{}{"vizier", "agents"},
					ResolverError: errStatus,
				},
			},
		},
	})
}

func TestVizierExecuteQuery(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	env := NewFakeAPIEnv(ctrl)
	ctx := CreateTestContext()

	resp := service.VizierQueryResponse{}
	u, _ := uuid.FromString("65294d6a-6ceb-48a7-96b0-9a1eb7d467cb")
	upb := utils.ProtoFromUUID(&u)

	resp.Responses = append(resp.Responses, &service.VizierQueryResponse_ResponseByAgent{
		Response: &service.AgentQueryResponse{
			QueryID: upb,
			QueryResult: &qrpb.QueryResult{

				Tables: []*schemapb.Table{
					{
						Relation: &schemapb.Relation{
							Columns: []*schemapb.Relation_ColumnInfo{
								{
									ColumnName: "scolE",
									ColumnType: typespb.BOOLEAN,
								},
								{
									ColumnName: "scolI",
									ColumnType: typespb.STRING,
								},
							},
						},
						RowBatches: []*schemapb.RowBatchData{
							{
								Cols: []*schemapb.Column{
									{
										ColData: &schemapb.Column_StringData{
											StringData: &schemapb.StringColumn{
												Data: []schemapb_types.StringData{
													[]byte("hello"),
													[]byte("test"),
												},
											},
										},
									},
								},
							},
						},
					},
					{
						Relation: &schemapb.Relation{
							Columns: []*schemapb.Relation_ColumnInfo{
								{
									ColumnName: "aCol",
									ColumnType: typespb.STRING,
								},
							},
						},
						RowBatches: []*schemapb.RowBatchData{
							{
								Cols: []*schemapb.Column{
									{
										ColData: &schemapb.Column_StringData{
											StringData: &schemapb.StringColumn{
												Data: []schemapb_types.StringData{
													[]byte("abc"),
													[]byte("def"),
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
	})
	resp.QueryID = upb
	resp.Status = &statuspb.Status{}
	env.client.EXPECT().
		ExecuteQuery(gomock.Any(), gomock.Any()).
		Return(&resp, nil).
		AnyTimes()

	gqlSchema := LoadSchema(env)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					ExecuteQuery(queryStr: "the_query") {
						id
						table {
							relation {
								colNames
								colTypes
							}
							data
						}
						error {
							compilerError{
								msg
								lineColErrors {
									line
									col
									msg
								}
							}
						}
					}
				}`,
			ExpectedResult: `
				{
				   "ExecuteQuery":{
					  "id":"65294d6a-6ceb-48a7-96b0-9a1eb7d467cb",
					  "table": [
						  {
							 "data":"{\"relation\":{\"columns\":[{\"columnName\":\"scolE\",\"columnType\":\"BOOLEAN\"},{\"columnName\":\"scolI\",\"columnType\":\"STRING\"}]},\"rowBatches\":[{\"cols\":[{\"stringData\":{\"data\":[\"hello\",\"test\"]}}]}]}",
							 "relation":{
								"colNames":[
								   "scolE",
								   "scolI"
								],
								"colTypes":[
								   "BOOLEAN",
								   "STRING"
								]
							 }
						  },
						  {
							 "data":"{\"relation\":{\"columns\":[{\"columnName\":\"aCol\",\"columnType\":\"STRING\"}]},\"rowBatches\":[{\"cols\":[{\"stringData\":{\"data\":[\"abc\",\"def\"]}}]}]}",
							 "relation":{
								"colNames":[
								   "aCol"
								],
								"colTypes":[
								   "STRING"
								]
							 }
						  }						  
					  ],
					  "error":{
							"compilerError": null 
						}
				  }
				}
				`,
		},
	})
}

// Makes sure that the api server can handle compiler errors.
func TestVizierExecuteQueryFailedCompilation(t *testing.T) {
	// TODO add a test that fails without line column errors.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	env := NewFakeAPIEnv(ctrl)
	ctx := CreateTestContext()

	resp := service.VizierQueryResponse{}
	u, _ := uuid.FromString("65294d6a-6ceb-48a7-96b0-9a1eb7d467cb")
	upb := utils.ProtoFromUUID(&u)
	statusContext, err := types.MarshalAny(&compilerpb.CompilerErrorGroup{
		Errors: []*compilerpb.CompilerError{
			&compilerpb.CompilerError{
				Error: &compilerpb.CompilerError_LineColError{
					&compilerpb.LineColError{
						Line:    10,
						Column:  54,
						Message: "Missing table",
					},
				},
			},
		},
	})
	resp.QueryID = upb

	if !assert.Nil(t, err) {
		t.FailNow()
	}

	resp.Status = &statuspb.Status{
		ErrCode: statuspb.INVALID_ARGUMENT,
		Context: statusContext}

	env.client.EXPECT().
		ExecuteQuery(gomock.Any(), gomock.Any()).
		Return(&resp, nil).
		AnyTimes()

	gqlSchema := LoadSchema(env)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					ExecuteQuery(queryStr: "the_query") {
					  id
					  table {
							relation {
								colNames
								colTypes
							}
							data
					  }
					  error{
					    compilerError {
								msg
								lineColErrors {
									line
									col
									msg
								}
							}
					  }
					}
				}`,
			ExpectedResult: `
			{
				"ExecuteQuery":{
					"id":"65294d6a-6ceb-48a7-96b0-9a1eb7d467cb",
					"table": null,
					"error": {
						"compilerError": {
							"msg": "", 
							"lineColErrors": [
								{ 
									"line": 10,
									"col": 54,
									"msg": "Missing table"
								}
							]
						}
					}
				}
			}
			`,
		},
	})
}

// Makes sure that the api server can handle compiler errors that don't have line, col errors.
func TestVizierExecuteQueryFailedCompilationNoContextObj(t *testing.T) {
	// TODO add a test that fails without line column errors.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	env := NewFakeAPIEnv(ctrl)
	ctx := CreateTestContext()

	resp := service.VizierQueryResponse{}
	u, _ := uuid.FromString("65294d6a-6ceb-48a7-96b0-9a1eb7d467cb")
	upb := utils.ProtoFromUUID(&u)
	resp.QueryID = upb

	resp.Status = &statuspb.Status{
		ErrCode: statuspb.INVALID_ARGUMENT,
		Msg:     "Missing UDF"}

	env.client.EXPECT().
		ExecuteQuery(gomock.Any(), gomock.Any()).
		Return(&resp, nil).
		AnyTimes()

	gqlSchema := LoadSchema(env)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					ExecuteQuery(queryStr: "the_query") {
					  id
					  table {
						relation {
						  colNames
						  colTypes
						}
						data
					  }
						error {
							compilerError{
								msg
								lineColErrors {
									line
									col
									msg
								}
							}
						}
					}
				}`,
			ExpectedResult: `
			{
				"ExecuteQuery":{
					"id":"65294d6a-6ceb-48a7-96b0-9a1eb7d467cb",
					"table": null,
					"error": {
						"compilerError": {
							"msg": "Missing UDF", 
							"lineColErrors": null
						}
					}
				}
			}
			`,
		},
	})
}
