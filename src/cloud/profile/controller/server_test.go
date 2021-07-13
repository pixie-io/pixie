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
	"errors"
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
	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	mock_idmanager "px.dev/pixie/src/cloud/profile/controller/idmanager/mock"
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

	d := mock_controller.NewMockDatastore(ctrl)

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
			s := controller.NewServer(nil, d, nil, nil)
			if utils.UUIDFromProtoOrNil(tc.userInfo.OrgID) != uuid.Nil {
				d.EXPECT().
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
				d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

	mockReply := &datastore.UserInfo{
		ID:             userUUID,
		OrgID:          orgUUID,
		Username:       "foobar",
		FirstName:      "foo",
		LastName:       "bar",
		Email:          "foo@bar.com",
		AuthProviderID: "github|asdfghjkl;",
	}

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)
	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

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

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

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

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil, nil)

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

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

			s := controller.NewServer(env, d, nil, nil)
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
			d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

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
			s := controller.NewServer(env, d, nil, nil)
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

	d := mock_controller.NewMockDatastore(ctrl)

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

	s := controller.NewServer(env, d, nil, nil)
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
	d.EXPECT().
		CreateUserAndOrg(exOrg, exUserInfo).
		Return(testOrgUUID, testUUID, nil)

	d.EXPECT().
		DeleteOrgAndUsers(testOrgUUID).
		Return(nil)

	resp, err := s.CreateOrgAndUser(context.Background(), req)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_GetOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	}

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	org2UUID := uuid.Must(uuid.NewV4())

	s := controller.NewServer(nil, d, nil, nil)

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

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	}

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil, nil)

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil, nil)

	orgUUID := uuid.Must(uuid.NewV4())

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "my-org.com",
		OrgName:    "my-org",
	}
	d.EXPECT().GetOrg(orgUUID).Return(mockReply, nil)
	d.EXPECT().DeleteOrgAndUsers(orgUUID).Return(nil)

	err := s.DeleteOrgAndUsers(context.Background(), utils.ProtoFromUUID(orgUUID))
	require.NoError(t, err)
}

func TestServer_DeleteOrgAndUsers_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil, nil)

	orgUUID := uuid.Must(uuid.NewV4())
	d.EXPECT().
		GetOrg(orgUUID).
		Return(nil, nil)

	err := s.DeleteOrgAndUsers(context.Background(), utils.ProtoFromUUID(orgUUID))
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_UpdateUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	updateUserTest := []struct {
		name              string
		userID            string
		userOrg           string
		updatedProfilePic string
		updatedIsApproved bool
		shouldReject      bool
	}{
		{
			name:              "user can update their own profile picture",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "new",
			updatedIsApproved: false,
			shouldReject:      false,
		},
		{
			name:              "admin can update another's profile picture",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "new",
			updatedIsApproved: false,
			shouldReject:      false,
		},
		{
			name:              "user cannot update their own isApproved",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "new",
			updatedIsApproved: true,
			shouldReject:      true,
		},
		{
			name:              "user cannot update user from another org",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
			userOrg:           "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "new",
			updatedIsApproved: false,
			shouldReject:      true,
		},
		{
			name:              "user cannot update user from another org",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
			userOrg:           "7ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "something",
			updatedIsApproved: true,
			shouldReject:      true,
		},
		{
			name:              "user should approve other user in org",
			userID:            "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:           "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedProfilePic: "something",
			updatedIsApproved: true,
			shouldReject:      false,
		},
	}

	for _, tc := range updateUserTest {
		t.Run(tc.name, func(t *testing.T) {
			ctx := CreateTestContext()
			s := controller.NewServer(nil, d, nil, nil)
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

			d.EXPECT().
				GetUser(userID).
				Return(originalUserInfo, nil)

			if !tc.shouldReject {
				d.EXPECT().
					UpdateUser(mockUpdateReq).
					Return(nil)
			}

			resp, err := s.UpdateUser(ctx, req)

			if !tc.shouldReject {
				require.NoError(t, err)
				assert.Equal(t, resp.ID, utils.ProtoFromUUID(userID))
				assert.Equal(t, resp.ProfilePicture, tc.updatedProfilePic)
				assert.Equal(t, resp.IsApproved, tc.updatedIsApproved)
			} else {
				assert.NotNil(t, err)
			}
		})
	}
}

func TestServer_UpdateOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, d, nil, nil)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: false,
	}

	mockUpdateReq := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: true,
	}

	d.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, d, nil, nil)

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

	d.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	d.EXPECT().
		UpdateOrg(mockUpdateReq).
		Return(nil)

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, d, nil, nil)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: true,
	}

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controller.NewServer(nil, d, nil, nil)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		EnableApprovals: true,
	}

	d.EXPECT().
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

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil, nil)
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

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d, nil)

	userID := uuid.Must(uuid.NewV4())
	tourSeen := true
	d.EXPECT().
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

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d, nil)

	userID := uuid.Must(uuid.NewV4())
	tourSeen := true
	d.EXPECT().
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

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d, nil)

	userID := uuid.Must(uuid.NewV4())
	analyticsOptout := true
	d.EXPECT().
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

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d, nil)

	userID := uuid.Must(uuid.NewV4())
	analyticsOptout := true
	d.EXPECT().
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

func TestServerInviteUser(t *testing.T) {
	userID := uuid.Must(uuid.NewV4())

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	inviteUserTests := []struct {
		name string
		// MustCreateUser is passed as part of the request.
		mustCreate bool
		// Whether the user already exists.
		doesUserExist   bool
		err             error
		EnableApprovals bool
	}{
		{
			name:          "invite_existing_user",
			mustCreate:    false,
			doesUserExist: true,
			err:           nil,
		},
		{
			name:          "error_must_create_but_user_exists",
			mustCreate:    true,
			doesUserExist: true,
			err:           errors.New("cannot invite a user that already exists"),
		},
		{
			name:          "create_user_if_does_not_exist",
			mustCreate:    false,
			doesUserExist: false,
			err:           nil,
		},
		{
			name:          "must_create_user_if_does_not_exist",
			mustCreate:    true,
			doesUserExist: false,
			err:           nil,
		},
		{
			name:            "enable_user_if_invited",
			mustCreate:      true,
			doesUserExist:   false,
			err:             nil,
			EnableApprovals: true,
		},
	}
	for _, tc := range inviteUserTests {
		t.Run(tc.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			d := mock_controller.NewMockDatastore(ctrl)

			client := mock_idmanager.NewMockManager(ctrl)
			s := controller.NewServer(nil, d, nil, client)

			req := &profilepb.InviteUserRequest{
				MustCreateUser:   tc.mustCreate,
				OrgID:            utils.ProtoFromUUID(orgID),
				Email:            "bobloblaw@lawblog.com",
				FirstName:        "Bob",
				LastName:         "Loblaw",
				IdentityProvider: "kratos",
			}

			userInfo := &datastore.UserInfo{
				ID:               userID,
				OrgID:            orgID,
				Username:         req.Email,
				FirstName:        req.FirstName,
				LastName:         req.LastName,
				Email:            req.Email,
				IsApproved:       !tc.EnableApprovals,
				IdentityProvider: "kratos",
			}
			client.EXPECT().
				CreateIdentity(
					gomock.Any(),
					"bobloblaw@lawblog.com").
				Return(&idmanager.CreateIdentityResponse{
					IdentityProvider: "kratos",
					AuthProviderID:   "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
				}, nil)

			if !tc.doesUserExist {
				d.EXPECT().
					GetOrg(orgID).
					Return(&datastore.OrgInfo{
						EnableApprovals: tc.EnableApprovals,
					}, nil)
				d.EXPECT().
					GetUserByEmail("bobloblaw@lawblog.com").
					Return(nil, datastore.ErrUserNotFound)
				// We always create a user if one does not exist.
				d.EXPECT().
					CreateUser(&datastore.UserInfo{
						OrgID:            orgID,
						Username:         req.Email,
						FirstName:        req.FirstName,
						LastName:         req.LastName,
						Email:            req.Email,
						IsApproved:       !tc.EnableApprovals,
						IdentityProvider: "kratos",
						AuthProviderID:   "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
					}).
					Return(userID, nil)
				d.EXPECT().
					GetUser(userID).
					Return(userInfo, nil)
				d.EXPECT().
					UpdateUser(
						&datastore.UserInfo{
							ID:               userID,
							OrgID:            orgID,
							Username:         req.Email,
							FirstName:        req.FirstName,
							LastName:         req.LastName,
							Email:            req.Email,
							IsApproved:       true,
							IdentityProvider: "kratos",
						}).
					Return(nil)
			} else {
				d.EXPECT().
					GetUserByEmail("bobloblaw@lawblog.com").
					Return(userInfo, nil)
			}
			if !tc.doesUserExist || !tc.mustCreate {
				client.EXPECT().
					SetPLMetadata(
						"6ba7b810-9dad-11d1-80b4-00c04fd430c8",
						orgID.String(),
						userID.String(),
					).Return(nil)
				client.EXPECT().
					CreateInviteLinkForIdentity(
						gomock.Any(),
						&idmanager.CreateInviteLinkForIdentityRequest{AuthProviderID: "6ba7b810-9dad-11d1-80b4-00c04fd430c8"},
					).Return(&idmanager.CreateInviteLinkForIdentityResponse{
					InviteLink: "self-service/recovery/methods",
				}, nil)
			}

			resp, err := s.InviteUser(ctx, req)

			if tc.err == nil {
				require.NoError(t, err)
				assert.Equal(t, resp.Email, req.Email)
				assert.Regexp(t, "self-service/recovery/methods", resp.InviteLink)
			} else {
				assert.Equal(t, err, tc.err)
			}
		})
	}
}

func TestServer_GetUsersInOrg(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	otherOrgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c7")
	userID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c9")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)
	s := controller.NewServer(nil, d, nil, nil)

	d.EXPECT().
		GetUsersInOrg(orgID).
		Return([]*datastore.UserInfo{
			&datastore.UserInfo{
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
