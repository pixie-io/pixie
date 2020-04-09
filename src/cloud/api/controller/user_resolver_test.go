package controller_test

import (
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	mock_profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb/mock"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestUserInfoResolver(t *testing.T) {
	sCtx := authcontext.New()

	ctrl := gomock.NewController(t)
	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrgInfo := &profilepb.OrgInfo{
		ID:      pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		OrgName: "testOrg",
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	gqlEnv := controller.GraphQLEnv{
		ProfileServiceClient: mockProfile,
	}

	sCtx.Claims = utils.GenerateJWTForUser("abcdef", "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "test@test.com", time.Now())

	resolver := controller.UserInfoResolver{SessionCtx: sCtx, GQLEnv: &gqlEnv}
	assert.Equal(t, "test@test.com", resolver.Email())
	assert.Equal(t, graphql.ID("abcdef"), resolver.ID())
	assert.Equal(t, "testOrg", resolver.OrgName())
}
