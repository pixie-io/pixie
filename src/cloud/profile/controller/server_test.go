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
	"context"
	"fmt"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/controller"
	mock_controller "px.dev/pixie/src/cloud/profile/controller/mock"
	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/profileenv"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	mock_projectmanager "px.dev/pixie/src/cloud/project_manager/projectmanagerpb/mock"
	"px.dev/pixie/src/shared/services/authcontext"
	svcutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

func TestServer_CreateUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	testOrgUUID := uuid.Must(uuid.NewV4())
	testUUID := uuid.Must(uuid.NewV4())
	createUsertests := []struct {
		name      string
		makesCall bool
		userInfo  *profilepb.CreateUserRequest

		expectErr  bool
		expectCode codes.Code
		respID     *uuidpb.UUID

		enableApprovals bool
	}{
		{
			name:      "valid request",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "foo@bar.com",
				IdentityProvider: "github",
				AuthProviderID:   "github|abcdefg",
			},
			expectErr:       false,
			expectCode:      codes.OK,
			respID:          utils.ProtoFromUUID(testUUID),
			enableApprovals: false,
		},
		{
			name:      "invalid orgid",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            &uuidpb.UUID{},
				Username:         "foobar",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "foo@bar.com",
				IdentityProvider: "github",
				AuthProviderID:   "github|asdfghjkl;",
			},
			expectErr:       true,
			expectCode:      codes.InvalidArgument,
			respID:          nil,
			enableApprovals: false,
		},
		{
			name:      "invalid username",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "foo@bar.com",
				IdentityProvider: "github",
				AuthProviderID:   "github|asdfghjkl;",
			},
			expectErr:       true,
			expectCode:      codes.InvalidArgument,
			respID:          nil,
			enableApprovals: false,
		},
		{
			name:      "empty first name is ok",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "",
				LastName:         "bar",
				Email:            "foo@bar.com",
				IdentityProvider: "github",
				AuthProviderID:   "github|asdfghjkl;",
			},
			expectErr:       false,
			expectCode:      codes.OK,
			respID:          utils.ProtoFromUUID(testUUID),
			enableApprovals: false,
		},
		{
			name:      "empty email",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "",
				IdentityProvider: "github",
			},
			expectErr:       true,
			expectCode:      codes.InvalidArgument,
			respID:          nil,
			enableApprovals: false,
		},
		{
			name:      "banned email",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "foo@blocklist.com",
				IdentityProvider: "github",
			},
			expectErr:       true,
			expectCode:      codes.InvalidArgument,
			respID:          nil,
			enableApprovals: false,
		},
		{
			name:      "allowed email",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "foo@gmail.com",
				IdentityProvider: "github",
			},
			expectErr:       false,
			expectCode:      codes.OK,
			respID:          utils.ProtoFromUUID(testUUID),
			enableApprovals: false,
		},
		{
			name:      "invalid email",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "foo",
				LastName:         "bar",
				Email:            "foo.com",
				IdentityProvider: "github",
			},
			expectErr:       true,
			expectCode:      codes.InvalidArgument,
			respID:          nil,
			enableApprovals: false,
		},
		{
			name:      "enable approvals properly sets users info",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "",
				LastName:         "bar",
				Email:            "foo@bar.com",
				IdentityProvider: "github",
			},
			expectErr:       false,
			expectCode:      codes.OK,
			respID:          utils.ProtoFromUUID(testUUID),
			enableApprovals: true,
		},
		{
			name:      "identity provider empty throws an error",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
				Username:         "foobar",
				FirstName:        "",
				LastName:         "bar",
				Email:            "foo@bar.com",
				IdentityProvider: "",
			},
			expectErr:       true,
			expectCode:      codes.InvalidArgument,
			respID:          nil,
			enableApprovals: false,
		},
	}

	for _, tc := range createUsertests {
		t.Run(tc.name, func(t *testing.T) {
			s := controller.NewServer(nil, uds, usds, ods, osds)
			if utils.UUIDFromProtoOrNil(tc.userInfo.OrgID) != uuid.Nil {
				ods.EXPECT().
					GetOrg(testOrgUUID).
					Return(&datastore.OrgInfo{
						EnableApprovals: tc.enableApprovals,
					}, nil)
			}
			if tc.makesCall {
				req := &datastore.UserInfo{
					OrgID:            testOrgUUID,
					Username:         tc.userInfo.Username,
					FirstName:        tc.userInfo.FirstName,
					LastName:         tc.userInfo.LastName,
					Email:            tc.userInfo.Email,
					IsApproved:       !tc.enableApprovals,
					IdentityProvider: tc.userInfo.IdentityProvider,
					AuthProviderID:   tc.userInfo.AuthProviderID,
				}
				uds.EXPECT().
					CreateUser(req).
					Return(testUUID, nil)
			}
			resp, err := s.CreateUser(context.Background(), tc.userInfo)

			if tc.expectErr {
				assert.NotNil(t, err)
				c := status.Code(err)
				assert.Equal(t, tc.expectCode, c)
				return
			}

			assert.Equal(t, tc.respID, resp)
		})
	}
}

func TestServer_GetUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.UserInfo{
		ID:             userUUID,
		OrgID:          orgUUID,
		Username:       "foobar",
		FirstName:      "foo",
		LastName:       "bar",
		Email:          "foo@bar.com",
		AuthProviderID: "github|asdfghjkl;",
	}

	uds.EXPECT().
		GetUser(userUUID).
		Return(mockReply, nil)

	resp, err := s.GetUser(context.Background(), utils.ProtoFromUUID(userUUID))

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(userUUID))
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUID(orgUUID))
	assert.Equal(t, resp.Username, "foobar")
	assert.Equal(t, resp.FirstName, "foo")
	assert.Equal(t, resp.LastName, "bar")
	assert.Equal(t, resp.Email, "foo@bar.com")
	assert.Equal(t, resp.AuthProviderID, "github|asdfghjkl;")
}

func TestServer_GetUser_MissingUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)
	uds.EXPECT().
		GetUser(userUUID).
		Return(nil, nil)

	resp, err := s.GetUser(context.Background(), utils.ProtoFromUUID(userUUID))
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_GetUserByEmail(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.UserInfo{
		ID:               userUUID,
		OrgID:            orgUUID,
		Username:         "foobar",
		FirstName:        "foo",
		LastName:         "bar",
		Email:            "foo@bar.com",
		IdentityProvider: "github",
		AuthProviderID:   "github|asdfghjkl;",
	}

	uds.EXPECT().
		GetUserByEmail("foo@bar.com").
		Return(mockReply, nil)

	resp, err := s.GetUserByEmail(
		context.Background(),
		&profilepb.GetUserByEmailRequest{Email: "foo@bar.com"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(userUUID))
	assert.Equal(t, resp.Email, "foo@bar.com")
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUID(orgUUID))
	assert.Equal(t, resp.IdentityProvider, "github")
	assert.Equal(t, resp.AuthProviderID, "github|asdfghjkl;")
}

func TestServer_GetUserByAuthProviderID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.UserInfo{
		ID:               userUUID,
		OrgID:            orgUUID,
		Username:         "foobar",
		FirstName:        "foo",
		LastName:         "bar",
		Email:            "foo@bar.com",
		IdentityProvider: "github",
		AuthProviderID:   "github|asdfghjkl;",
	}

	uds.EXPECT().
		GetUserByAuthProviderID("github|asdfghjkl;").
		Return(mockReply, nil)

	resp, err := s.GetUserByAuthProviderID(
		context.Background(),
		&profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: "github|asdfghjkl;"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(userUUID))
	assert.Equal(t, resp.Email, "foo@bar.com")
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUID(orgUUID))
	assert.Equal(t, resp.IdentityProvider, "github")
	assert.Equal(t, resp.AuthProviderID, "github|asdfghjkl;")
}

func TestServer_GetUserByEmail_MissingEmail(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	uds.EXPECT().
		GetUserByEmail("foo@bar.com").
		Return(nil, datastore.ErrUserNotFound)

	resp, err := s.GetUserByEmail(
		context.Background(),
		&profilepb.GetUserByEmailRequest{Email: "foo@bar.com"})

	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_CreateOrgAndUser_SuccessCases(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	testOrgUUID := uuid.Must(uuid.NewV4())
	testUUID := uuid.Must(uuid.NewV4())
	createOrgUserTest := []struct {
		name string
		req  *profilepb.CreateOrgAndUserRequest
		resp *profilepb.CreateOrgAndUserResponse
	}{
		{
			name: "valid request",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "foobar",
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "foo@bar.com",
					IdentityProvider: "github",
					AuthProviderID:   "github|asdfghjkl;",
				},
			},
			resp: &profilepb.CreateOrgAndUserResponse{
				OrgID:  utils.ProtoFromUUID(testOrgUUID),
				UserID: utils.ProtoFromUUID(testUUID),
			},
		}, {
			name: "allowed email",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "foobar",
					FirstName:        "foo",
					LastName:         "",
					Email:            "foo@gmail.com",
					IdentityProvider: "github",
					AuthProviderID:   "github|asdfghjkl;",
				},
			},
			resp: &profilepb.CreateOrgAndUserResponse{
				OrgID:  utils.ProtoFromUUID(testOrgUUID),
				UserID: utils.ProtoFromUUID(testUUID),
			},
		},
	}

	for _, tc := range createOrgUserTest {
		t.Run(tc.name, func(t *testing.T) {
			pm := mock_projectmanager.NewMockProjectManagerServiceClient(ctrl)
			req := &projectmanagerpb.RegisterProjectRequest{
				ProjectName: controller.DefaultProjectName,
				OrgID:       utils.ProtoFromUUID(testOrgUUID),
			}
			resp := &projectmanagerpb.RegisterProjectResponse{
				ProjectRegistered: true,
			}
			pm.EXPECT().RegisterProject(gomock.Any(), req).Return(resp, nil)

			env := profileenv.New(pm)

			s := controller.NewServer(env, uds, usds, ods, osds)
			exUserInfo := &datastore.UserInfo{
				Username:         tc.req.User.Username,
				FirstName:        tc.req.User.FirstName,
				LastName:         tc.req.User.LastName,
				Email:            tc.req.User.Email,
				IsApproved:       true,
				IdentityProvider: tc.req.User.IdentityProvider,
				AuthProviderID:   tc.req.User.AuthProviderID,
			}
			exOrg := &datastore.OrgInfo{
				DomainName: tc.req.Org.DomainName,
				OrgName:    tc.req.Org.OrgName,
			}
			uds.EXPECT().
				CreateUserAndOrg(exOrg, exUserInfo).
				Return(testOrgUUID, testUUID, nil)
			orgResp, err := s.CreateOrgAndUser(context.Background(), tc.req)
			require.NoError(t, err)
			assert.Equal(t, orgResp, tc.resp)
		})
	}
}

func TestServer_CreateOrgAndUser_InvalidArgumentCases(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	createOrgUserTest := []struct {
		name string
		req  *profilepb.CreateOrgAndUserRequest
	}{
		{
			name: "invalid org name",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "",
					DomainName: "my-org.com",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "foobar",
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "foo@bar.com",
					IdentityProvider: "github",
				},
			},
		},
		{
			name: "invalid domain name",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "foobar",
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "foo@bar.com",
					IdentityProvider: "github",
				},
			},
		},
		{
			name: "invalid username",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "",
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "foo@bar.com",
					IdentityProvider: "github",
				},
			},
		},
		{
			name: "missing email",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "foobar",
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "",
					IdentityProvider: "github",
				},
			},
		},
		{
			name: "banned email",
			req: &profilepb.CreateOrgAndUserRequest{
				Org: &profilepb.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profilepb.CreateOrgAndUserRequest_User{
					Username:         "foobar",
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "foo@blocklist.com",
					IdentityProvider: "github",
				},
			},
		},
	}

	for _, tc := range createOrgUserTest {
		t.Run(tc.name, func(t *testing.T) {
			pm := mock_projectmanager.NewMockProjectManagerServiceClient(ctrl)
			env := profileenv.New(pm)
			s := controller.NewServer(env, uds, usds, ods, osds)
			resp, err := s.CreateOrgAndUser(context.Background(), tc.req)
			assert.NotNil(t, err)
			assert.Nil(t, resp)
			c := status.Code(err)
			assert.Equal(t, c, codes.InvalidArgument)
		})
	}
}

func TestServer_CreateOrgAndUser_CreateProjectFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	testOrgUUID := uuid.Must(uuid.NewV4())
	testUUID := uuid.Must(uuid.NewV4())

	pm := mock_projectmanager.NewMockProjectManagerServiceClient(ctrl)
	projectReq := &projectmanagerpb.RegisterProjectRequest{
		ProjectName: controller.DefaultProjectName,
		OrgID:       utils.ProtoFromUUID(testOrgUUID),
	}

	pm.EXPECT().RegisterProject(gomock.Any(), projectReq).Return(nil, fmt.Errorf("an error"))

	env := profileenv.New(pm)

	req := &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    "my-org",
			DomainName: "my-org.com",
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:         "foobar",
			FirstName:        "foo",
			LastName:         "bar",
			Email:            "foo@bar.com",
			IdentityProvider: "github",
		},
	}

	s := controller.NewServer(env, uds, usds, ods, osds)
	exUserInfo := &datastore.UserInfo{
		Username:         req.User.Username,
		FirstName:        req.User.FirstName,
		LastName:         req.User.LastName,
		Email:            req.User.Email,
		IsApproved:       true,
		IdentityProvider: "github",
	}
	exOrg := &datastore.OrgInfo{
		DomainName: req.Org.DomainName,
		OrgName:    req.Org.OrgName,
	}
	uds.EXPECT().
		CreateUserAndOrg(exOrg, exUserInfo).
		Return(testOrgUUID, testUUID, nil)

	ods.EXPECT().
		DeleteOrgAndUsers(testOrgUUID).
		Return(nil)

	resp, err := s.CreateOrgAndUser(context.Background(), req)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_GetOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	}

	ods.EXPECT().
		GetOrg(orgUUID).
		Return(mockReply, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(orgUUID))

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgUUID))
	assert.Equal(t, resp.DomainName, "my-org.com")
	assert.Equal(t, resp.OrgName, "my-org")
}

func TestServer_GetOrgs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	org2UUID := uuid.Must(uuid.NewV4())

	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := []*datastore.OrgInfo{{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	},
		{
			ID:         org2UUID,
			DomainName: "pixie.com",
			OrgName:    "pixie",
		}}

	ods.EXPECT().
		GetOrgs().
		Return(mockReply, nil)

	resp, err := s.GetOrgs(context.Background(), &profilepb.GetOrgsRequest{})

	require.NoError(t, err)
	assert.Equal(t, 2, len(resp.Orgs))
	assert.Equal(t, utils.ProtoFromUUID(orgUUID), resp.Orgs[0].ID)
	assert.Equal(t, "my-org.com", resp.Orgs[0].DomainName)
	assert.Equal(t, "my-org", resp.Orgs[0].OrgName)
	assert.Equal(t, utils.ProtoFromUUID(org2UUID), resp.Orgs[1].ID)
	assert.Equal(t, "pixie.com", resp.Orgs[1].DomainName)
	assert.Equal(t, "pixie", resp.Orgs[1].OrgName)
}

func TestServer_GetOrg_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetOrg(orgUUID).
		Return(nil, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(orgUUID))
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_GetOrgByDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	}

	ods.EXPECT().
		GetOrgByDomain("my-org.com").
		Return(mockReply, nil)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profilepb.GetOrgByDomainRequest{DomainName: "my-org.com"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgUUID))
	assert.Equal(t, resp.DomainName, "my-org.com")
	assert.Equal(t, resp.OrgName, "my-org")
}

func TestServer_GetOrgByDomain_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetOrgByDomain("my-org.com").
		Return(nil, datastore.ErrOrgNotFound)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profilepb.GetOrgByDomainRequest{DomainName: "my-org.com"})

	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_DeleteOrgAndUsers(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	orgUUID := uuid.Must(uuid.NewV4())

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	}
	ods.EXPECT().GetOrg(orgUUID).Return(mockReply, nil)
	ods.EXPECT().DeleteOrgAndUsers(orgUUID).Return(nil)

	err := s.DeleteOrgAndUsers(context.Background(), utils.ProtoFromUUID(orgUUID))
	require.NoError(t, err)
}

func TestServer_DeleteOrgAndUsers_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	orgUUID := uuid.Must(uuid.NewV4())
	ods.EXPECT().
		GetOrg(orgUUID).
		Return(nil, nil)

	err := s.DeleteOrgAndUsers(context.Background(), utils.ProtoFromUUID(orgUUID))
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_UpdateUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	updateUserTest := []struct {
		name              string
		userID            string
		userOrg           string
		updatedProfilePic string
		updatedIsApproved bool
	}{
		{
			name:              "user can update their own profile picture",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "new",
			updatedIsApproved: false,
		},
		{
			name:              "admin can update another's profile picture",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "new",
			updatedIsApproved: false,
		},
		{
			name:              "user should approve other user in org",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "something",
			updatedIsApproved: true,
		},
	}

	for _, tc := range updateUserTest {
		t.Run(tc.name, func(t *testing.T) {
			ctx := CreateTestContext()
			s := controller.NewServer(nil, uds, usds, ods, osds)
			userID := uuid.FromStringOrNil(tc.userID)

			// This is the original user's info.
			profilePicture := "something"
			originalUserInfo := &datastore.UserInfo{
				ID:             userID,
				FirstName:      "first",
				LastName:       "last",
				ProfilePicture: &profilePicture,
				IsApproved:     false,
				OrgID:          uuid.FromStringOrNil(tc.userOrg),
			}

			req := &profilepb.UpdateUserRequest{
				ID: utils.ProtoFromUUID(userID),
			}

			mockUpdateReq := &datastore.UserInfo{
				ID:             userID,
				FirstName:      "first",
				LastName:       "last",
				ProfilePicture: &profilePicture,
				IsApproved:     false,
				OrgID:          uuid.FromStringOrNil(tc.userOrg),
			}

			if tc.updatedProfilePic != profilePicture {
				req.DisplayPicture = &types.StringValue{Value: tc.updatedProfilePic}
				mockUpdateReq.ProfilePicture = &tc.updatedProfilePic
			}

			if tc.updatedIsApproved != originalUserInfo.IsApproved {
				req.IsApproved = &types.BoolValue{Value: tc.updatedIsApproved}
				mockUpdateReq.IsApproved = tc.updatedIsApproved
			}

			uds.EXPECT().
				GetUser(userID).
				Return(originalUserInfo, nil)

			uds.EXPECT().
				UpdateUser(mockUpdateReq).
				Return(nil)

			resp, err := s.UpdateUser(ctx, req)
			require.NoError(t, err)
			assert.Equal(t, resp.ID, utils.ProtoFromUUID(userID))
			assert.Equal(t, resp.ProfilePicture, tc.updatedProfilePic)
			assert.Equal(t, resp.IsApproved, tc.updatedIsApproved)
		})
	}
}

func TestServer_UpdateOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: false,
	}

	mockUpdateReq := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: true,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	ods.EXPECT().
		UpdateOrg(mockUpdateReq).
		Return(nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID:              utils.ProtoFromUUID(orgID),
			EnableApprovals: &types.BoolValue{Value: true},
		})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgID))
	assert.Equal(t, resp.EnableApprovals, true)
}

func TestServer_UpdateOrg_DisableApprovals(t *testing.T) {
	// Disabling approvals now causes all existing users to become approved.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID: orgID,
		// Start with approvals enabled.
		EnableApprovals: true,
	}

	mockUpdateReq := &datastore.OrgInfo{
		ID: orgID,
		// Disable approvals.
		EnableApprovals: false,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	ods.EXPECT().
		UpdateOrg(mockUpdateReq).
		Return(nil)

	ods.EXPECT().
		ApproveAllOrgUsers(orgID).
		Return(nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID: utils.ProtoFromUUID(orgID),
			// Disable approvals.
			EnableApprovals: &types.BoolValue{Value: false},
		})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgID))
	assert.Equal(t, resp.EnableApprovals, false)

	// TODO go through all users and get approvals.
}

func TestServer_UpdateOrg_NoChangeInState(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: true,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID:              utils.ProtoFromUUID(orgID),
			EnableApprovals: &types.BoolValue{Value: true},
		},
	)

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgID))
	assert.Equal(t, resp.EnableApprovals, true)
}

func TestServer_UpdateOrg_EnableApprovalsIsNull(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: true,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID: utils.ProtoFromUUID(orgID),
		},
	)

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgID))
	assert.Equal(t, resp.EnableApprovals, true)
}

func TestServer_UpdateOrg_RequestBlockedForUserOutsideOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)
	_, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			// Random org that doesn't match org claims in context.
			ID: utils.ProtoFromUUID(uuid.Must(uuid.NewV4())),
		},
	)

	require.Regexp(t, "user does not have permission", err)
}

func TestServer_GetUserAttributes(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	tourSeen := true
	usds.EXPECT().
		GetUserAttributes(userID).
		Return(&datastore.UserAttributes{TourSeen: &tourSeen}, nil)

	resp, err := s.GetUserAttributes(context.Background(), &profilepb.GetUserAttributesRequest{
		ID: utils.ProtoFromUUID(userID),
	})
	require.NoError(t, err)
	assert.Equal(t, true, resp.TourSeen)
}

func TestServer_SetUserAttributes(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	tourSeen := true
	usds.EXPECT().
		SetUserAttributes(&datastore.UserAttributes{
			UserID:   userID,
			TourSeen: &tourSeen,
		}).
		Return(nil)

	resp, err := s.SetUserAttributes(context.Background(), &profilepb.SetUserAttributesRequest{
		ID:       utils.ProtoFromUUID(userID),
		TourSeen: &types.BoolValue{Value: true},
	})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.SetUserAttributesResponse{}, resp)
}

func TestServer_GetUserSettings(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	analyticsOptout := true
	usds.EXPECT().
		GetUserSettings(userID).
		Return(&datastore.UserSettings{AnalyticsOptout: &analyticsOptout}, nil)

	resp, err := s.GetUserSettings(context.Background(), &profilepb.GetUserSettingsRequest{
		ID: utils.ProtoFromUUID(userID),
	})
	require.NoError(t, err)
	assert.Equal(t, true, resp.AnalyticsOptout)
}

func TestServer_UpdateUserSettings(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	analyticsOptout := true
	usds.EXPECT().
		UpdateUserSettings(&datastore.UserSettings{
			UserID:          userID,
			AnalyticsOptout: &analyticsOptout,
		}).
		Return(nil)

	resp, err := s.UpdateUserSettings(context.Background(), &profilepb.UpdateUserSettingsRequest{
		ID:              utils.ProtoFromUUID(userID),
		AnalyticsOptout: &types.BoolValue{Value: true},
	})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.UpdateUserSettingsResponse{}, resp)
}

func CreateTestContext() context.Context {
	sCtx := authcontext.New()
	sCtx.Claims = svcutils.GenerateJWTForUser(
		"6ba7b810-9dad-11d1-80b4-00c04fd430c9",
		"6ba7b810-9dad-11d1-80b4-00c04fd430c8",
		"test@test.com",
		time.Now(),
		"pixie",
	)
	return authcontext.NewContext(context.Background(), sCtx)
}

func TestServer_GetUsersInOrg(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	otherOrgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c7")
	userID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetUsersInOrg(orgID).
		Return([]*datastore.UserInfo{
			{
				OrgID:            orgID,
				Username:         "test@test.com",
				FirstName:        "first",
				LastName:         "last",
				Email:            "test@test.com",
				IsApproved:       true,
				ID:               userID,
				IdentityProvider: "github",
				AuthProviderID:   "github|asdfghjkl;",
			},
		}, nil)

	resp, err := s.GetUsersInOrg(ctx, &profilepb.GetUsersInOrgRequest{
		OrgID: utils.ProtoFromUUID(orgID),
	})
	require.NoError(t, err)
	assert.Equal(t, 1, len(resp.Users))
	assert.Equal(t, &profilepb.UserInfo{
		OrgID:            utils.ProtoFromUUID(orgID),
		ID:               utils.ProtoFromUUID(userID),
		Username:         "test@test.com",
		FirstName:        "first",
		LastName:         "last",
		Email:            "test@test.com",
		IsApproved:       true,
		IdentityProvider: "github",
		AuthProviderID:   "github|asdfghjkl;",
	}, resp.Users[0])

	_, err = s.GetUsersInOrg(ctx, &profilepb.GetUsersInOrgRequest{
		OrgID: utils.ProtoFromUUID(otherOrgID),
	})
	assert.NotNil(t, err)
}

func TestServer_AddOrgIDEConfig(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	osds.EXPECT().
		AddIDEConfig(orgID, &datastore.IDEConfig{Name: "test", Path: "test://path/{{symbol}}"}).
		Return(nil)

	resp, err := s.AddOrgIDEConfig(ctx, &profilepb.AddOrgIDEConfigRequest{
		OrgID: utils.ProtoFromUUID(orgID),
		Config: &profilepb.IDEConfig{
			IDEName: "test",
			Path:    "test://path/{{symbol}}",
		},
	})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.IDEConfig{
		IDEName: "test",
		Path:    "test://path/{{symbol}}",
	}, resp.Config)
}

func TestServer_DeleteOrgIDEConfig(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	osds.EXPECT().
		DeleteIDEConfig(orgID, "test").
		Return(nil)

	resp, err := s.DeleteOrgIDEConfig(ctx, &profilepb.DeleteOrgIDEConfigRequest{
		OrgID:   utils.ProtoFromUUID(orgID),
		IDEName: "test",
	})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.DeleteOrgIDEConfigResponse{}, resp)
}

func TestServer_GetOrgIDEConfigs_Single(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	osds.EXPECT().
		GetIDEConfig(orgID, "test").
		Return(&datastore.IDEConfig{
			Name: "test",
			Path: "test://{{symbol}}",
		}, nil)

	resp, err := s.GetOrgIDEConfigs(ctx, &profilepb.GetOrgIDEConfigsRequest{
		OrgID:   utils.ProtoFromUUID(orgID),
		IDEName: "test",
	})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.GetOrgIDEConfigsResponse{
		Configs: []*profilepb.IDEConfig{
			&profilepb.IDEConfig{
				IDEName: "test",
				Path:    "test://{{symbol}}",
			},
		},
	}, resp)
}

func TestServer_GetOrgIDEConfigs_Multi(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controller.NewMockUserDatastore(ctrl)
	ods := mock_controller.NewMockOrgDatastore(ctrl)
	usds := mock_controller.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controller.NewMockOrgSettingsDatastore(ctrl)

	s := controller.NewServer(nil, uds, usds, ods, osds)

	osds.EXPECT().
		GetIDEConfigs(orgID).
		Return([]*datastore.IDEConfig{
			&datastore.IDEConfig{
				Name: "test",
				Path: "test://{{symbol}}",
			},
			&datastore.IDEConfig{
				Name: "test2",
				Path: "test2://{{symbol2}}",
			},
		}, nil)

	resp, err := s.GetOrgIDEConfigs(ctx, &profilepb.GetOrgIDEConfigsRequest{
		OrgID: utils.ProtoFromUUID(orgID),
	})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.GetOrgIDEConfigsResponse{
		Configs: []*profilepb.IDEConfig{
			&profilepb.IDEConfig{
				IDEName: "test",
				Path:    "test://{{symbol}}",
			},
			&profilepb.IDEConfig{
				IDEName: "test2",
				Path:    "test2://{{symbol2}}",
			},
		},
	}, resp)
}
