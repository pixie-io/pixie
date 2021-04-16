package controller_test

import (
	"context"
	"fmt"
	"testing"

	"github.com/gofrs/uuid"
	"github.com/golang/mock/gomock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/public/uuidpb"
	"px.dev/pixie/src/cloud/profile/controller"
	mock_controller "px.dev/pixie/src/cloud/profile/controller/mock"
	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/profileenv"
	profile "px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	mock_projectmanager "px.dev/pixie/src/cloud/project_manager/projectmanagerpb/mock"
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
		userInfo  *profile.CreateUserRequest

		expectErr  bool
		expectCode codes.Code
		respID     *uuidpb.UUID
	}{
		{
			name:      "valid request",
			makesCall: true,
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			userInfo: &profile.CreateUserRequest{
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
			s := controller.NewServer(nil, d, nil)
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
	s := controller.NewServer(nil, d, nil)

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
	s := controller.NewServer(nil, d, nil)
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
	s := controller.NewServer(nil, d, nil)

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
		&profile.GetUserByEmailRequest{Email: "foo@bar.com"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(userUUID))
	assert.Equal(t, resp.Email, "foo@bar.com")
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUID(orgUUID))
}

func TestServer_GetUserByEmail_MissingEmail(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil)

	d.EXPECT().
		GetUserByEmail("foo@bar.com").
		Return(nil, datastore.ErrUserNotFound)

	resp, err := s.GetUserByEmail(
		context.Background(),
		&profile.GetUserByEmailRequest{Email: "foo@bar.com"})

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
		req  *profile.CreateOrgAndUserRequest
		resp *profile.CreateOrgAndUserResponse
	}{
		{
			name: "valid request",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
			resp: &profile.CreateOrgAndUserResponse{
				OrgID:  utils.ProtoFromUUID(testOrgUUID),
				UserID: utils.ProtoFromUUID(testUUID),
			},
		}, {
			name: "allowed email",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "",
					Email:     "foo@gmail.com",
				},
			},
			resp: &profile.CreateOrgAndUserResponse{
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

			s := controller.NewServer(env, d, nil)
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
		req  *profile.CreateOrgAndUserRequest
	}{
		{
			name: "invalid org name",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "",
					DomainName: "my-org.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
		},
		{
			name: "invalid domain name",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
		},
		{
			name: "invalid username",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
		},
		{
			name: "missing email",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "",
				},
			},
		},
		{
			name: "banned email",
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "my-org",
					DomainName: "my-org.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
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
			s := controller.NewServer(env, d, nil)
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

	req := &profile.CreateOrgAndUserRequest{
		Org: &profile.CreateOrgAndUserRequest_Org{
			OrgName:    "my-org",
			DomainName: "my-org.com",
		},
		User: &profile.CreateOrgAndUserRequest_User{
			Username:  "foobar",
			FirstName: "foo",
			LastName:  "bar",
			Email:     "foo@bar.com",
		},
	}

	s := controller.NewServer(env, d, nil)
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
	s := controller.NewServer(nil, d, nil)

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

	s := controller.NewServer(nil, d, nil)

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

	resp, err := s.GetOrgs(context.Background(), &profile.GetOrgsRequest{})

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
	s := controller.NewServer(nil, d, nil)

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
	s := controller.NewServer(nil, d, nil)

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
		&profile.GetOrgByDomainRequest{DomainName: "my-org.com"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(orgUUID))
	assert.Equal(t, resp.DomainName, "my-org.com")
	assert.Equal(t, resp.OrgName, "my-org")
}

func TestServer_GetOrgByDomain_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil)

	d.EXPECT().
		GetOrgByDomain("my-org.com").
		Return(nil, datastore.ErrOrgNotFound)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profile.GetOrgByDomainRequest{DomainName: "my-org.com"})

	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_DeleteOrgAndUsers(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d, nil)

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

	s := controller.NewServer(nil, d, nil)

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
	s := controller.NewServer(nil, d, nil)

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
		&profile.UpdateUserRequest{ID: utils.ProtoFromUUID(userID), ProfilePicture: "new"})

	require.NoError(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(userID))
	assert.Equal(t, resp.ProfilePicture, "new")
}

func TestServer_GetUserSettings(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockUserSettingsDatastore(ctrl)

	s := controller.NewServer(nil, nil, d)

	userID := uuid.Must(uuid.NewV4())
	d.EXPECT().
		GetUserSettings(userID, []string{"test", "another_key"}).
		Return([]string{"a", "b"}, nil)

	resp, err := s.GetUserSettings(context.Background(), &profile.GetUserSettingsRequest{
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

	s := controller.NewServer(nil, nil, d)

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

			resp, err := s.UpdateUserSettings(context.Background(), &profile.UpdateUserSettingsRequest{
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
