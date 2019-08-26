package controller_test

import (
	"context"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/gqltesting"
	jwt "pixielabs.ai/pixielabs/src/shared/services/proto"

	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/schema"
	"pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	cloudpb "pixielabs.ai/pixielabs/src/cloud/cloudpb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
)

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = &jwt.JWTClaims{}
	sCtx.Claims.Email = "test@test.com"
	sCtx.Claims.UserID = "abcdef"
	sCtx.Claims.OrgID = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	return authcontext.NewContext(context.Background(), sCtx)
}

func LoadSchema(env apienv.APIEnv) *graphql.Schema {
	schemaData := schema.MustLoadSchema()
	opts := []graphql.SchemaOpt{graphql.UseFieldResolvers(), graphql.MaxParallelism(20)}
	gqlSchema := graphql.MustParseSchema(schemaData, &controller.QueryResolver{Env: env}, opts...)
	return gqlSchema
}

func TestCreateCluster(t *testing.T) {
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	clusterID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	apiEnv, _, _, _, mockVzMgr, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	expectedVZMgrReq := &vzmgrpb.CreateVizierClusterRequest{
		OrgID: &uuidpb.UUID{Data: []byte(orgID)},
	}
	mockVzMgr.EXPECT().CreateVizierCluster(gomock.Any(), expectedVZMgrReq).
		Return(&uuidpb.UUID{Data: []byte(clusterID)}, nil)

	gqlSchema := LoadSchema(apiEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					CreateCluster {
						id
					}
				}
			`,
			ExpectedResult: `
				{
					"CreateCluster": {
						"id":"7ba7b810-9dad-11d1-80b4-00c04fd430c8"
					}
				}
			`,
		},
	})
}

func TestClusterInfo(t *testing.T) {
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	clusterID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	apiEnv, _, _, _, mockVzMgr, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzrIDs := make([]*uuidpb.UUID, 1)
	vzrIDs[0] = &uuidpb.UUID{Data: []byte(clusterID)}
	vzrResp := &vzmgrpb.GetViziersByOrgResponse{
		VizierIDs: vzrIDs,
	}
	mockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), &uuidpb.UUID{Data: []byte(orgID)}).
		Return(vzrResp, nil)

	vzrInfoResp := &cloudpb.VizierInfo{
		VizierID:        &uuidpb.UUID{Data: []byte(clusterID)},
		Status:          1,
		LastHeartbeatNs: 4000000,
	}
	mockVzMgr.EXPECT().GetVizierInfo(gomock.Any(), &uuidpb.UUID{Data: []byte(clusterID)}).
		Return(vzrInfoResp, nil)

	gqlSchema := LoadSchema(apiEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					cluster {
						id
						status
						lastHeartbeatMs
					}
				}
			`,
			ExpectedResult: `
				{
					"cluster": {
						"id":"7ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"status": "VZ_ST_HEALTHY",
						"lastHeartbeatMs": 4
					}
				}
			`,
		},
	})
}

func TestClusterConnectionInfo(t *testing.T) {
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	clusterID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	apiEnv, _, _, _, mockVzMgr, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	vzrIDs := make([]*uuidpb.UUID, 1)
	vzrIDs[0] = &uuidpb.UUID{Data: []byte(clusterID)}
	vzrResp := &vzmgrpb.GetViziersByOrgResponse{
		VizierIDs: vzrIDs,
	}
	mockVzMgr.EXPECT().GetViziersByOrg(gomock.Any(), &uuidpb.UUID{Data: []byte(orgID)}).
		Return(vzrResp, nil)

	vzrInfoResp := &cloudpb.VizierConnectionInfo{
		IPAddress: "127.0.0.1",
		Token:     "this-is-a-token",
	}
	mockVzMgr.EXPECT().GetVizierConnectionInfo(gomock.Any(), &uuidpb.UUID{Data: []byte(clusterID)}).
		Return(vzrInfoResp, nil)

	gqlSchema := LoadSchema(apiEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					clusterConnection {
						ipAddress
						token
					}
				}
			`,
			ExpectedResult: `
				{
					"clusterConnection": {
						"ipAddress": "127.0.0.1",
						"token": "this-is-a-token"
					}
				}
			`,
		},
	})
}
