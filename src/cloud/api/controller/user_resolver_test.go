package controller_test

import (
	"errors"
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/gqltesting"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	gqltestutils "pixielabs.ai/pixielabs/src/cloud/api/controller/testutils"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	mock_profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb/mock"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/shared/services/utils"
	pbutils "pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestUserInfoResolver(t *testing.T) {
	userID := "123e4567-e89b-12d3-a456-426655440000"

	tests := []struct {
		name            string
		mockUser        *profilepb.UserInfo
		error           error
		expectedName    string
		expectedPicture string
	}{
		{
			name: "existing user",
			mockUser: &profilepb.UserInfo{
				ID:             pbutils.ProtoFromUUIDStrOrNil(userID),
				ProfilePicture: "test",
				FirstName:      "first",
				LastName:       "last",
			},
			error:           nil,
			expectedName:    "first last",
			expectedPicture: "test",
		},
		{
			name:     "no user",
			mockUser: nil,
			error:    errors.New("Could not get user"),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
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

			sCtx.Claims = utils.GenerateJWTForUser(userID, "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "test@test.com", time.Now())

			resolver := controller.UserInfoResolver{SessionCtx: sCtx, GQLEnv: &gqlEnv, UserInfo: test.mockUser}
			assert.Equal(t, "test@test.com", resolver.Email())
			assert.Equal(t, graphql.ID(userID), resolver.ID())
			assert.Equal(t, "testOrg", resolver.OrgName())
			assert.Equal(t, test.expectedPicture, resolver.Picture())
			assert.Equal(t, test.expectedName, resolver.Name())
		})
	}
}

func TestUserSettingsResolver_GetUserSettings(t *testing.T) {
	_, mockClients, cleanup := gqltestutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockProfile.EXPECT().GetUserSettings(gomock.Any(), &profilepb.GetUserSettingsRequest{
		ID:   pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
		Keys: []string{"test", "a_key"},
	}).Return(&profilepb.GetUserSettingsResponse{
		Keys:   []string{"test", "a_key"},
		Values: []string{"a", "b"},
	}, nil)

	gqlEnv := controller.GraphQLEnv{
		ProfileServiceClient: mockClients.MockProfile,
	}

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					userSettings(keys: ["test", "a_key"]) {
						key
						value
					}
				}
			`,
			ExpectedResult: `
				{
					"userSettings":
						[
						 {"key": "test", "value": "a"},
						 {"key": "a_key", "value": "b"}
						]
				}
			`,
		},
	})
}

func TestUserSettingsResolver_UpdateUserSettings(t *testing.T) {
	_, mockClients, cleanup := gqltestutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockProfile.EXPECT().UpdateUserSettings(gomock.Any(), &profilepb.UpdateUserSettingsRequest{
		ID:     pbutils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
		Keys:   []string{"test", "a_key"},
		Values: []string{"c", "d"},
	}).Return(&profilepb.UpdateUserSettingsResponse{
		OK: true,
	}, nil)

	gqlEnv := controller.GraphQLEnv{
		ProfileServiceClient: mockClients.MockProfile,
	}

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateUserSettings(keys: ["test", "a_key"], values: ["c", "d"])
				}
			`,
			ExpectedResult: `
				{
					"UpdateUserSettings": true
				}
			`,
		},
	})
}
