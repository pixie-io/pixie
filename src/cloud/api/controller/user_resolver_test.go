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

	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/graph-gophers/graphql-go/gqltesting"

	"px.dev/pixie/src/api/proto/cloudpb"
	gqltestutils "px.dev/pixie/src/cloud/api/controller/testutils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func TestUserInfoResolver_GetUserInfo(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	userID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")
	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	mockClients.MockUser.EXPECT().GetUser(gomock.Any(), userID).Return(&cloudpb.UserInfo{
		ID:             userID,
		OrgID:          orgID,
		ProfilePicture: "test",
		FirstName:      "first",
		LastName:       "last",
		Email:          "test@test.com",
		IsApproved:     true,
	}, nil)

	mockClients.MockUser.EXPECT().GetOrg(gomock.Any(), orgID).Return(&cloudpb.OrgInfo{
		EnableApprovals: true,
		OrgName:         "test.com",
		ID:              orgID,
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					user {
						id
						name
						email
						picture
						isApproved
						orgID
						orgName
					}
				}
			`,
			ExpectedResult: `
				{
					"user": {
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
						"name": "first last",
						"email": "test@test.com",
						"picture": "test",
						"orgID": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"orgName": "test.com",
						"isApproved": true
					}
				}
			`,
		},
	})
}

func TestUserInfoResolver_GetSupportUserInfo(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	userID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")
	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	// Will fetch info from the context instead.
	mockClients.MockUser.EXPECT().GetUser(gomock.Any(), userID).Return(nil, errors.New("no such person"))
	mockClients.MockUser.EXPECT().GetOrg(gomock.Any(), orgID).Return(&cloudpb.OrgInfo{
		EnableApprovals: true,
		OrgName:         "test.com",
		ID:              orgID,
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					user {
						id
						name
						email
						picture
						orgID
						orgName
					}
				}
				`,
			ExpectedResult: `
				{
					"user": {
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
						"name": " ",
						"email": "test@test.com",
						"picture": "",
						"orgID": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
						"orgName": "test.com"
					}
				}
			`,
		},
	})
}

func TestUserInfoResolver_UpdateUserPermissions(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	userID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

	mockClients.MockUser.EXPECT().UpdateUser(gomock.Any(), &cloudpb.UpdateUserRequest{
		ID:         userID,
		IsApproved: &types.BoolValue{Value: true},
	}).Return(nil, nil)

	mockClients.MockUser.EXPECT().GetUser(gomock.Any(), userID).Return(&cloudpb.UserInfo{
		ID:             userID,
		OrgID:          utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		ProfilePicture: "test",
		FirstName:      "first",
		LastName:       "last",
		Email:          "test@test.com",
		IsApproved:     true,
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateUserPermissions(userID: "6ba7b810-9dad-11d1-80b4-00c04fd430c9", userPermissions: { isApproved: true }) {
						name
						isApproved
					}
				}
			`,
			ExpectedResult: `
				{
					"UpdateUserPermissions": {
						"name": "first last",
						"isApproved": true
					}
				}
			`,
		},
	})
}

func TestUserSettingsResolver_GetUserSettings(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockUser.EXPECT().GetUserSettings(gomock.Any(), &cloudpb.GetUserSettingsRequest{
		ID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
	}).Return(&cloudpb.GetUserSettingsResponse{
		AnalyticsOptout: true,
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					userSettings {
						analyticsOptout
						id
					}
				}
			`,
			ExpectedResult: `
				{
					"userSettings": {
                    	"analyticsOptout": true,
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c9"
					}
				}
			`,
		},
	})
}

func TestUserSettingsResolver_UpdateUserSettings(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockUser.EXPECT().UpdateUserSettings(gomock.Any(), &cloudpb.UpdateUserSettingsRequest{
		ID:              utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
		AnalyticsOptout: &types.BoolValue{Value: true},
	}).Return(&cloudpb.UpdateUserSettingsResponse{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					UpdateUserSettings(settings: { analyticsOptout: true }) {
						analyticsOptout
						id
					}
				}
			`,
			ExpectedResult: `
				{
					"UpdateUserSettings": {
						"analyticsOptout": true,
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c9"
					}
				}
			`,
		},
	})
}

func TestUserSettingsResolver_GetUserAttributes(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockUser.EXPECT().GetUserAttributes(gomock.Any(), &cloudpb.GetUserAttributesRequest{
		ID: utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
	}).Return(&cloudpb.GetUserAttributesResponse{
		TourSeen: true,
	}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				query {
					userAttributes {
						tourSeen
						id
					}
				}
			`,
			ExpectedResult: `
				{
					"userAttributes": {
                    	"tourSeen": true,
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c9"
					}
				}
			`,
		},
	})
}

func TestUserSettingsResolver_SetUserAttributes(t *testing.T) {
	gqlEnv, mockClients, cleanup := gqltestutils.CreateTestGraphQLEnv(t)
	defer cleanup()
	ctx := CreateTestContext()

	mockClients.MockUser.EXPECT().SetUserAttributes(gomock.Any(), &cloudpb.SetUserAttributesRequest{
		ID:       utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9"),
		TourSeen: &types.BoolValue{Value: true},
	}).Return(&cloudpb.SetUserAttributesResponse{}, nil)

	gqlSchema := LoadSchema(gqlEnv)
	gqltesting.RunTests(t, []*gqltesting.Test{
		{
			Schema:  gqlSchema,
			Context: ctx,
			Query: `
				mutation {
					SetUserAttributes(attributes: { tourSeen: true }) {
						tourSeen
						id
					}
				}
			`,
			ExpectedResult: `
				{
					"SetUserAttributes": {
						"tourSeen": true,
						"id": "6ba7b810-9dad-11d1-80b4-00c04fd430c9"
					}
				}
			`,
		},
	})
}
