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
	claimsutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

func getTestContext() context.Context {
	return authcontext.NewContext(context.Background(), authcontext.New())
}

func TestServer_LoginNewUser_NoOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: "auth0",
	}

	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: "auth0",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			Username:         "abc@gmail.com",
			FirstName:        "first",
			LastName:         "last",
			Email:            "abc@gmail.com",
			AuthProviderID:   authProviderID,
			IdentityProvider: "auth0",
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

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
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
	assert.Equal(t, resp.OrgInfo.OrgID, "")
	assert.Equal(t, resp.OrgInfo.OrgName, "")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_NoHostDomainAndNotAuth0FailsSignup(t *testing.T) {
	// TODO(philkuz, PC-1264) This test will be removed when we generally support CreateOrg.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: "google-oauth2",
	}

	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		GetOrgByName(gomock.Any(), &profilepb.GetOrgByNameRequest{Name: "abc@gmail.com"}).
		Return(nil, status.Error(codes.NotFound, "org not found"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = doLoginRequest(getTestContext(), t, s)
	require.Error(t, err)
	assert.Equal(t, codes.NotFound, status.Code(err))
}

func TestServer_LoginNewUser_ExistingOrgWithHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@test.com",
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: authProviderID,
		HostedDomain:   "test.com",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@test.com",
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: authProviderID,
		HostedDomain:   "test.com",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "test.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:          orgPb,
			Username:       "abc@test.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@test.com",
			AuthProviderID: authProviderID,
		}).
		Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockOrg.EXPECT().
		UpdateOrg(gomock.Any(), &profilepb.UpdateOrgRequest{
			ID:         orgPb,
			DomainName: &types.StringValue{Value: "test.com"},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.UserCreated)
	assert.Equal(t, utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String(), userID)
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@test.com")
	assert.Equal(t, resp.OrgInfo.OrgID, orgID)
	assert.Equal(t, resp.OrgInfo.OrgName, "testOrg")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Login_GoogleUser_ImproperHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:            "abc@randomorg.com",
		PLUserID:         userID,
		PLOrgID:          orgID,
		IdentityProvider: "google-oauth2",
		HostedDomain:     "",
		AuthProviderID:   authProviderID,
	}

	a.EXPECT().GetUserInfo(fakeUserInfo1.AuthProviderID).Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		OrgName: "randomorg.com",
		ID:      orgPb,
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = doLoginRequest(getTestContext(), t, s)
	assert.Regexp(t, "contact support", err)
}

func TestServer_Login_GoogleUser_ProperHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:            "abc@randomorg.com",
		PLUserID:         userID,
		PLOrgID:          orgID,
		IdentityProvider: "google-oauth2",
		HostedDomain:     "randomorg.com",
		AuthProviderID:   authProviderID,
	}

	a.EXPECT().GetUserInfo(fakeUserInfo1.AuthProviderID).Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		OrgName: "randomorg.com",
		ID:      orgPb,
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)
	mockOrg.EXPECT().
		UpdateOrg(gomock.Any(), &profilepb.UpdateOrgRequest{
			ID:         orgPb,
			DomainName: &types.StringValue{Value: "randomorg.com"},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = doLoginRequest(getTestContext(), t, s)
	assert.NoError(t, err)
}

func TestServer_LoginNewUser_NoAutoCreate(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
	}

	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequestNoAutoCreate(getTestContext(), t, s)
	assert.NotNil(t, err)
	assert.Nil(t, resp)
}

func TestServer_Login_OrgLookupThrowsError(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@test.com",
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: authProviderID,
		HostedDomain:   "test.com",
	}

	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "test.com"}).
		Return(nil, errors.New("mysterious microservice error"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_LoginNewUser_CreateUserFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@test.com",
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: authProviderID,
		HostedDomain:   "test.com",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "test.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:          orgPb,
			Username:       "abc@test.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@test.com",
			AuthProviderID: authProviderID,
		}).
		Return(nil, errors.New("Could not create user"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.NotNil(t, err)
}

func TestServer_Login_BadToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return("", errors.New("bad token"))

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.NotNil(t, err)
	// Check the response data.
	stat, ok := status.FromError(err)
	assert.True(t, ok)
	assert.Equal(t, codes.Unauthenticated, stat.Code())
	assert.Nil(t, resp)
}

func TestServer_Login_UserInExistingOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		PLUserID:       userID,
		PLOrgID:        orgID,
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfo(fakeUserInfo1.AuthProviderID).Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)
	mockOrg.EXPECT().
		UpdateOrg(gomock.Any(), &profilepb.UpdateOrgRequest{
			ID:         orgPb,
			DomainName: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	require.NoError(t, err)
	assert.NotNil(t, resp)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.False(t, resp.UserCreated)
	verifyToken(t, resp.Token, userID, orgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_LoginNewUser_JoinOrgByPLOrgID(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo1 := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		PLUserID:       userID,
		PLOrgID:        orgID,
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfo(fakeUserInfo1.AuthProviderID).Return(fakeUserInfo1, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfoSecondRequest.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(nil, errors.New("Could not find user"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID: orgPb,
	}

	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			AuthProviderID: authProviderID,
			OrgID:          orgPb,
			Username:       "abc@gmail.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@gmail.com",
		}).
		Return(userPb, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)
	mockOrg.EXPECT().
		UpdateOrg(gomock.Any(), &profilepb.UpdateOrgRequest{
			ID:         orgPb,
			DomainName: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
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
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
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
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")

	env, err := authenv.New(mockProfile, mockOrg)
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
	maxExpiryTime := time.Now().Add(90 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	verifyToken(t, resp.Token, testingutils.TestUserID, testingutils.TestOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken_Service(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	viper.Set("jwt_signing_key", "jwtkey")
	env, err := authenv.New(mockProfile, mockOrg)
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
	maxExpiryTime := time.Now().Add(90 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	jwtclaims := jwt.MapClaims{}
	_, err = jwt.ParseWithClaims(token, jwtclaims, func(token *jwt.Token) (interface{}, error) {
		return []byte("jwtkey"), nil
	}, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
	assert.Equal(t, "vzmgr", jwtclaims["ServiceID"])
}

func TestServer_GetAugmentedToken_EmptyOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(&profilepb.UserInfo{
			ID:    utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
			OrgID: utils.ProtoFromUUIDStrOrNil(""),
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := claimsutils.GenerateJWTForUser(testingutils.TestUserID, "", "testing@testing.org", time.Now().Add(time.Hour), "withpixie.ai")
	token := testingutils.SignPBClaims(t, claims, "jwtkey")
	req := &authpb.GetAugmentedAuthTokenRequest{
		Token: token,
	}
	sCtx := authcontext.New()
	sCtx.Claims = claims
	resp, err := s.GetAugmentedToken(context.Background(), req)

	require.Nil(t, err)

	// Make sure expiry time is in the future & > 0.
	assert.True(t, resp.ExpiresAt > time.Now().Unix() && resp.ExpiresAt <= time.Now().Add(90*time.Minute).Unix())
	verifyToken(t, resp.Token, testingutils.TestUserID, "", resp.ExpiresAt, "jwtkey")
}

func TestServer_GetAugmentedToken_OrgDoesntExist(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(nil, status.Error(codes.NotFound, "no such org"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil("test"),
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)
	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(nil, status.Error(codes.NotFound, "no such user"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

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
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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

func TestServer_GetAugmentedTokenAPIUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	claims := claimsutils.GenerateJWTForAPIUser(testingutils.TestUserID, testingutils.TestOrgID, time.Now().Add(30*time.Minute), "withpixie.ai")
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
	maxExpiryTime := time.Now().Add(90 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	returnedClaims := jwt.MapClaims{}
	_, err = jwt.ParseWithClaims(resp.Token, returnedClaims, func(token *jwt.Token) (interface{}, error) {
		return []byte("jwtkey"), nil
	}, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
	assert.Equal(t, testingutils.TestOrgID, returnedClaims["OrgID"])
	assert.Equal(t, testingutils.TestUserID, returnedClaims["UserID"])
	assert.Equal(t, resp.ExpiresAt, int64(returnedClaims["exp"].(float64)))
	assert.True(t, returnedClaims["IsAPIUser"].(bool))
}

func TestServer_GetAugmentedTokenFromAPIKey(t *testing.T) {
	ctrl := gomock.NewController(t)
	a := mock_controllers.NewMockAuthProvider(ctrl)
	apiKeyServer := mock_controllers.NewMockAPIKeyMgr(ctrl)
	apiKeyServer.EXPECT().FetchOrgUserIDUsingAPIKey(gomock.Any(), "test_api").Return(uuid.FromStringOrNil(testingutils.TestOrgID), uuid.FromStringOrNil(testingutils.TestUserID), nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockOrgInfo := &profilepb.OrgInfo{
		ID: utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID)).
		Return(mockOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	maxExpiryTime := time.Now().Add(90 * time.Minute).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.True(t, resp.ExpiresAt > 0)

	claims := jwt.MapClaims{}
	_, err = jwt.ParseWithClaims(resp.Token, claims, func(token *jwt.Token) (interface{}, error) {
		return []byte("jwtkey"), nil
	}, jwt.WithAudience("withpixie.ai"))
	require.NoError(t, err)
	assert.Equal(t, testingutils.TestUserID, claims["UserID"])
	assert.Equal(t, testingutils.TestOrgID, claims["OrgID"])
	assert.Equal(t, resp.ExpiresAt, int64(claims["exp"].(float64)))
	assert.True(t, claims["IsAPIUser"].(bool))
}

func TestServer_Signup_LookupHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	authProviderID := "github|abcdefg"
	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		HostedDomain:   "abcorg",
	}

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "abcorg",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abcorg"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          orgPb,
		Username:       "abc@abcorg.com",
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@abcorg.com",
		AuthProviderID: "github|abcdefg",
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	assert.Equal(t, resp.OrgName, "testOrg")
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.Equal(t, resp.UserInfo.UserID, utils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@abcorg.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_FallbackToEmailDomainLookupAfterHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		HostedDomain:   "abcorg",
	}

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "abcorg",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfoSecondRequest.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	// Will make an attempt to use the HostedDomain as the domain.
	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "abcorg"}).
		Return(nil, status.Error(codes.NotFound, "not found"))

	mockOrg.EXPECT().
		GetOrgByName(gomock.Any(), &profilepb.GetOrgByNameRequest{Name: "abcorg.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          orgPb,
		Username:       "abc@abcorg.com",
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@abcorg.com",
		AuthProviderID: authProviderID,
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	assert.Equal(t, resp.OrgName, "testOrg")
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.Equal(t, resp.UserInfo.UserID, utils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@abcorg.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_AlwaysCreateOrgOnEmptyHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		HostedDomain:   "",
	}

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfoSecondRequest.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockProfile.EXPECT().CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    "abc@gmail.com",
			DomainName: "",
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:       "abc@gmail.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@gmail.com",
			AuthProviderID: authProviderID,
		},
	}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  orgPb,
			UserID: userPb,
		}, nil)

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	require.NoError(t, err)

	assert.True(t, resp.OrgCreated)
	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)

	// Check to make sure the response values match our userInfo.
	assert.Equal(t, resp.OrgID, utils.ProtoFromUUIDStrOrNil(orgID))
	assert.Equal(t, resp.OrgName, "testOrg")
	assert.Equal(t, resp.UserInfo.UserID, utils.ProtoFromUUIDStrOrNil(userID))
	assert.Equal(t, resp.UserInfo.FirstName, "first")
	assert.Equal(t, resp.UserInfo.LastName, "last")
	assert.Equal(t, resp.UserInfo.Email, "abc@gmail.com")
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_ExistingOrgWithHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "asdf@asdf.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		HostedDomain:   "asdf.com",
	}

	// Add PL UserID to the response of the second call.
	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "asdf@asdf.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "asdf.com",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfoSecondRequest.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "asdf.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          orgPb,
		Username:       "asdf@asdf.com",
		FirstName:      "first",
		LastName:       "last",
		Email:          "asdf@asdf.com",
		AuthProviderID: authProviderID,
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.False(t, resp.OrgCreated)
	assert.Equal(t, utils.ProtoFromUUIDStrOrNil(orgID), resp.OrgID)
	assert.Equal(t, "testOrg", resp.OrgName)
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.Equal(t, utils.ProtoFromUUIDStrOrNil(userID), resp.UserInfo.UserID)
	assert.Equal(t, "first", resp.UserInfo.FirstName)
	assert.Equal(t, "last", resp.UserInfo.LastName)
	assert.Equal(t, "asdf@asdf.com", resp.UserInfo.Email)
	verifyToken(t, resp.Token, fakeUserInfoSecondRequest.PLUserID, fakeUserInfoSecondRequest.PLOrgID, resp.ExpiresAt, "jwtkey")
}

func TestServer_Signup_CreateOrg_ForSelfUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
	}

	a.EXPECT().GetUserInfo(fakeUserInfo.AuthProviderID).Return(fakeUserInfo, nil)

	// Add PL UserID to the response of the second call.
	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
	}
	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(fakeUserInfoSecondRequest.AuthProviderID).Return(fakeUserInfoSecondRequest, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    "abc@gmail.com",
			DomainName: "",
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:       "abc@gmail.com",
			FirstName:      "first",
			LastName:       "last",
			Email:          "abc@gmail.com",
			AuthProviderID: authProviderID,
		},
	}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  orgPb,
			UserID: userPb,
		}, nil)

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:              orgPb,
		EnableApprovals: false,
		OrgName:         "testOrg",
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	assert.Equal(t, resp.OrgName, "testOrg")
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
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
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		CreateOrgAndUser(gomock.Any(), &profilepb.CreateOrgAndUserRequest{
			Org: &profilepb.CreateOrgAndUserRequest_Org{
				OrgName:    "abc@gmail.com",
				DomainName: "",
			},
			User: &profilepb.CreateOrgAndUserRequest_User{
				Username:       "abc@gmail.com",
				FirstName:      "first",
				LastName:       "last",
				Email:          "abc@gmail.com",
				AuthProviderID: authProviderID,
			},
		}).
		Return(nil, errors.New("Could not create user org"))

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
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

func doLoginRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*authpb.LoginReply, error) {
	req := &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
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

func TestServer_Signup_UserNotApproved(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "asdf@asdf.com",
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		HostedDomain:   "asdf.com",
	}

	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("doesnt exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:              utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		EnableApprovals: true,
	}

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "asdf.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		Username:       "asdf@asdf.com",
		FirstName:      "first",
		LastName:       "last",
		Email:          "asdf@asdf.com",
		AuthProviderID: authProviderID,
	}).Return(utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID), nil)

	mockUserInfo := &profilepb.UserInfo{
		ID:         utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID),
		OrgID:      utils.ProtoFromUUIDStrOrNil(testingutils.TestOrgID),
		Email:      "asdf@asdf.com",
		IsApproved: false,
	}

	mockProfile.EXPECT().
		GetUser(gomock.Any(), utils.ProtoFromUUIDStrOrNil(testingutils.TestUserID)).
		Return(mockUserInfo, nil)

	fakeUserInfoSecondRequest := &controllers.UserInfo{
		Email:        "asdf@asdf.com",
		FirstName:    "first",
		LastName:     "last",
		HostedDomain: "asdf.com",
	}

	a.EXPECT().SetPLMetadata(authProviderID, gomock.Any(), gomock.Any()).Do(func(uid, plorgid, plid string) {
		fakeUserInfoSecondRequest.PLUserID = plid
		fakeUserInfoSecondRequest.PLOrgID = plorgid
	}).Return(nil)
	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfoSecondRequest, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.Regexp(t, "You are not approved to log in", err)
}

func TestServer_Login_UserNotApproved(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"
	a.EXPECT().GetUserIDFromToken("tokenabc").Return(authProviderID, nil)

	fakeUserInfo := &controllers.UserInfo{
		Email:     "abc@gmail.com",
		FirstName: "first",
		LastName:  "last",
		PLUserID:  testingutils.TestUserID,
		PLOrgID:   testingutils.TestOrgID,
	}

	a.EXPECT().GetUserInfo(authProviderID).Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID:              orgPb,
		EnableApprovals: true,
	}

	mockOrg.EXPECT().
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
	mockOrg.EXPECT().
		UpdateOrg(gomock.Any(), &profilepb.UpdateOrgRequest{
			ID:         orgPb,
			DomainName: &types.StringValue{Value: ""},
		}).
		Return(nil, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.Regexp(t, "You are not approved to log in", err)
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
			mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
			env, err := authenv.New(mockProfile, mockOrg)
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
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	env, err := authenv.New(mockProfile, mockOrg)
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

func TestServer_GetAuthConnectorToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	sCtx := authcontext.New()
	sCtx.Claims = claimsutils.GenerateJWTForUser(userID, orgID, "test@test.com", time.Now(), "pixie")
	ctx := authcontext.NewContext(context.Background(), sCtx)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUser(gomock.Any(), userPb).
		Return(&profilepb.UserInfo{
			ID:    userPb,
			OrgID: orgPb,
			Email: "test@test.com",
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, nil, nil)
	require.NoError(t, err)

	resp, err := s.GetAuthConnectorToken(ctx, &authpb.GetAuthConnectorTokenRequest{
		ClusterName: "test-cluster",
	})
	require.Nil(t, err)
	require.NotNil(t, resp)
	// Make sure expiry time is in the future.
	verifyToken(t, resp.Token, userID, orgID, resp.ExpiresAt, "jwtkey")
}
