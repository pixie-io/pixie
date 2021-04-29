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
	"px.dev/pixie/src/cloud/shared/idprovider/testutils"
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
	}{
		{
			name:      "valid request",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@bar.com",
			},
			expectErr:  false,
			expectCode: codes.OK,
			respID:     utils.ProtoFromUUID(testUUID),
		},
		{
			name:      "invalid orgid",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     &uuidpb.UUID{},
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@bar.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "invalid username",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@bar.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "empty first name is ok",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "foobar",
				FirstName: "",
				LastName:  "bar",
				Email:     "foo@bar.com",
			},
			expectErr:  false,
			expectCode: codes.OK,
			respID:     utils.ProtoFromUUID(testUUID),
		},
		{
			name:      "empty email",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "banned email",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@blocklist.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "allowed email",
			makesCall: true,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@gmail.com",
			},
			expectErr:  false,
			expectCode: codes.OK,
			respID:     utils.ProtoFromUUID(testUUID),
		},
		{
			name:      "invalid email",
			makesCall: false,
			userInfo: &profilepb.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
	}

	for _, tc := range createUsertests {
		t.Run(tc.name, func(t *testing.T) {
			s := controller.NewServer(nil, d, nil, nil)
			if tc.makesCall {
				req := &datastore.UserInfo{
					OrgID:     testOrgUUID,
					Username:  tc.userInfo.Username,
					FirstName: tc.userInfo.FirstName,
					LastName:  tc.userInfo.LastName,
					Email:     tc.userInfo.Email,
				}
				d.EXPECT().
					CreateUser(req).
					Return(testUUID, nil)
			}
			resp, err := s.CreateUser(context.Background(), tc.userInfo)

			if tc.expectErr {
				assert.NotNil(t, err)
				c := status.Code(err)
				assert.Equal(t, c, tc.expectCode)
				return
			}

			assert.Equal(t, resp, tc.respID)
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
		ID:        userUUID,
		OrgID:     orgUUID,
		Username:  "foobar",
		FirstName: "foo",
		LastName:  "bar",
		Email:     "foo@bar.com",
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
		ID:        userUUID,
		OrgID:     orgUUID,
		Username:  "foobar",
		FirstName: "foo",
		LastName:  "bar",
		Email:     "foo@bar.com",
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
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
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
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "",
					Email:     "foo@gmail.com",
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
				Username:  tc.req.User.Username,
				FirstName: tc.req.User.FirstName,
				LastName:  tc.req.User.LastName,
				Email:     tc.req.User.Email,
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
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
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
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
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
					Username:  "",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
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
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "",
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
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@blocklist.com",
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
			Username:  "foobar",
			FirstName: "foo",
			LastName:  "bar",
			Email:     "foo@bar.com",
		},
	}

	s := controller.NewServer(env, d, nil, nil)
	exUserInfo := &datastore.UserInfo{
		Username:  req.User.Username,
		FirstName: req.User.FirstName,
		LastName:  req.User.LastName,
		Email:     req.User.Email,
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

	userID := uuid.Must(uuid.NewV4())
	s := controller.NewServer(nil, d, nil, nil)

	profilePicture := "something"
	newProfilePicture := "new"
	mockReply := &datastore.UserInfo{
		ID:             userID,
		FirstName:      "first",
		LastName:       "last",
		ProfilePicture: &profilePicture,
	}

	mockUpdateReq := &datastore.UserInfo{
		ID:             userID,
		FirstName:      "first",
		LastName:       "last",
		ProfilePicture: &newProfilePicture,
	}

	d.EXPECT().
		GetUser(userID).
		Return(mockReply, nil)

	d.EXPECT().
		UpdateUser(mockUpdateReq).
		Return(nil)

	resp, err := s.UpdateUser(
		context.Background(),
		&profilepb.UpdateUserRequest{ID: utils.ProtoFromUUID(userID), ProfilePicture: "new"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(userID))
	assert.Equal(t, resp.ProfilePicture, "new")
}

func TestServer_GetUserSettings(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d, nil)

	userID := uuid.Must(uuid.NewV4())
	d.EXPECT().
		GetUserSettings(userID, []string{"test", "another_key"}).
		Return([]string{"a", "b"}, nil)

	resp, err := s.GetUserSettings(context.Background(), &profilepb.GetUserSettingsRequest{
		ID:   utils.ProtoFromUUID(userID),
		Keys: []string{"test", "another_key"},
	})
	require.NoError(t, err)
	assert.Equal(t, []string{"test", "another_key"}, resp.Keys)
	assert.Equal(t, []string{"a", "b"}, resp.Values)
}

func TestServer_UpdateUserSettings(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d, nil)

	userID := uuid.Must(uuid.NewV4())

	tests := []struct {
		name string

		keys   []string
		values []string

		expectCall   bool
		expectErr    bool
		expectedCode codes.Code
	}{
		{
			name:       "valid",
			keys:       []string{"test1", "test2"},
			values:     []string{"val1", "val2"},
			expectCall: true,
			expectErr:  false,
		},
		{
			name:         "mismatched length",
			keys:         []string{"test1", "test2"},
			values:       []string{"val1"},
			expectCall:   false,
			expectErr:    true,
			expectedCode: codes.InvalidArgument,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if tc.expectCall {
				d.EXPECT().
					UpdateUserSettings(userID, tc.keys, tc.values).
					Return(nil)
			}

			resp, err := s.UpdateUserSettings(context.Background(), &profilepb.UpdateUserSettingsRequest{
				ID:     utils.ProtoFromUUID(userID),
				Keys:   tc.keys,
				Values: tc.values,
			})
			if tc.expectErr {
				assert.NotNil(t, err)
				assert.Equal(t, tc.expectedCode, status.Code(err))
			} else {
				require.NoError(t, err)
				assert.NotNil(t, resp)
				assert.Equal(t, true, resp.OK)
			}
		})
	}
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

func TestServer_InviteUser(t *testing.T) {
	userID := uuid.Must(uuid.NewV4())

	orgID := uuid.FromStringOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")

	ctx := CreateTestContext()
	inviteUserTests := []struct {
		name string
		// MustCreateUser is passed as part of the request.
		mustCreate bool
		// Whether the user already exists.
		doesUserExist bool
		err           error
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
	}
	for _, tc := range inviteUserTests {
		t.Run(tc.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)
			defer ctrl.Finish()

			d := mock_controller.NewMockDatastore(ctrl)

			kratos, err := testutils.NewKratosServer()
			require.NoError(t, err)

			defer kratos.CleanUp()

			client, closeSrv, err := kratos.Serve()
			require.NoError(t, err)
			defer closeSrv()
			assert.NotNil(t, client)
			s := controller.NewServer(nil, d, nil, client)

			req := &profilepb.InviteUserRequest{
				MustCreateUser: tc.mustCreate,
				OrgID:          utils.ProtoFromUUID(orgID),
				Email:          "bobloblaw@lawblog.com",
				FirstName:      "Bob",
				LastName:       "Loblaw",
			}

			userInfo := &datastore.UserInfo{
				OrgID:     orgID,
				Username:  req.Email,
				FirstName: req.FirstName,
				LastName:  req.LastName,
				Email:     req.Email,
			}

			if !tc.doesUserExist {
				d.EXPECT().
					GetUserByEmail("bobloblaw@lawblog.com").
					Return(nil, datastore.ErrUserNotFound)
				// We always create a user if one does not exist.
				d.EXPECT().
					CreateUser(userInfo).
					Return(userID, nil)
			} else {
				d.EXPECT().
					GetUserByEmail("bobloblaw@lawblog.com").
					Return(userInfo, nil)
			}

			resp, err := s.InviteUser(ctx, req)

			if tc.err == nil {
				require.NoError(t, tc.err)
				assert.Equal(t, resp.Email, req.Email)
				assert.Regexp(t, "self-service/recovery/methods", resp.InviteLink)
			} else {
				assert.Equal(t, err, tc.err)
			}
		})
	}
}
