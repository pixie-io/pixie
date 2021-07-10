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
	"errors"
	"testing"
	"time"

	"github.com/dgrijalva/jwt-go/v4"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/cloud/auth/authenv"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/auth/controllers"
	mock_controllers "px.dev/pixie/src/cloud/auth/controllers/mock"
	"px.dev/pixie/src/cloud/profile/profilepb"
	mock_profile "px.dev/pixie/src/cloud/profile/profilepb/mock"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func getTestContext() context.Context {
	return authcontext.NewContext(context.Background(), authcontext.New())
}

func TestServer_LoginNewUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: "github|abcdefg",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: "github|abcdefg",
	}
	a.EXPECT().SetPLMetadata(userID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:          orgPb,
			Username:       "abc@gmail.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@gmail.com",
			AuthProviderID: "github|abcdefg",
		}).
		Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.UserCreated)
	assert.Equal(t, utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String(), userID)
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	assert.Equal(t, resp.OrgInfo.OrgID, orgID)
	assert.Equal(t, resp.OrgInfo.OrgName, "testOrg")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_NoAutoCreate(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequestNoAutoCreate(getTestContext(), t, s)
	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func TestServer_Login_OrgNameSpecified(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

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
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(nil, errors.New("organization does not exist"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_LoginNewUser_InvalidEmail(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_LoginNewUser_SupportUserNoOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "test@pixie.support",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")
	viper.Set("support_access_enabled", true)

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

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
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := uuid.FromStringOrNil("")
	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}
	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "test@pixie.support",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "my-org.com"}).
		Return(fakeOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "my-org.com")
	assert.NotNil(t, resp)
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.False(t, resp.UserCreated)
	assert.Equal(t, utils.UUIDFromProtoOrNil(resp.UserInfo.UserID), userID)
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "test@pixie.support")
	verifyToken(t, resp.Token, userID.String(), orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_InvalidOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(nil, errors.New("organization does not exist"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func TestServer_LoginNewUser_CreateUserFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}
	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     orgPb,
			Username:  "abc@gmail.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@gmail.com",
		}).
		Return(nil, errors.New("Could not create user"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_Login_BadToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("", errors.New("bad token"))

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

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
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:    "abc@gmail.com",
		PLUserID: userID,
		PLOrgID:  orgID,
	}

	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	require.NoError(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.False(t, resp.UserCreated)
	verifyToken(t, resp.Token, userID, orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Login_HasOldPLUserID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
		PLUserID:  userID,
		PLOrgID:   orgID,
	}

	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo1, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().SetPLMetadata(userID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, errors.New("Could not find user"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}

	mockProfile.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:     orgPb,
			Username:  "abc@gmail.com",
			FirstName: "first",
			LastName:  "last",
			Email:     "abc@gmail.com",
		}).
		Return(userPb, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	require.NoError(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)

	verifyToken(t, resp.Token, userID, orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockUserInfo := &profilepb.UserInfo{
		ID:    utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	require.NoError(t, err)
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
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestServiceClaims(t, "vzmgr")
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	require.NoError(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future & > 0.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(60 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	jwtclaims := jwt.MapClaims{}
	_, err = jwt.ParseWithClaims(token, jwtclaims, func(token *jwt.Token) (interface{}, error) {
		return []byte("jwtkey"), nil
	}, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
	assert.Equal(t, "vzmgr", jwtclaims["ServiceID"])
}

func TestServer_GetAugmentedToken_NoOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(nil, status.Error(codes.NotFound, "no such org"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
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
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil("test"),
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(nil, status.Error(codes.NotFound, "no such user"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
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
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockUserInfo := &profilepb.UserInfo{
		ID:    utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID: utils.ProtoFromUUIDStrOrNil("0cb7b810-9dad-11d1-80b4-00c04fd430c8"),
	}
	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
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
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey1")
	req := &authpb.GetAugmentedAuthTokenRequest{
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
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaims(t)
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
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
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := testingutils.GenerateTestClaimsWithEmail(t, "test@pixie.support")
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	require.NoError(t, err)
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
	a := mock_controllers.NewMockAuthProvider(ctrl)
	apiKeyServer := mock_controllers.NewMockAPIKeyMgr(ctrl)
	apiKeyServer.EXPECT().FetchOrgUserIDUsingAPIKey(gomock.Any(), "test_api").Return(uuid.FromStringOrNil(testingutils.TestOrgID), uuid.FromStringOrNil(testingutils.TestUserID), nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockUserInfo := &profilepb.UserInfo{
		ID:    utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		Email: "testUser@pixielabs.ai",
	}
	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, apiKeyServer)
	require.NoError(t, err)

	req := &authpb.GetAugmentedTokenForAPIKeyRequest{
		APIKey: "test_api",
	}
	resp, err := s.GetAugmentedTokenForAPIKey(context.Background(), req)

	require.NoError(t, err)
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
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: "github|abcdefg",
	}

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: "github|abcdefg",
	}
	a.EXPECT().SetPLMetadata(userID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, errors.New("user does not exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          orgPb,
		Username:       "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@gmail.com",
		AuthProviderID: "github|abcdefg",
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.False(t, resp.OrgCreated)
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil(orgID))
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.Equal(t, resp.UserInfo.UserID, utils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_CreateOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: "github|abcdefg",
	}

	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: "github|abcdefg",
	}
	a.EXPECT().SetPLMetadata(userID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfoSecondRequest, nil)

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
			Username:       "abc@gmail.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@gmail.com",
			AuthProviderID: "github|abcdefg",
		},
	}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  orgPb,
			UserID: userPb,
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.OrgCreated)
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil(orgID))
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.Equal(t, resp.UserInfo.UserID, utils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_CreateUserOrgFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
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
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_Signup_UserAlreadyExists(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("userid", nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo("userid").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func verifyToken(t *testing.T, token, expectedUserID string, expectedOrgID string, expectedExpiry int64, key string) {
	claims := jwt.MapClaims{}
	_, err := jwt.ParseWithClaims(token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte(key), nil
	}, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
	assert.Equal(t, expectedUserID, claims["UserID"])
	assert.Equal(t, expectedOrgID, claims["OrgID"])
	assert.Equal(t, expectedExpiry, int64(claims["exp"].(float64)))
}

func doLoginRequest(ctx context.Context, t *testing.T, server *controllers.Server, orgName string) (*authpb.LoginReply, error) {
	req := &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		OrgName:               orgName,
	}
	return server.Login(ctx, req)
}

func doLoginRequestNoAutoCreate(ctx context.Context, t *testing.T, server *controllers.Server) (*authpb.LoginReply, error) {
	req := &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: false,
	}
	return server.Login(ctx, req)
}

func doSignupRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*authpb.SignupReply, error) {
	req := &authpb.SignupRequest{
		AccessToken: "tokenabc",
	}
	return server.Signup(ctx, req)
}

func TestServer_LoginUserForOrgMembership(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(userID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:    "abc@gmail.com",
		PLUserID: userID,
		PLOrgID:  orgID,
	}

	a.EXPECT().GetUserInfo(userID).Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}
	mockProfile.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	require.NoError(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.False(t, resp.UserCreated)
	verifyToken(t, resp.Token, userID, orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_UserNotApproved(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(testingutils.TestUserID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo(testingutils.TestUserID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "abc@gmail.com"}).
		Return(nil, errors.New("doesnt exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:              utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		EnableApprovals: true,
	}

	mockProfile.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abc@gmail.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:     utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		Username:  "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
		Email:     "abc@gmail.com",
	}).Return(utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID), nil)

	mockUserInfo := &profilepb.UserInfo{
		ID:         utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID:      utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		Email:      "testUser@pixielabs.ai",
		IsApproved: false,
	}

	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}
	a.EXPECT().SetPLMetadata(testingutils.TestUserID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(testingutils.TestUserID).Return(fakeUserInfoSecondRequest, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.Regexp(t, "user not yet approved to log in", err)
}

func TestServer_Login_UserNotApproved(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(testingutils.TestUserID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
		PLUserID:  testingutils.TestUserID,
		PLOrgID:   testingutils.TestOrgID,
	}

	a.EXPECT().GetUserInfo(testingutils.TestUserID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID:              orgPb,
		EnableApprovals: true,
	}

	mockProfile.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Times(2).
		Return(&profilepb.UserInfo{
			ID:         utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
			OrgID:      utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
			IsApproved: false,
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s, "")
	assert.Nil(t, resp)
	assert.Regexp(t, "user not yet approved to log in", err)
}

func TestServer_InviteUser(t *testing.T) {
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

			a := mock_controllers.NewMockAuthProvider(ctrl)
			mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
			env, err := authenv.New(mockProfile)
			require.NoError(t, err)
			s, err := controllers.NewServer(env, a, nil)
			require.NoError(t, err)

			authProviderID := "8de9da32-aefe-22e2-91c5-11d250d430c8"
			userID := utils.ProtoFromUUIDStrOrNil("7cb8c921-9dad-11d1-80b4-00c04fd430c8")
			orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
			req := &authpb.InviteUserRequest{
				OrgID:     orgID,
				Email:     "bobloblaw@lawblog.com",
				FirstName: "Bob",
				LastName:  "Loblaw",
			}

			if !tc.doesUserExist {
				a.EXPECT().
					CreateIdentity("bobloblaw@lawblog.com").
					Return(&controllers.CreateIdentityResponse{
						IdentityProvider: "kratos",
						AuthProviderID:   authProviderID,
					}, nil)
				mockProfile.EXPECT().
					GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "bobloblaw@lawblog.com"}).
					Return(nil, errors.New("no such user"))
				// We always create a user if one does not exist.
				mockProfile.EXPECT().
					CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
						OrgID:            orgID,
						Username:         req.Email,
						FirstName:        req.FirstName,
						LastName:         req.LastName,
						Email:            req.Email,
						IdentityProvider: "kratos",
						AuthProviderID:   authProviderID,
					}).
					Return(userID, nil)
				a.EXPECT().
					SetPLMetadata(
						authProviderID,
						utils.ProtoToUUIDStr(orgID),
						utils.ProtoToUUIDStr(userID),
					).Return(nil)

				a.EXPECT().
					GetUserInfo(authProviderID).
					Return(&controllers.UserInfo{
						AuthProviderID: authProviderID,
						PLUserID:       utils.ProtoToUUIDStr(userID),
					}, nil)
				mockProfile.EXPECT().
					UpdateUser(gomock.Any(),
						&profilepb.UpdateUserRequest{
							ID:         userID,
							IsApproved: &types.BoolValue{Value: true},
						}).
					Return(nil, nil)
			} else {
				mockProfile.EXPECT().
					GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "bobloblaw@lawblog.com"}).
					Return(&profilepb.UserInfo{
						AuthProviderID: authProviderID,
					}, nil)
			}

			if !tc.doesUserExist || !tc.mustCreate {
				a.EXPECT().
					CreateInviteLink(authProviderID).Return(
					&controllers.CreateInviteLinkResponse{
						InviteLink: "self-service/recovery/methods",
					}, nil)
			}

			resp, err := s.InviteUser(getTestContext(), req)

			if tc.err == nil {
				require.NoError(t, err)
				assert.Regexp(t, "self-service/recovery/methods", resp.InviteLink)
			} else {
				assert.Equal(t, err, tc.err)
			}
		})
	}
}

func TestServer_CreateOrgAndInviteUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuthProvider(ctrl)
	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	env, err := authenv.New(mockProfile)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	authProviderID := "8de9da32-aefe-22e2-91c5-11d250d430c8"
	userID := utils.ProtoFromUUIDStrOrNil("7cb8c921-9dad-11d1-80b4-00c04fd430c8")
	orgID := utils.ProtoFromUUIDStrOrNil("6ba7b810-9dad-11d1-80b4-00c04fd430c8")
	email := "admin@default.com"

	mockProfile.EXPECT().CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			DomainName: "default.com",
			OrgName:    "default",
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:         email,
			FirstName:        "admin",
			LastName:         "admin",
			Email:            email,
			AuthProviderID:   authProviderID,
			IdentityProvider: "kratos",
		},
	}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  orgID,
			UserID: userID,
		}, nil)

	a.EXPECT().
		GetUserInfo(authProviderID).
		Return(&controllers.UserInfo{
			AuthProviderID: authProviderID,
			PLUserID:       utils.ProtoToUUIDStr(userID),
		}, nil)

	a.EXPECT().
		SetPLMetadata(
			authProviderID,
			utils.ProtoToUUIDStr(orgID),
			utils.ProtoToUUIDStr(userID),
		).Return(nil)

	a.EXPECT().
		CreateIdentity(email).
		Return(&controllers.CreateIdentityResponse{
			IdentityProvider: "kratos",
			AuthProviderID:   authProviderID,
		}, nil)

	a.EXPECT().
		CreateInviteLink(authProviderID).Return(
		&controllers.CreateInviteLinkResponse{
			InviteLink: "self-service/recovery/methods",
		}, nil)

	req := &authpb.CreateOrgAndInviteUserRequest{
		Org: &authpb.CreateOrgAndInviteUserRequest_Org{
			DomainName: "default.com",
			OrgName:    "default",
		},
		User: &authpb.CreateOrgAndInviteUserRequest_User{
			Username:  email,
			FirstName: "admin",
			LastName:  "admin",
			Email:     email,
		},
	}

	resp, err := s.CreateOrgAndInviteUser(getTestContext(), req)
	require.NoError(t, err)
	assert.Regexp(t, "self-service/recovery/methods", resp.InviteLink)
}
