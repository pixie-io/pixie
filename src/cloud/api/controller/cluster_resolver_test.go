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
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
)

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = &jwt.JWTClaims{}
	sCtx.Claims.Email = "test@test.com"
	sCtx.Claims.UserID = "abcdef"
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

	apiEnv, _, _, mockProfile, mockVzMgr, cleanup := testutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	expectedOrgReq := &profilepb.GetOrgByDomainRequest{
		DomainName: "test",
	}
	orgReply := &profilepb.OrgInfo{
		ID:         &uuidpb.UUID{Data: []byte(orgID)},
		OrgName:    "test.com",
		DomainName: "test",
	}
	mockProfile.EXPECT().GetOrgByDomain(gomock.Any(), expectedOrgReq).
		Return(orgReply, nil)

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
					CreateCluster(domainName: "test") {
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
