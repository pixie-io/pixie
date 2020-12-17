package controllers_test

import (
	"errors"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go"
	"github.com/golang/mock/gomock"
	uuid "github.com/satori/go.uuid"
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
	pbutils "pixielabs.ai/pixielabs/src/utils"
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
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
		Picture:     "something",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      &uuidpb.UUID{Data: []byte(orgID)},
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo, nil)

	a.EXPECT().GetClientID().Return("foo").AnyTimes()

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		UserID:      userID,
		AppMetadata: make(map[string]*controllers.UserMetadata),
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
		Picture:     "something",
	}
	a.EXPECT().SetPLMetadata(userID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata["foo"] = &controllers.UserMetadata{
			PLUserID: plid,
			PLOrgID:  plorgid,
		}
	}).Return(nil)
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     &uuidpb.UUID{Data: []byte(orgID)},
			Username:  "abc@gmail.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@gmail.com",
		}).
		Return(pbutils.ProtoFromUUIDStrOrNil(userID), nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             &uuidpb.UUID{Data: []byte(userID)},
			ProfilePicture: "something",
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.UserCreated)
	assert.Equal(t, pbutils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String(), userID)
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	assert.Equal(t, resp.OrgInfo.OrgID, orgID)
	assert.Equal(t, resp.OrgInfo.OrgName, "testOrg")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.AppMetadata["foo"].PLUserID, fakeUserInfoSecondRequest.AppMetadata["foo"].PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_NoAutoCreate(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequestNoAutoCreate(getTestContext(), t, s)
	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func TestServer_Login_OrgNameSpecified(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "testorg")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.InvalidArgument, stat.Code())
}

func TestServer_Login_MissingOrgError(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(nil, errors.New("organization does not exist"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
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
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_LoginNewUser_SupportUserNoOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "test@pixie.support",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.InvalidArgument, stat.Code())
}

func TestServer_LoginNewUser_SupportUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userID := uuid.FromStringOrNil("")
	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}
	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "test@pixie.support",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "hulu.com"}).
		Return(fakeOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "hulu.com")
	assert.NotNil(t, resp)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.False(t, resp.UserCreated)
	assert.Equal(t, pbutils.UUIDFromProtoOrNil(resp.UserInfo.UserID), userID)
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "test@pixie.support")
	verifyToken(t, resp.Token, userID.String(), orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_InvalidOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(nil, errors.New("organization does not exist"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
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
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     &uuidpb.UUID{Data: []byte(orgID)},
			Username:  "abc@gmail.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@gmail.com",
		}).
		Return(nil, errors.New("Could not create user"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
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
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
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

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	appMetadata := make(map[string]*controllers.UserMetadata)
	appMetadata["foo"] = &controllers.UserMetadata{
		PLUserID: "pluserid",
		PLOrgID:  "plorgid",
	}

	fakeUserInfo1 := &controllers.UserInfo{
		AppMetadata: appMetadata,
		Email:       "abc@gmail.com",
	}

	a.EXPECT().GetClientID().Return("foo").MinTimes(1)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), &uuidpb.UUID{Data: []byte("pluserid")}).
		Return(nil, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}
	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             &uuidpb.UUID{Data: []byte("pluserid")},
			ProfilePicture: "",
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.False(t, resp.UserCreated)
	verifyToken(t, resp.Token, "pluserid", "plorgid", resp.ExpiresAt, "jwtkey")
}

func TestServer_Login_HasOldPLUserID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	appMetadata := make(map[string]*controllers.UserMetadata)
	appMetadata["foo"] = &controllers.UserMetadata{
		PLUserID: "pluserid",
		PLOrgID:  "plorgid",
	}

	fakeUserInfo1 := &controllers.UserInfo{
		AppMetadata: appMetadata,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetClientID().Return("foo").MinTimes(1)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo1, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		AppMetadata: make(map[string]*controllers.UserMetadata),
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().SetPLMetadata("userid", gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata["foo"] = &controllers.UserMetadata{
			PLUserID: plid,
			PLOrgID:  plorgid,
		}
	}).Return(nil)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), &uuidpb.UUID{Data: []byte("pluserid")}).
		Return(nil, errors.New("Could not find user"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     &uuidpb.UUID{Data: []byte(orgID)},
			Username:  "abc@gmail.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@gmail.com",
		}).
		Return(&uuidpb.UUID{Data: []byte(userID)}, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             &uuidpb.UUID{Data: []byte(userID)},
			ProfilePicture: "",
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, userID, orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockUserInfo := &profilepb.UserInfo{
		ID:    pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID: pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockOrgInfo := &profilepb.OrgInfo{
		ID: pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockProfile.EXPECT().
		GetUser(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
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

	verifyToken(t, resp.Token, testingutils.TestUserID, testingutils.TestOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken_Service(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestServiceClaims(t, "vzmgr")
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

	jwtclaims := jwt.MapClaims{}
	_, err = jwt.ParseWithClaims(token, jwtclaims, func(token *jwt.Token) (interface{}, error) {
		return []byte("jwtkey"), nil
	})
	assert.Nil(t, err)
	assert.Equal(t, "vzmgr", jwtclaims["ServiceID"])
}

func TestServer_GetAugmentedToken_NoOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrg(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(nil, status.Error(codes.NotFound, "no such org"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	assert.Nil(t, resp)
	assert.NotNil(t, err)

	e, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, e.Code(), codes.Unauthenticated)
}

func TestServer_GetAugmentedToken_NoUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockOrgInfo := &profilepb.OrgInfo{
		ID: pbutils.ProtoFromUUIDStrOrNil("test"),
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(nil, status.Error(codes.NotFound, "no such user"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	assert.Nil(t, resp)
	assert.NotNil(t, err)

	e, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, e.Code(), codes.Unauthenticated)
}

func TestServer_GetAugmentedToken_MismatchedOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockUserInfo := &profilepb.UserInfo{
		ID:    pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID: pbutils.ProtoFromUUIDStrOrNil("0cb7b810-9dad-11d1-80b4-00c04fd430c8"),
	}
	mockOrgInfo := &profilepb.OrgInfo{
		ID: pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockProfile.EXPECT().
		GetUser(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &pb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	assert.Nil(t, resp)
	assert.NotNil(t, err)

	e, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, e.Code(), codes.Unauthenticated)
}

func TestServer_GetAugmentedTokenBadSigningKey(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
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
	s, err := controllers.NewServer(env, a, nil)
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

func TestServer_GetAugmentedTokenSupportAccount(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrgInfo := &profilepb.OrgInfo{
		ID: pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	claims := testingutils.GenerateTestClaimsWithEmail(t, "test@pixie.support")
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

	verifyToken(t, resp.Token, testingutils.TestUserID, testingutils.TestOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedTokenFromAPIKey(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	apiKeyServer := mock_controllers.NewMockAPIKeyMgr(ctrl)
	apiKeyServer.EXPECT().FetchOrgUserIDUsingAPIKey(gomock.Any(), "test_api").Return(uuid.FromStringOrNil(testingutils.TestOrgID), uuid.FromStringOrNil(testingutils.TestUserID), nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockUserInfo := &profilepb.UserInfo{
		ID:    pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID: pbutils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		Email: "testUser@pixielabs.ai",
	}
	mockProfile.EXPECT().
		GetUser(gomock.Any(), pbutils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, apiKeyServer)
	assert.Nil(t, err)

	req := &pb.GetAugmentedTokenForAPIKeyRequest{
		APIKey: "test_api",
	}
	resp, err := s.GetAugmentedTokenForAPIKey(context.Background(), req)

	assert.Nil(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future & > 0.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(60 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	verifyToken(t, resp.Token, testingutils.TestUserID, testingutils.TestOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_ExistingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetClientID().Return("foo").AnyTimes()

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		AppMetadata: make(map[string]*controllers.UserMetadata),
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}
	a.EXPECT().SetPLMetadata("userid", gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata["foo"] = &controllers.UserMetadata{
			PLUserID: plid,
			PLOrgID:  plorgid,
		}
	}).Return(nil)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, errors.New("user does not exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: &uuidpb.UUID{Data: []byte(orgID)},
	}

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:     &uuidpb.UUID{Data: []byte(orgID)},
		Username:  "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
		Email:     "abc@gmail.com",
	}).Return(pbutils.ProtoFromUUIDStrOrNil(userID), nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.False(t, resp.OrgCreated)
	assert.Equal(t, resp.OrgID, pbutils.ProtoFromUUIDStrOrNil(orgID))
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.Equal(t, resp.UserInfo.UserID, pbutils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.AppMetadata["foo"].PLUserID, fakeUserInfoSecondRequest.AppMetadata["foo"].PLOrgID, resp.ExpiresAt, "jwtkey")

}

func TestServer_Signup_CreateOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetClientID().Return("foo").AnyTimes()

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		AppMetadata: make(map[string]*controllers.UserMetadata),
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}
	a.EXPECT().SetPLMetadata("userid", gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.AppMetadata["foo"] = &controllers.UserMetadata{
			PLUserID: plid,
			PLOrgID:  plorgid,
		}
	}).Return(nil)
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(nil, errors.New("organization does not exist"))

	mockProfile.EXPECT().CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    "abc@gmail.com",
			DomainName: "abc@gmail.com",
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:  "abc@gmail.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@gmail.com",
		},
	}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  &uuidpb.UUID{Data: []byte(orgID)},
			UserID: &uuidpb.UUID{Data: []byte(userID)},
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.OrgCreated)
	assert.Equal(t, resp.OrgID, pbutils.ProtoFromUUIDStrOrNil(orgID))
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.Equal(t, resp.UserInfo.UserID, pbutils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.AppMetadata["foo"].PLUserID, fakeUserInfoSecondRequest.AppMetadata["foo"].PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_CreateUserOrgFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(nil, errors.New("organization does not exist"))

	mockProfile.EXPECT().
		CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
			Org: &profilepb.CreateOrgAndUserRequest_Org{
				OrgName:    "abc@gmail.com",
				DomainName: "abc@gmail.com",
			},
			User: &profilepb.CreateOrgAndUserRequest_User{
				Username:  "abc@gmail.com",
				FirstName: "first",
				LastName:  "last",
				Email:     "abc@gmail.com",
			},
		}).
		Return(nil, errors.New("Could not create user org"))

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_Signup_UserAlreadyExists(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuth0Connector(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		AppMetadata: nil,
		Email:       "abc@gmail.com",
		FirstName:   "first",
		LastName:    "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	assert.Nil(t, err)
	s, err := controllers.NewServer(env, a, nil)
	assert.Nil(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func verifyToken(t *testing.T, token, expectedUserID string, expectedOrgID string, expectedExpiry int64, key string) {
	claims := jwt.MapClaims{}
	_, err := jwt.ParseWithClaims(token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(key), nil
	})
	assert.Nil(t, err)
	assert.Equal(t, expectedUserID, claims["UserID"])
	assert.Equal(t, expectedOrgID, claims["OrgID"])
	assert.Equal(t, expectedExpiry, int64(claims["exp"].(float64)))
}

func doLoginRequest(ctx context.Context, t *testing.T, server *controllers.Server, orgName string) (*pb.LoginReply, error) {
	req := &pb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		OrgName:               orgName,
	}
	return server.Login(ctx, req)
}

func doLoginRequestNoAutoCreate(ctx context.Context, t *testing.T, server *controllers.Server) (*pb.LoginReply, error) {
	req := &pb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: false,
	}
	return server.Login(ctx, req)
}

func doSignupRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*pb.SignupReply, error) {
	req := &pb.SignupRequest{
		AccessToken: "tokenabc",
	}
	return server.Signup(ctx, req)
}
