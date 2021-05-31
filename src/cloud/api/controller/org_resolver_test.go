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
	gqltestutils "px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestUserSettingsResolver_UpdateOrg(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	idPb := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

	mockClients.MockOrg.EXPECT().UpdateOrg(gomock.Any(), &cloudpb.UpdateOrgRequest{
		ID:              idPb,
		EnableApprovals: &types.BoolValue{Value: true},
	}).Return(nil, nil)

	mockClients.MockOrg.EXPECT().
		GetOrg(gomock.Any(), idPb).
		Return(&cloudpb.OrgInfo{
			EnableApprovals: true,
			OrgName:         "testOrg.com",
			ID:              idPb,
		}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateOrgSettings(orgID: "6ba7b810-9dad-11d1-80b4-00c04fd430c9", orgSettings: { enableApprovals: true }) {
						name
						enableApprovals
					}
				}
			`,
			ExpectedResult: `
				{
					"UpdateOrgSettings": {
						"name": "testOrg.com",
						"enableApprovals": true
					}
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
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	//mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockClients.MockOrg.EXPECT().
		GetUsersInOrg(gomock.Any(), &cloudpb.GetUsersInOrgRequest{
			OrgID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		}).
		Return(&cloudpb.GetUsersInOrgResponse{
			Users: []*cloudpb.UserInfo{
				&cloudpb.UserInfo{
					FirstName:  "first",
					LastName:   "last",
					IsApproved: false,
					Email:      "user@test.com",
				},
				&cloudpb.UserInfo{
					FirstName:  "test",
					LastName:   "user",
					IsApproved: true,
					Email:      "test@test.com",
				},
			},
		}, nil)
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
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(&cloudpb.OrgInfo{
			EnableApprovals: true,
			OrgName:         "test.com",
			ID:              utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		}, nil)
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
