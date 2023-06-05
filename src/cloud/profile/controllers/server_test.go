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
	"fmt"
	"testing"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/lestrrat-go/jwx/jwa"
	"github.com/lestrrat-go/jwx/jwt"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/controllers"
	mock_controllers "px.dev/pixie/src/cloud/profile/controllers/mock"
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

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
			name:      "no orgid",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				FirstName:        "foo",
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
			name:      "nil orgid",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(uuid.Nil),
				FirstName:        "foo",
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
			name:      "empty first name is ok",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
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
			name:      "allowed email",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
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
			name:      "enable approvals properly sets users info",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:            utils.ProtoFromUUID(testOrgUUID),
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
			s := controllers.NewServer(nil, uds, usds, ods, osds)
			if utils.UUIDFromProtoOrNil(tc.userInfo.OrgID) != uuid.Nil {
				ods.EXPECT().
					GetOrg(testOrgUUID).
					Return(&datastore.OrgInfo{
						EnableApprovals: tc.enableApprovals,
					}, nil)
			}
			if tc.makesCall {
				req := &datastore.UserInfo{
					FirstName:        tc.userInfo.FirstName,
					LastName:         tc.userInfo.LastName,
					Email:            tc.userInfo.Email,
					IsApproved:       !tc.enableApprovals,
					IdentityProvider: tc.userInfo.IdentityProvider,
					AuthProviderID:   tc.userInfo.AuthProviderID,
				}
				if utils.UUIDFromProtoOrNil(tc.userInfo.OrgID) != uuid.Nil {
					req.OrgID = &testOrgUUID
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

func TestServer_CreateOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	testOrgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, nil, nil, ods, osds)
	domain := "pixielabs.ai"
	req := &datastore.OrgInfo{
		OrgName:    "pixie",
		DomainName: &domain,
	}
	ods.EXPECT().
		CreateOrg(req).
		Return(testOrgUUID, nil)
	resp, err := s.CreateOrg(context.Background(), &profilepb.CreateOrgRequest{
		OrgName:    "pixie",
		DomainName: &types.StringValue{Value: "pixielabs.ai"},
	})

	assert.Nil(t, err)
	c := status.Code(err)
	assert.Equal(t, codes.OK, c)
	assert.Equal(t, utils.ProtoFromUUID(testOrgUUID), resp)
}

func TestServer_CreateOrg_NilDomainName(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	testOrgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, nil, nil, ods, osds)
	req := &datastore.OrgInfo{
		OrgName: "pixie",
	}
	ods.EXPECT().
		CreateOrg(req).
		Return(testOrgUUID, nil)
	resp, err := s.CreateOrg(context.Background(), &profilepb.CreateOrgRequest{
		OrgName:    "pixie",
		DomainName: nil,
	})

	assert.Nil(t, err)
	c := status.Code(err)
	assert.Equal(t, codes.OK, c)
	assert.Equal(t, utils.ProtoFromUUID(testOrgUUID), resp)
}

func TestServer_GetUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.UserInfo{
		ID:             userUUID,
		OrgID:          &orgUUID,
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
	assert.Equal(t, resp.FirstName, "foo")
	assert.Equal(t, resp.LastName, "bar")
	assert.Equal(t, resp.Email, "foo@bar.com")
	assert.Equal(t, resp.AuthProviderID, "github|asdfghjkl;")
}

func TestServer_GetUser_MissingUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.UserInfo{
		ID:               userUUID,
		OrgID:            &orgUUID,
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	userUUID := uuid.Must(uuid.NewV4())
	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.UserInfo{
		ID:               userUUID,
		OrgID:            &orgUUID,
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

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
				ProjectName: controllers.DefaultProjectName,
				OrgID:       utils.ProtoFromUUID(testOrgUUID),
			}
			resp := &projectmanagerpb.RegisterProjectResponse{
				ProjectRegistered: true,
			}
			pm.EXPECT().RegisterProject(gomock.Any(), req).Return(resp, nil)

			env := profileenv.New(pm)

			s := controllers.NewServer(env, uds, usds, ods, osds)
			exUserInfo := &datastore.UserInfo{
				FirstName:        tc.req.User.FirstName,
				LastName:         tc.req.User.LastName,
				Email:            tc.req.User.Email,
				IsApproved:       true,
				IdentityProvider: tc.req.User.IdentityProvider,
				AuthProviderID:   tc.req.User.AuthProviderID,
			}
			exOrg := &datastore.OrgInfo{
				DomainName: &tc.req.Org.DomainName,
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

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
					FirstName:        "foo",
					LastName:         "bar",
					Email:            "",
					IdentityProvider: "github",
				},
			},
		},
	}

	for _, tc := range createOrgUserTest {
		t.Run(tc.name, func(t *testing.T) {
			pm := mock_projectmanager.NewMockProjectManagerServiceClient(ctrl)
			env := profileenv.New(pm)
			s := controllers.NewServer(env, uds, usds, ods, osds)
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	testOrgUUID := uuid.Must(uuid.NewV4())
	testUUID := uuid.Must(uuid.NewV4())

	pm := mock_projectmanager.NewMockProjectManagerServiceClient(ctrl)
	projectReq := &projectmanagerpb.RegisterProjectRequest{
		ProjectName: controllers.DefaultProjectName,
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
			FirstName:        "foo",
			LastName:         "bar",
			Email:            "foo@bar.com",
			IdentityProvider: "github",
		},
	}

	s := controllers.NewServer(env, uds, usds, ods, osds)
	exUserInfo := &datastore.UserInfo{
		FirstName:        req.User.FirstName,
		LastName:         req.User.LastName,
		Email:            req.User.Email,
		IsApproved:       true,
		IdentityProvider: "github",
	}
	exOrg := &datastore.OrgInfo{
		DomainName: &req.Org.DomainName,
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	orgDomain := "my-org.com"
	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: &orgDomain,
		OrgName:    "my-org",
	}

	ods.EXPECT().
		GetOrg(orgUUID).
		Return(mockReply, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(orgUUID))

	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(orgUUID), resp.ID)
	assert.Equal(t, "my-org.com", resp.DomainName.GetValue())
	assert.Equal(t, "my-org", resp.OrgName)
}

func TestServer_GetOrg_NilDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: nil,
		OrgName:    "my-org",
	}

	ods.EXPECT().
		GetOrg(orgUUID).
		Return(mockReply, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(orgUUID))

	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(orgUUID), resp.ID)
	assert.Nil(t, resp.DomainName)
	assert.Equal(t, "my-org", resp.OrgName)
}

func TestServer_GetOrgs(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	org2UUID := uuid.Must(uuid.NewV4())

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	org1Domain := "my-org.com"
	org2Domain := "pixie.com"
	mockReply := []*datastore.OrgInfo{
		{
			ID:         orgUUID,
			DomainName: &org1Domain,
			OrgName:    "my-org",
		},
		{
			ID:         org2UUID,
			DomainName: &org2Domain,
			OrgName:    "pixie",
		},
	}

	ods.EXPECT().
		GetOrgs().
		Return(mockReply, nil)

	resp, err := s.GetOrgs(context.Background(), &profilepb.GetOrgsRequest{})

	require.NoError(t, err)
	assert.Equal(t, 2, len(resp.Orgs))
	assert.Equal(t, utils.ProtoFromUUID(orgUUID), resp.Orgs[0].ID)
	assert.Equal(t, "my-org.com", resp.Orgs[0].DomainName.GetValue())
	assert.Equal(t, "my-org", resp.Orgs[0].OrgName)
	assert.Equal(t, utils.ProtoFromUUID(org2UUID), resp.Orgs[1].ID)
	assert.Equal(t, "pixie.com", resp.Orgs[1].DomainName.GetValue())
	assert.Equal(t, "pixie", resp.Orgs[1].OrgName)
}

func TestServer_GetOrg_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetOrg(orgUUID).
		Return(nil, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(orgUUID))
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestServer_GetOrgByName(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	orgDomain := "my-org.com"
	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: &orgDomain,
		OrgName:    "my-org",
	}

	ods.EXPECT().
		GetOrgByName("my-org").
		Return(mockReply, nil)

	resp, err := s.GetOrgByName(
		context.Background(),
		&profilepb.GetOrgByNameRequest{Name: "my-org"})

	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(orgUUID), resp.ID)
	assert.Equal(t, "my-org.com", resp.DomainName.GetValue())
	assert.Equal(t, "my-org", resp.OrgName)
}

func TestServer_GetOrgByName_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetOrgByName("my-org").
		Return(nil, datastore.ErrOrgNotFound)

	resp, err := s.GetOrgByName(
		context.Background(),
		&profilepb.GetOrgByNameRequest{Name: "my-org"})

	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestServer_GetOrgByDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgUUID := uuid.Must(uuid.NewV4())
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	orgDomain := "my-org.com"
	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: &orgDomain,
		OrgName:    "my-org",
	}

	ods.EXPECT().
		GetOrgByDomain("my-org.com").
		Return(mockReply, nil)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profilepb.GetOrgByDomainRequest{DomainName: "my-org.com"})

	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(orgUUID), resp.ID)
	assert.Equal(t, "my-org.com", resp.DomainName.GetValue())
	assert.Equal(t, "my-org", resp.OrgName)
}

func TestServer_GetOrgByDomain_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetOrgByDomain("my-org.com").
		Return(nil, datastore.ErrOrgNotFound)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profilepb.GetOrgByDomainRequest{DomainName: "my-org.com"})

	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestServer_DeleteUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	orgID := uuid.Must(uuid.NewV4())

	userInfo := &datastore.UserInfo{
		ID:         userID,
		FirstName:  "first",
		LastName:   "last",
		IsApproved: false,
		OrgID:      &orgID,
	}
	uds.EXPECT().GetUser(userID).Return(userInfo, nil)
	ods.EXPECT().GetUsersInOrg(orgID).Return([]*datastore.UserInfo{userInfo}, nil)
	ods.EXPECT().DeleteOrgAndUsers(orgID).Return(nil)

	resp, err := s.DeleteUser(context.Background(), &profilepb.DeleteUserRequest{ID: utils.ProtoFromUUID(userID)})
	require.NoError(t, err)
	require.NotNil(t, resp)
}

func TestServer_DeleteUser_MultiUserOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	orgID := uuid.Must(uuid.NewV4())

	userInfo := &datastore.UserInfo{
		ID:         userID,
		FirstName:  "first",
		LastName:   "last",
		IsApproved: false,
		OrgID:      &orgID,
	}
	userInfo2 := &datastore.UserInfo{
		ID:         uuid.Must(uuid.NewV4()),
		FirstName:  "first2",
		LastName:   "last2",
		IsApproved: false,
		OrgID:      &orgID,
	}
	uds.EXPECT().GetUser(userID).Return(userInfo, nil)
	ods.EXPECT().GetUsersInOrg(orgID).Return([]*datastore.UserInfo{userInfo, userInfo2}, nil)
	uds.EXPECT().DeleteUser(userID).Return(nil)

	resp, err := s.DeleteUser(context.Background(), &profilepb.DeleteUserRequest{ID: utils.ProtoFromUUID(userID)})
	require.NoError(t, err)
	require.NotNil(t, resp)
}

func TestServer_DeleteOrgAndUsers(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	orgUUID := uuid.Must(uuid.NewV4())

	orgDomain := "my-org.com"
	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: &orgDomain,
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	updateUserTest := []struct {
		name              string
		userID            string
		userOrg           string
		updatedProfilePic string
		updatedIsApproved bool
		updatedOrg        string
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
		{
			name:       "user should be able to update org if org isn't already set",
			userID:     "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:    "00000000-0000-0000-0000-000000000000",
			updatedOrg: "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
		},
		{
			name:       "user should be able to update org to leave org",
			userID:     "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:    "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedOrg: "00000000-0000-0000-0000-000000000000",
		},
		{
			name:       "user should be able to change orgs (unused but allowed)",
			userID:     "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			userOrg:    "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
			updatedOrg: "6ba7b810-9dad-11d1-80b4-00c04fd430c9",
		},
	}

	for _, tc := range updateUserTest {
		t.Run(tc.name, func(t *testing.T) {
			ctx := CreateTestContext()
			s := controllers.NewServer(nil, uds, usds, ods, osds)
			userID := uuid.FromStringOrNil(tc.userID)
			orgID := uuid.FromStringOrNil(tc.userOrg)

			// This is the original user's info.
			profilePicture := "something"
			originalUserInfo := &datastore.UserInfo{
				ID:             userID,
				FirstName:      "first",
				LastName:       "last",
				ProfilePicture: &profilePicture,
				IsApproved:     false,
				OrgID:          &orgID,
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
				OrgID:          &orgID,
			}

			if tc.updatedProfilePic != profilePicture {
				req.DisplayPicture = &types.StringValue{Value: tc.updatedProfilePic}
				mockUpdateReq.ProfilePicture = &tc.updatedProfilePic
			}

			if tc.updatedIsApproved != originalUserInfo.IsApproved {
				req.IsApproved = &types.BoolValue{Value: tc.updatedIsApproved}
				mockUpdateReq.IsApproved = tc.updatedIsApproved
			}

			if tc.updatedOrg != "" {
				req.OrgID = utils.ProtoFromUUIDStrOrNil(tc.updatedOrg)
				newOrgID := uuid.FromStringOrNil(tc.updatedOrg)
				mockUpdateReq.OrgID = &newOrgID
				if newOrgID == uuid.Nil {
					mockUpdateReq.OrgID = nil
				}
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
			if tc.updatedOrg != "" {
				assert.Equal(t, utils.ProtoToUUIDStr(resp.OrgID), tc.updatedOrg)
			}
		})
	}
}

func TestServer_UpdateOrg_EnableApprovals(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

func TestServer_UpdateOrg_DomainName(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:         orgID,
		DomainName: nil,
	}

	domainName := "asdf.com"
	mockUpdateReq := &datastore.OrgInfo{
		ID:         orgID,
		DomainName: &domainName,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	ods.EXPECT().
		UpdateOrg(gomock.Any()).
		Do(func(arg *datastore.OrgInfo) {
			assert.Equal(t, mockUpdateReq.ID, arg.ID)
			assert.Equal(t, mockUpdateReq.EnableApprovals, arg.EnableApprovals)
			assert.NotNil(t, arg.DomainName)
			assert.Equal(t, *mockUpdateReq.DomainName, *arg.DomainName)
		}).
		Return(nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID:         utils.ProtoFromUUID(orgID),
			DomainName: &types.StringValue{Value: "asdf.com"},
		})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgID))
	assert.Equal(t, resp.DomainName.GetValue(), "asdf.com")
}

func TestServer_UpdateOrg_NilToEmptyDomainName(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:         orgID,
		DomainName: nil,
	}

	domainName := ""
	mockUpdateReq := &datastore.OrgInfo{
		ID:         orgID,
		DomainName: &domainName,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	ods.EXPECT().
		UpdateOrg(gomock.Any()).
		Do(func(arg *datastore.OrgInfo) {
			assert.Equal(t, mockUpdateReq.ID, arg.ID)
			assert.Equal(t, mockUpdateReq.EnableApprovals, arg.EnableApprovals)
			assert.NotNil(t, arg.DomainName)
			assert.Equal(t, *mockUpdateReq.DomainName, *arg.DomainName)
		}).
		Return(nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID:         utils.ProtoFromUUID(orgID),
			DomainName: &types.StringValue{Value: ""},
		})

	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(orgID), resp.ID)
	assert.NotNil(t, resp.DomainName)
	assert.Equal(t, "", resp.DomainName.GetValue())
}

func TestServer_UpdateOrg_DomainName_EnableApprovals(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	s := controllers.NewServer(nil, uds, usds, ods, osds)

	mockReply := &datastore.OrgInfo{
		ID:              orgID,
		DomainName:      nil,
		EnableApprovals: false,
	}

	domainName := "asdf.com"
	mockUpdateReq := &datastore.OrgInfo{
		ID:              orgID,
		DomainName:      &domainName,
		EnableApprovals: true,
	}

	ods.EXPECT().
		GetOrg(orgID).
		Return(mockReply, nil)

	ods.EXPECT().
		UpdateOrg(gomock.Any()).
		Do(func(arg *datastore.OrgInfo) {
			assert.Equal(t, mockUpdateReq.ID, arg.ID)
			assert.Equal(t, mockUpdateReq.EnableApprovals, arg.EnableApprovals)
			assert.NotNil(t, arg.DomainName)
			assert.Equal(t, *mockUpdateReq.DomainName, *arg.DomainName)
		}).
		Return(nil)

	resp, err := s.UpdateOrg(
		CreateTestContext(),
		&profilepb.UpdateOrgRequest{
			ID:              utils.ProtoFromUUID(orgID),
			DomainName:      &types.StringValue{Value: "asdf.com"},
			EnableApprovals: &types.BoolValue{Value: true},
		})

	require.NoError(t, err)
	assert.Equal(t, utils.ProtoFromUUID(orgID), resp.ID)
	assert.Equal(t, "asdf.com", resp.DomainName.GetValue())
}

func TestServer_UpdateOrg_RequestBlockedForUserOutsideOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	userID := uuid.Must(uuid.NewV4())
	tourSeen := true
	usds.EXPECT().
		GetUserAttributes(userID).
		Return(&datastore.UserAttributes{TourSeen: &tourSeen}, nil)

	resp, err := s.GetUserAttributes(context.Background(), &profilepb.GetUserAttributesRequest{
		ID: utils.ProtoFromUUID(userID),
	})
	require.NoError(t, err)
	assert.True(t, resp.TourSeen)
}

func TestServer_SetUserAttributes(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		GetUsersInOrg(orgID).
		Return([]*datastore.UserInfo{
			{
				OrgID:            &orgID,
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

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
			{
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

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	osds.EXPECT().
		GetIDEConfigs(orgID).
		Return([]*datastore.IDEConfig{
			{
				Name: "test",
				Path: "test://{{symbol}}",
			},
			{
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
			{
				IDEName: "test",
				Path:    "test://{{symbol}}",
			},
			{
				IDEName: "test2",
				Path:    "test2://{{symbol2}}",
			},
		},
	}, resp)
}

func TestServer_CreateInviteToken(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	inviteSigningKey := "secret_jwt_key"
	ods.EXPECT().
		GetInviteSigningKey(orgID).
		Return(inviteSigningKey, nil)

	resp, err := s.CreateInviteToken(ctx, &profilepb.CreateInviteTokenRequest{
		OrgID: utils.ProtoFromUUID(orgID),
	})
	require.NoError(t, err)

	token, err := jwt.Parse([]byte(resp.SignedClaims), jwt.WithVerify(jwa.HS256, []byte(inviteSigningKey)), jwt.WithValidate(false))
	require.NoError(t, err)

	exp := time.Until(token.Expiration())

	assert.Equal(t, token.Subject(), orgID.String())
	assert.Greater(t, exp.Hours(), 0.0)
	assert.Less(t, exp.Hours(), 7.0*24.0)
}

func TestServer_CreateInviteToken_NoSigningKey(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	inviteSigningKey := "secret_jwt_key"
	ods.EXPECT().
		GetInviteSigningKey(orgID).
		Return("", nil)

	ods.EXPECT().
		CreateInviteSigningKey(orgID).
		Return(inviteSigningKey, nil)

	resp, err := s.CreateInviteToken(ctx, &profilepb.CreateInviteTokenRequest{
		OrgID: utils.ProtoFromUUID(orgID),
	})
	require.NoError(t, err)

	token, err := jwt.Parse([]byte(resp.SignedClaims), jwt.WithVerify(jwa.HS256, []byte(inviteSigningKey)), jwt.WithValidate(false))
	require.NoError(t, err)

	exp := time.Until(token.Expiration())

	require.NoError(t, err)
	assert.Equal(t, token.Subject(), orgID.String())
	assert.Greater(t, exp.Hours(), 0.0)
	assert.Less(t, exp.Hours(), 7.0*24.0)
}

func TestServer_CreateInviteToken_BadOrg(t *testing.T) {
	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	_, err := s.CreateInviteToken(ctx, &profilepb.CreateInviteTokenRequest{
		OrgID: utils.ProtoFromUUID(uuid.Nil),
	})
	require.Error(t, err)
}

func TestServer_RevokeInvites(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	ods.EXPECT().
		CreateInviteSigningKey(orgID)

	_, err := s.RevokeAllInviteTokens(ctx, utils.ProtoFromUUID(orgID))
	require.NoError(t, err)
}

func TestServer_RevokeInvites_BadOrg(t *testing.T) {
	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	_, err := s.RevokeAllInviteTokens(ctx, utils.ProtoFromUUID(uuid.Nil))
	require.Error(t, err)
}

func TestServer_VerifyInvites_Good(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	inviteSigningKey := "secret_jwt_key"
	builder := jwt.NewBuilder().
		Subject(orgID.String()).
		Expiration(time.Now().Add(7 * 24 * time.Hour))
	token, err := builder.Build()
	require.NoError(t, err)

	signedClaims, err := svcutils.SignToken(token, inviteSigningKey)
	require.NoError(t, err)

	ods.EXPECT().
		GetInviteSigningKey(orgID).
		Return(inviteSigningKey, nil)

	resp, err := s.VerifyInviteToken(ctx, &profilepb.InviteToken{SignedClaims: string(signedClaims)})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.VerifyInviteTokenResponse{
		Valid: true,
		OrgID: utils.ProtoFromUUID(orgID),
	}, resp)
}

func TestServer_VerifyInvites_Expired(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	inviteSigningKey := "secret_jwt_key"
	builder := jwt.NewBuilder().
		Subject(orgID.String()).
		Expiration(time.Now().Add(-1 * time.Minute))
	token, err := builder.Build()
	require.NoError(t, err)

	signedClaims, err := svcutils.SignToken(token, inviteSigningKey)
	require.NoError(t, err)

	resp, err := s.VerifyInviteToken(ctx, &profilepb.InviteToken{SignedClaims: string(signedClaims)})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.VerifyInviteTokenResponse{Valid: false}, resp)
}

func TestServer_VerifyInvites_BadSigningKey(t *testing.T) {
	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	inviteSigningKey := "secret_jwt_key"
	builder := jwt.NewBuilder().
		Subject(orgID.String()).
		Expiration(time.Now().Add(7 * 24 * time.Hour))
	token, err := builder.Build()
	require.NoError(t, err)

	signedClaims, err := svcutils.SignToken(token, "wrong_key")
	require.NoError(t, err)

	ods.EXPECT().
		GetInviteSigningKey(orgID).
		Return(inviteSigningKey, nil)

	resp, err := s.VerifyInviteToken(ctx, &profilepb.InviteToken{SignedClaims: string(signedClaims)})
	require.NoError(t, err)
	assert.Equal(t, &profilepb.VerifyInviteTokenResponse{Valid: false}, resp)
}

func TestServer_VerifyInvites_BadOrg(t *testing.T) {
	ctx := CreateTestContext()
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	uds := mock_controllers.NewMockUserDatastore(ctrl)
	ods := mock_controllers.NewMockOrgDatastore(ctrl)
	usds := mock_controllers.NewMockUserSettingsDatastore(ctrl)
	osds := mock_controllers.NewMockOrgSettingsDatastore(ctrl)

	s := controllers.NewServer(nil, uds, usds, ods, osds)

	inviteSigningKey := "secret_jwt_key"
	builder := jwt.NewBuilder().
		Subject(uuid.Nil.String()).
		Expiration(time.Now().Add(7 * 24 * time.Hour))
	token, err := builder.Build()
	require.NoError(t, err)

	signedClaims, err := svcutils.SignToken(token, inviteSigningKey)
	require.NoError(t, err)

	_, err = s.VerifyInviteToken(ctx, &profilepb.InviteToken{SignedClaims: string(signedClaims)})
	require.Error(t, err)
}
