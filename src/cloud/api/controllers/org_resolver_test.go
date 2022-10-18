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

package controllers_test

import (
	"context"
	"testing"

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	gqltestutils "px.dev/pixie/src/cloud/api/controllers/testutils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestOrgSettingsResolver_UpdateOrg(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

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
		})
	}
}

func TestOrgSettingsResolver_InviteUser(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

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
		})
	}
}

func TestOrgSettingsResolver_CreateOrg(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "user no org",
			ctx:  CreateTestContextNoOrg(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

			mockClients.MockOrg.EXPECT().CreateOrg(gomock.Any(), &cloudpb.CreateOrgRequest{
				OrgName: "my_new_org",
			}).Return(orgID, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						mutation {
							CreateOrg(orgName: "my_new_org")
						}
					`,
					ExpectedResult: `
						{
							"CreateOrg": "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
						}
					`,
				},
			})
		})
	}
}

func TestOrgSettingsResolver_OrgUsers(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			//mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
			mockClients.MockOrg.EXPECT().
				GetUsersInOrg(gomock.Any(), &cloudpb.GetUsersInOrgRequest{
					OrgID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
				}).
				Return(&cloudpb.GetUsersInOrgResponse{
					Users: []*cloudpb.UserInfo{
						{
							FirstName:  "first",
							LastName:   "last",
							IsApproved: false,
							Email:      "user@test.com",
						},
						{
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
		})
	}
}

func TestOrgSettingsResolver_OrgInfo(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockOrg.EXPECT().
				GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
				Return(&cloudpb.OrgInfo{
					EnableApprovals: true,
					OrgName:         "test.com",
					DomainName:      "test.com",
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
								domainName
								enableApprovals
							}
						}
					`,
					ExpectedResult: `
						{
							"org": {
								"name": "test.com",
								"domainName": "test.com",
								"enableApprovals": true
							}
						}
					`,
				},
			})
		})
	}
}

func TestOrgSettingsResolver_IDEConfigs(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "regular user",
			ctx:  CreateTestContext(),
		},
		{
			name: "api user",
			ctx:  CreateAPIUserTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockOrg.EXPECT().
				GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
				Return(&cloudpb.OrgInfo{
					EnableApprovals: true,
					OrgName:         "test.com",
					ID:              utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
				}, nil)

			mockClients.MockOrg.EXPECT().
				GetOrgIDEConfigs(gomock.Any(), &cloudpb.GetOrgIDEConfigsRequest{
					OrgID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
				}).
				Return(&cloudpb.GetOrgIDEConfigsResponse{
					Configs: []*cloudpb.IDEConfig{
						{
							IDEName: "test",
							Path:    "subl://{{symbol}}",
						},
						{
							IDEName: "anothertest",
							Path:    "test://{{symbol}}",
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
							org {
								name
								enableApprovals
								idePaths {
									IDEName
									path
								}
							}
						}
					`,
					ExpectedResult: `
						{
							"org": {
								"name": "test.com",
								"enableApprovals": true,
								"idePaths": [{ "IDEName": "test", "path": "subl://{{symbol}}"}, { "IDEName": "anothertest", "path": "test://{{symbol}}"}]
							}
						}
					`,
				},
			})
		})
	}
}

func TestOrgSettingsResolver_CreateInviteToken(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "user",
			ctx:  CreateTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			idPb := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

			mockClients.MockOrg.EXPECT().CreateInviteToken(gomock.Any(), &cloudpb.CreateInviteTokenRequest{
				OrgID: idPb,
			}).Return(&cloudpb.InviteToken{SignedClaims: "jwt_invite_claim.with_signature"}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						mutation {
							CreateInviteToken(orgID: "6ba7b810-9dad-11d1-80b4-00c04fd430c9")
						}
					`,
					ExpectedResult: `
						{
							"CreateInviteToken": "jwt_invite_claim.with_signature"
						}
					`,
				},
			})
		})
	}
}

func TestOrgSettingsResolver_RevokeAllInviteTokens(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "user",
			ctx:  CreateTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			idPb := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

			mockClients.MockOrg.EXPECT().RevokeAllInviteTokens(gomock.Any(), idPb).Return(&types.Empty{}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						mutation {
							RevokeAllInviteTokens(orgID: "6ba7b810-9dad-11d1-80b4-00c04fd430c9")
						}
					`,
					ExpectedResult: `
						{
							"RevokeAllInviteTokens": true
						}
					`,
				},
			})
		})
	}
}

func TestOrgSettingsResolver_VerifyInviteToken(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "no auth",
			ctx:  context.Background(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			mockClients.MockOrg.EXPECT().VerifyInviteToken(gomock.Any(), &cloudpb.InviteToken{
				SignedClaims: "jwt_invite_claim.with_signature",
			}).Return(&cloudpb.VerifyInviteTokenResponse{Valid: true}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						query {
							verifyInviteToken(inviteToken: "jwt_invite_claim.with_signature")
						}
					`,
					ExpectedResult: `
						{
							"verifyInviteToken": true
						}
					`,
				},
			})
		})
	}
}

func TestOrgSettingsResolver_RemoveUserFromOrg(t *testing.T) {
	tests := []struct {
		name string
		ctx  context.Context
	}{
		{
			name: "user",
			ctx:  CreateTestContext(),
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
			defer cleanup()
			ctx := test.ctx

			idPb := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd43000")

			mockClients.MockOrg.EXPECT().RemoveUserFromOrg(gomock.Any(), &cloudpb.RemoveUserFromOrgRequest{
				UserID: idPb,
			}).Return(&cloudpb.RemoveUserFromOrgResponse{Success: true}, nil)

			gqlSchema := LoadSchema(gqlEnv)
			gqltesting.RunTests(t, []*gqltesting.Test{
				{
					Schema:  gqlSchema,
					Context: ctx,
					Query: `
						mutation {
							RemoveUserFromOrg(userID: "6ba7b810-9dad-11d1-80b4-00c04fd43000")
						}
					`,
					ExpectedResult: `
						{
							"RemoveUserFromOrg": true
						}
					`,
				},
			})
		})
	}
}
