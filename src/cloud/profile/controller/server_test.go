package controller_test

import (
	"context"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/cloud/profile/controller"
	mock_controller "pixielabs.ai/pixielabs/src/cloud/profile/controller/mock"
	"pixielabs.ai/pixielabs/src/cloud/profile/datastore"
	profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/utils"
)

func TestServer_CreateUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	testOrgUUID := uuid.NewV4()
	testUUID := uuid.NewV4()
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
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@bar.com",
			},
			expectErr:  false,
			expectCode: codes.OK,
			respID:     utils.ProtoFromUUID(&testUUID),
		},
		{
			name:      "invalid orgid",
			makesCall: false,
			userInfo: &profile.CreateUserRequest{
				OrgID:     &uuidpb.UUID{Data: []byte("1234")},
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
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
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
			name:      "invalid first name",
			makesCall: false,
			userInfo: &profile.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
				Username:  "foobar",
				FirstName: "",
				LastName:  "bar",
				Email:     "foo@bar.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "invalid last name",
			makesCall: false,
			userInfo: &profile.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "",
				Email:     "foo@bar.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "empty email",
			makesCall: false,
			userInfo: &profile.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
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
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@blacklist.com",
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
			respID:     nil,
		},
		{
			name:      "allowed email",
			makesCall: true,
			userInfo: &profile.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
				Username:  "foobar",
				FirstName: "foo",
				LastName:  "bar",
				Email:     "foo@gmail.com",
			},
			expectErr:  false,
			expectCode: codes.OK,
			respID:     utils.ProtoFromUUID(&testUUID),
		},
		{
			name:      "invalid email",
			makesCall: false,
			userInfo: &profile.CreateUserRequest{
				OrgID:     utils.ProtoFromUUID(&testOrgUUID),
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
			s := controller.NewServer(nil, d)
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

	userUUID := uuid.NewV4()
	orgUUID := uuid.NewV4()
	s := controller.NewServer(nil, d)

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

	resp, err := s.GetUser(context.Background(), utils.ProtoFromUUID(&userUUID))

	require.Nil(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(&userUUID))
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUID(&orgUUID))
	assert.Equal(t, resp.Username, "foobar")
	assert.Equal(t, resp.FirstName, "foo")
	assert.Equal(t, resp.LastName, "bar")
	assert.Equal(t, resp.Email, "foo@bar.com")
}

func TestServer_GetUser_MissingUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	userUUID := uuid.NewV4()
	s := controller.NewServer(nil, d)
	d.EXPECT().
		GetUser(userUUID).
		Return(nil, nil)

	resp, err := s.GetUser(context.Background(), utils.ProtoFromUUID(&userUUID))
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_CreateOrgAndUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	testOrgUUID := uuid.NewV4()
	testUUID := uuid.NewV4()
	createOrgUserTest := []struct {
		name      string
		makesCall bool

		req        *profile.CreateOrgAndUserRequest
		expectErr  bool
		expectCode codes.Code
		resp       *profile.CreateOrgAndUserResponse
	}{
		{
			name:      "valid request",
			makesCall: true,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
			expectErr: false,
			resp: &profile.CreateOrgAndUserResponse{
				OrgID:  utils.ProtoFromUUID(&testOrgUUID),
				UserID: utils.ProtoFromUUID(&testUUID),
			},
		},
		{
			name:      "invalid org name",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "invalid domain name",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "invalid username",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "invalid first name",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "",
					LastName:  "bar",
					Email:     "foo@bar.com",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "invalid last name",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "",
					Email:     "foo@bar.com",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "missing email",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "banned email",
			makesCall: false,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@blacklist.com",
				},
			},
			expectErr:  true,
			expectCode: codes.InvalidArgument,
		},
		{
			name:      "allowed email",
			makesCall: true,
			req: &profile.CreateOrgAndUserRequest{
				Org: &profile.CreateOrgAndUserRequest_Org{
					OrgName:    "hulu",
					DomainName: "hulu.com",
				},
				User: &profile.CreateOrgAndUserRequest_User{
					Username:  "foobar",
					FirstName: "foo",
					LastName:  "bar",
					Email:     "foo@gmail.com",
				},
			},
			expectErr: false,
			resp: &profile.CreateOrgAndUserResponse{
				OrgID:  utils.ProtoFromUUID(&testOrgUUID),
				UserID: utils.ProtoFromUUID(&testUUID),
			},
		},
	}

	for _, tc := range createOrgUserTest {
		t.Run(tc.name, func(t *testing.T) {
			s := controller.NewServer(nil, d)
			if tc.makesCall {
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
			}
			resp, err := s.CreateOrgAndUser(context.Background(), tc.req)

			if tc.expectErr {
				assert.NotNil(t, err)
				c := status.Code(err)
				assert.Equal(t, c, tc.expectCode)
				return
			}

			assert.Equal(t, resp, tc.resp)
		})
	}
}

func TestServer_GetOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.NewV4()
	s := controller.NewServer(nil, d)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "hulu.com",
		OrgName:    "hulu",
	}

	d.EXPECT().
		GetOrg(orgUUID).
		Return(mockReply, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(&orgUUID))

	require.Nil(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(&orgUUID))
	assert.Equal(t, resp.DomainName, "hulu.com")
	assert.Equal(t, resp.OrgName, "hulu")
}

func TestServer_GetOrg_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.NewV4()
	s := controller.NewServer(nil, d)

	d.EXPECT().
		GetOrg(orgUUID).
		Return(nil, nil)

	resp, err := s.GetOrg(context.Background(), utils.ProtoFromUUID(&orgUUID))
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}

func TestServer_GetOrgByDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	orgUUID := uuid.NewV4()
	s := controller.NewServer(nil, d)

	mockReply := &datastore.OrgInfo{
		ID:         orgUUID,
		DomainName: "hulu.com",
		OrgName:    "hulu",
	}

	d.EXPECT().
		GetOrgByDomain("hulu.com").
		Return(mockReply, nil)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profile.GetOrgByDomainRequest{DomainName: "hulu.com"})

	require.Nil(t, err)
	assert.Equal(t, resp.ID, utils.ProtoFromUUID(&orgUUID))
	assert.Equal(t, resp.DomainName, "hulu.com")
	assert.Equal(t, resp.OrgName, "hulu")
}

func TestServer_GetOrgByDomain_MissingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	d := mock_controller.NewMockDatastore(ctrl)

	s := controller.NewServer(nil, d)

	d.EXPECT().
		GetOrgByDomain("hulu.com").
		Return(nil, nil)

	resp, err := s.GetOrgByDomain(
		context.Background(),
		&profile.GetOrgByDomainRequest{DomainName: "hulu.com"})

	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, status.Code(err), codes.NotFound)
}
