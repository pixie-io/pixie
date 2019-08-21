package controllers_test

import (
	"errors"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"golang.org/x/net/context"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/auth/authenv"
	"pixielabs.ai/pixielabs/src/cloud/auth/controllers"
	mock_controllers "pixielabs.ai/pixielabs/src/cloud/auth/controllers/mock"
	pb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	mock_profile "pixielabs.ai/pixielabs/src/cloud/profile/profilepb/mock"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func getTestContext() context.Context {
	return authcontext.NewContext(context.Background(), authcontext.New())
}

func TestServer_LoginNewUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		AppMetadata: &controllers.UserMetadata{},
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}
	a.EXPECT().SetPLMetadata("userid", gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata.PLUserID = plid
		fakeUserInfoSecondRequest.AppMetadata.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "defg.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     &uuidpb.UUID{Data: []byte(orgID)},
			Username:  "abc@defg.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		}).
		Return(&uuidpb.UUID{Data: []byte(userID)}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.AppMetadata.PLUserID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_InvalidEmail(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_LoginNewUser_InvalidOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "defg.com"}).
		Return(nil, errors.New("organization does not exist"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func TestServer_LoginNewUser_CreateUserFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "defg.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     &uuidpb.UUID{Data: []byte(orgID)},
			Username:  "abc@defg.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@defg.com",
		}).
		Return(nil, errors.New("Could not create user"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_Login_BadToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("", errors.New("bad token"))

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.NotNil(t, err)
	// Check the response data.
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.Unauthenticated, stat.Code())
	assert.Nil(t, resp)
}

func TestServer_Login_HasPLUserID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo1 := &controllers.UserInfo{
		AppMetadata: &controllers.UserMetadata{
			PLUserID: "pluserid",
		},
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, "pluserid", resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future & > 0.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(60 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	verifyToken(t, resp.Token, "test", resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedTokenBadSigningKey(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey1")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	resp, err := s.GetAugmentedToken(context.Background(), req)

	assert.NotNil(t, err)
	assert.Nil(t, resp)

	e, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, e.Code(), codes.Unauthenticated)
}

func TestServer_GetAugmentedTokenBadToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token + "a",
	}
	resp, err := s.GetAugmentedToken(context.Background(), req)

	assert.NotNil(t, err)
	assert.Nil(t, resp)

	e, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, e.Code(), codes.Unauthenticated)
}

func TestServer_CreateUserOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		AppMetadata: &controllers.UserMetadata{},
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}
	a.EXPECT().SetPLMetadata("userid", gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata.PLUserID = plid
		fakeUserInfoSecondRequest.AppMetadata.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
			Org: &profilepb.CreateOrgAndUserRequest_Org{
				OrgName:    "defg",
				DomainName: "defg.com",
			},
			User: &profilepb.CreateOrgAndUserRequest_User{
				Username:  "abc@defg.com",
				FirstName: "first",
				LastName:  "last",
				Email:     "abc@defg.com",
			},
		}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  &uuidpb.UUID{Data: []byte(orgID)},
			UserID: &uuidpb.UUID{Data: []byte(userID)},
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doCreateUserOrgRequest(getTestContext(), t, s)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(7 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.AppMetadata.PLUserID, resp.ExpiresAt, "jwtkey")
}

func TestServer_CreateUserOrg_NonMatchingEmails(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "anotheremail@test.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doCreateUserOrgRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_CreateUserOrg_AccountExists(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: &controllers.UserMetadata{
			PLUserID: "pluserid",
		},
		Email:     "abc@defg.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doCreateUserOrgRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_CreateUserOrg_CreateFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@defg.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
			Org: &profilepb.CreateOrgAndUserRequest_Org{
				OrgName:    "defg",
				DomainName: "defg.com",
			},
			User: &profilepb.CreateOrgAndUserRequest_User{
				Username:  "abc@defg.com",
				FirstName: "first",
				LastName:  "last",
				Email:     "abc@defg.com",
			},
		}).
		Return(nil, errors.New("Could not create user org"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a)
	assert.Nil(t, err)

	resp, err := doCreateUserOrgRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func verifyToken(t *testing.T, token, expectedUserID string, expectedExpiry int64, key string) {
	claims := jwt.MapClaims{}
	_, err := jwt.ParseWithClaims(token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(key), nil
	})
	assert.Nil(t, err)
	assert.Equal(t, expectedUserID, claims["UserID"])
	assert.Equal(t, expectedExpiry, int64(claims["exp"].(float64)))
}

func doLoginRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*pb.LoginReply, error) {
	req := &pb.LoginRequest{
		AccessToken: "tokenabc",
	}
	return server.Login(ctx, req)
}

func doCreateUserOrgRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*pb.CreateUserOrgResponse, error) {
	req := &pb.CreateUserOrgRequest{
		AccessToken: "tokenabc",
		UserEmail:   "abc@defg.com",
		DomainName:  "defg.com",
		OrgName:     "defg",
	}
	return server.CreateUserOrg(ctx, req)
}
