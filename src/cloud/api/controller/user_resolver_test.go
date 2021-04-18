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

package controller_test

import (
	"errors"
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go"
	"github.com/graph-gophers/graphql-go/gqltesting"
	"github.com/stretchr/testify/assert"

	public_cloudapipb "px.dev/pixie/src/api/public/cloudapipb"
	"px.dev/pixie/src/cloud/api/controller"
	gqltestutils "px.dev/pixie/src/cloud/api/controller/testutils"
	profilepb "px.dev/pixie/src/cloud/profile/profilepb"
	mock_profile "px.dev/pixie/src/cloud/profile/profilepb/mock"
	"px.dev/pixie/src/shared/services/authcontext"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
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
				ID:             utils.ProtoFromUUIDStrOrNil(userID),
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
				ID:      utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
				OrgName: "testOrg",
			}

			mockProfile.EXPECT().
				GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
				Return(mockOrgInfo, nil)

			gqlEnv := controller.GraphQLEnv{
				ProfileServiceClient: mockProfile,
			}

			sCtx.Claims = srvutils.GenerateJWTForUser(userID, "6ba7b810-9dad-11d1-80b4-00c04fd430c8", "test@test.com", time.Now(), "pixie")

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
		ID:   utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
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
		ID:     utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
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

func TestUserSettingsResolver_InviteUser(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockOrg.EXPECT().InviteUser(gomock.Any(), &public_cloudapipb.InviteUserRequest{
		Email:     "test@test.com",
		FirstName: "Tester",
		LastName:  "Person",
	}).Return(&public_cloudapipb.InviteUserResponse{
		Email:      "test@test.com",
		InviteLink: "https://pixie.ai/inviteLink",
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					InviteUser(email: "test@test.com", firstName: "Tester", lastName: "Person" ) { 
						email
						inviteLink
					}
				}
			`,
			ExpectedResult: `
				{
					"InviteUser": {
						"email": "test@test.com",
						"inviteLink": "https://pixie.ai/inviteLink"
					}
				}
			`,
		},
	})
}
