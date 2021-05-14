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
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/api/controller"
	gqltestutils "px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/cloud/profile/profilepb"
	mock_profile "px.dev/pixie/src/cloud/profile/profilepb/mock"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestUserSettingsResolver_UpdateOrg(t *testing.T) {
	_, mockClients, cleanup := gqltestutils.CreateTestAPIEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockProfile.EXPECT().UpdateOrg(gomock.Any(), &profilepb.UpdateOrgRequest{
		ID:              utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
		EnableApprovals: &types.BoolValue{Value: true},
	}).Return(nil, nil)

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
					UpdateOrg(orgInfo: {id: "6ba7b810-9dad-11d1-80b4-00c04fd430c9", enableApprovals: true })
				}
			`,
			ExpectedResult: `
				{
					"UpdateOrg": true
				}
			`,
		},
	})
}

func TestUserSettingsResolver_InviteUser(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockOrg.EXPECT().InviteUser(gomock.Any(), &cloudpb.InviteUserRequest{
		Email:     "test@test.com",
		FirstName: "Tester",
		LastName:  "Person",
	}).Return(&cloudpb.InviteUserResponse{
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

func TestUserSettingsResolver_OrgUsers(t *testing.T) {
	gqlEnv, _, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	ctrl := gomock.NewController(t)
	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUsersInOrg(gomock.Any(), &profilepb.GetUsersInOrgRequest{
			OrgID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		}).
		Return(&profilepb.GetUsersInOrgResponse{
			Users: []*profilepb.UserInfo{
				&profilepb.UserInfo{
					FirstName:  "first",
					LastName:   "last",
					IsApproved: false,
					Email:      "user@test.com",
				},
				&profilepb.UserInfo{
					FirstName:  "test",
					LastName:   "user",
					IsApproved: true,
					Email:      "test@test.com",
				},
			},
		}, nil)
	gqlEnv.ProfileServiceClient = mockProfile
	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					orgUsers {
						name
						email
						isApproved
					}
				}
			`,
			ExpectedResult: `
				{
					"orgUsers":
						[
						 {"name": "first last", "email": "user@test.com", "isApproved": false},
						 {"name": "test user", "email": "test@test.com", "isApproved": true}
						]
				}
			`,
		},
	})
}

func TestUserSettingsResolver_OrgInfo(t *testing.T) {
	gqlEnv, _, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	ctrl := gomock.NewController(t)
	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(&profilepb.OrgInfo{
			EnableApprovals: true,
			OrgName:         "test.com",
			ID:              utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		}, nil)
	gqlEnv.ProfileServiceClient = mockProfile
	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					org {
						name
						enableApprovals
					}
				}
			`,
			ExpectedResult: `
				{
					"org": {
						"name": "test.com",
						"enableApprovals": true
					}
				}
			`,
		},
	})
}
