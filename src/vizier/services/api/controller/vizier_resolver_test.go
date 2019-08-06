package controller_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/errors"
	"github.com/graph-gophers/graphql-go/gqltesting"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	qrpb "pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/services/common/env"
	jwt "pixielabs.ai/pixielabs/src/services/common/proto"
	"pixielabs.ai/pixielabs/src/services/common/sessioncontext"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/api/apienv"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller"
	"pixielabs.ai/pixielabs/src/vizier/services/api/controller/schema"
	service "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
	mock_proto "pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb/mock"
)

// Impl is an implementation of the APIEnv interface
type FakeAPIEnv struct {
	*env.BaseEnv
	client *mock_proto.MockQueryBrokerServiceClient
}

// New creates a new api env.
func NewFakeAPIEnv(c *gomock.Controller) *FakeAPIEnv {
	return &FakeAPIEnv{
		client: mock_proto.NewMockQueryBrokerServiceClient(c),
	}
}

func (c *FakeAPIEnv) QueryBrokerClient() service.QueryBrokerServiceClient {
	return c.client
}

func CreateTestContext() context.Context {
	sCtx := sessioncontext.New()
	sCtx.Claims = &jwt.JWTClaims{}
	sCtx.Claims.Email = "test@test.com"
	sCtx.Claims.UserID = "abcdef"
	return sessioncontext.NewContext(context.Background(), sCtx)
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
	resp.Info = append(resp.Info, &service.AgentStatus{
		Info: &service.AgentInfo{
			AgentID: &uuidpb.UUID{
				Data: []byte(agentID),
			},
			HostInfo: &service.HostInfo{
				Hostname: "abcd",
			},
		},
		State:           service.AGENT_STATE_HEALTHY,
		LastHeartbeatNs: 121312321321312,
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
	upb, _ := utils.ProtoFromUUID(&u)

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
									ColumnType: typespb.INT64,
								},
							},
						},
						RowBatches: []*schemapb.RowBatchData{
							{
								Cols: []*schemapb.Column{},
							},
						},
					},
				},
			},
		},
	})
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
					}
				}`,
			ExpectedResult: `
				{
				   "ExecuteQuery":{
					  "id":"65294d6a-6ceb-48a7-96b0-9a1eb7d467cb",
					  "table":{
						 "data":"{\"relation\":{\"columns\":[{\"columnName\":\"scolE\",\"columnType\":\"BOOLEAN\"},{\"columnName\":\"scolI\",\"columnType\":\"INT64\"}]},\"rowBatches\":[{\"cols\":[]}]}",
						 "relation":{
							"colNames":[
							   "scolE",
							   "scolI"
							],
							"colTypes":[
							   "BOOLEAN",
							   "INT64"
							]
						 }
					  }
				   }
				}
				`,
		},
	})
}
