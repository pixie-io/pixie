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
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/testingutils"
)

const (
	googleIdentityProvider = "google-oauth2"
	auth0IdentityProvider  = "auth0"
	kratosIdentityProvider = "kratos"
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

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			FirstName:        "first",
			LastName:         "last",
			Email:            "abc@gmail.com",
			AuthProviderID:   authProviderID,
			IdentityProvider: auth0IdentityProvider,
		}).
		Return(userPb, nil)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

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

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@test.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "test.com",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "test.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:            orgPb,
			FirstName:        "first",
			LastName:         "last",
			Email:            "abc@test.com",
			IdentityProvider: googleIdentityProvider,
			AuthProviderID:   authProviderID,
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
}

func TestServer_Login_ExistingOrgWithHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@test.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "test.com",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).Times(2).
		Return(&profilepb.UserInfo{
			ID: userPb,
		}, nil)

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "test.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:    userPb,
			OrgID: orgPb,
			IsApproved: &types.BoolValue{
				Value: true,
			},
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
	assert.Equal(t, resp.OrgInfo.OrgID, orgID)
	assert.Equal(t, resp.OrgInfo.OrgName, "testOrg")
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

	fakeUserInfo1 := &controllers.UserInfo{
		Email:            "abc@randomorg.com",
		EmailVerified:    true,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "",
		AuthProviderID:   authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(&profilepb.UserInfo{
			ID:    userPb,
			OrgID: orgPb,
		}, nil)

	fakeOrgInfo := &profilepb.OrgInfo{
		OrgName:    "randomorg.com",
		ID:         orgPb,
		DomainName: nil,
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

	fakeUserInfo1 := &controllers.UserInfo{
		Email:            "abc@randomorg.com",
		EmailVerified:    true,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "randomorg.com",
		AuthProviderID:   authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).Times(2).
		Return(&profilepb.UserInfo{
			ID:    userPb,
			OrgID: orgPb,
		}, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		OrgName: "randomorg.com",
		ID:      orgPb,
	}
	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{
			DomainName: "randomorg.com",
		}).
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

func TestServer_Login_GoogleUser_ManualOrgNoHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo1 := &controllers.UserInfo{
		Email:            "abc@randomorg.com",
		EmailVerified:    true,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "",
		AuthProviderID:   authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).Times(2).
		Return(&profilepb.UserInfo{
			ID:    userPb,
			OrgID: orgPb,
		}, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		OrgName: "randomorg.com",
		ID:      orgPb,
		// DomainName will be an empty string.
		DomainName: &types.StringValue{Value: ""},
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

	_, err = doLoginRequest(getTestContext(), t, s)
	assert.NoError(t, err)
}

func TestServer_LoginNewUser_NoAutoCreate(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: false,
	})
	assert.Regexp(t, "user not found, please register.", err)
}

func TestServer_LoginNewUser_OrgLookupThrowsError(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@test.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "test.com",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

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

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@test.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "test.com",
	}

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "testOrg",
	}
	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

	mockOrg.EXPECT().
		GetOrgByDomain(gomock.Any(), &profilepb.GetOrgByDomainRequest{DomainName: "test.com"}).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:            orgPb,
			FirstName:        "first",
			LastName:         "last",
			Email:            "abc@test.com",
			AuthProviderID:   authProviderID,
			IdentityProvider: googleIdentityProvider,
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
	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(nil, errors.New("bad token"))

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

	fakeUserInfo1 := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    true,
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo1, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).Times(2).
		Return(&profilepb.UserInfo{
			ID:    userPb,
			OrgID: orgPb,
		}, nil)
	fakeOrgInfo := &profilepb.OrgInfo{
		ID:         orgPb,
		DomainName: &types.StringValue{},
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

	parsed, err := srvutils.ParseToken(resp.Token, "jwtkey", "withpixie.ai")
	require.NoError(t, err)
	assert.Equal(t, "vzmgr", srvutils.GetServiceID(parsed))
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

	claims := srvutils.GenerateJWTForUser(testingutils.TestUserID, "", "testing@testing.org", time.Now().Add(time.Hour), "withpixie.ai")
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

	claims := srvutils.GenerateJWTForAPIUser(testingutils.TestUserID, testingutils.TestOrgID, time.Now().Add(30*time.Minute), "withpixie.ai")
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

	parsed, err := srvutils.ParseToken(resp.Token, "jwtkey", "withpixie.ai")
	require.NoError(t, err)
	assert.Equal(t, testingutils.TestOrgID, srvutils.GetOrgID(parsed))
	assert.Equal(t, testingutils.TestUserID, srvutils.GetUserID(parsed))
	assert.Equal(t, resp.ExpiresAt, parsed.Expiration().Unix())
	assert.True(t, srvutils.GetIsAPIUser(parsed))
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

	parsed, err := srvutils.ParseToken(resp.Token, "jwtkey", "withpixie.ai")
	require.NoError(t, err)
	assert.Equal(t, testingutils.TestUserID, srvutils.GetUserID(parsed))
	assert.Equal(t, testingutils.TestOrgID, srvutils.GetOrgID(parsed))
	assert.Equal(t, resp.ExpiresAt, parsed.Expiration().Unix())
	assert.True(t, srvutils.GetIsAPIUser(parsed))
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

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "abcorg",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

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

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "abcorg",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

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

	fakeUserInfo := &controllers.UserInfo{
		Email:          "asdf@asdf.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
		HostedDomain:   "asdf.com",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

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
}

func TestServer_Signup_DoNotCreateOrgForEmptyHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		Picture:        "something",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@gmail.com",
		AuthProviderID: authProviderID,
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

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
	assert.False(t, resp.OrgCreated)
	assert.Nil(t, resp.OrgID)
	assert.Equal(t, "", resp.OrgName)
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.Equal(t, utils.ProtoFromUUIDStrOrNil(userID), resp.UserInfo.UserID)
	assert.Equal(t, "first", resp.UserInfo.FirstName)
	assert.Equal(t, "last", resp.UserInfo.LastName)
	assert.Equal(t, "abc@gmail.com", resp.UserInfo.Email)
}

func TestServer_Signup_CreateUserOrgFailed(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@gmail.com",
		AuthProviderID: authProviderID,
	}).Return(nil, errors.New("db error"))

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

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@gmail.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

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
	parsed, err := srvutils.ParseToken(token, key, "withpixie.ai")
	require.NoError(t, err)
	assert.Equal(t, expectedUserID, srvutils.GetUserID(parsed))
	assert.Equal(t, expectedOrgID, srvutils.GetOrgID(parsed))
	assert.Equal(t, expectedExpiry, parsed.Expiration().Unix())
}

func doLoginRequest(ctx context.Context, t *testing.T, server *controllers.Server) (*authpb.LoginReply, error) {
	req := &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
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

	fakeUserInfo := &controllers.UserInfo{
		Email:          "asdf@asdf.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
		HostedDomain:   "asdf.com",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

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
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(mockUserInfo, nil)

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

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)
	fakeOrgInfo := &profilepb.OrgInfo{
		OrgName:         "abc@gmail.com",
		ID:              orgPb,
		EnableApprovals: true,
	}

	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).Times(2).
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
						IdentityProvider: kratosIdentityProvider,
						AuthProviderID:   authProviderID,
					}, nil)
				mockProfile.EXPECT().
					GetUserByEmail(gomock.Any(), &profilepb.GetUserByEmailRequest{Email: "bobloblaw@lawblog.com"}).
					Return(nil, errors.New("no such user"))
				// We always create a user if one does not exist.
				mockProfile.EXPECT().
					CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
						OrgID:            orgID,
						FirstName:        req.FirstName,
						LastName:         req.LastName,
						Email:            req.Email,
						IdentityProvider: kratosIdentityProvider,
						AuthProviderID:   authProviderID,
					}).
					Return(userID, nil)

				times := 1
				if tc.doesUserExist {
					times = 2
				}
				mockProfile.EXPECT().
					GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
						AuthProviderID: authProviderID,
					}).Times(times).
					Return(&profilepb.UserInfo{
						ID:         userID,
						IsApproved: true,
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
			FirstName:        "admin",
			LastName:         "admin",
			Email:            email,
			AuthProviderID:   authProviderID,
			IdentityProvider: kratosIdentityProvider,
		},
	}).
		Return(&profilepb.CreateOrgAndUserResponse{
			OrgID:  orgID,
			UserID: userID,
		}, nil)

	a.EXPECT().
		CreateIdentity(email).
		Return(&controllers.CreateIdentityResponse{
			IdentityProvider: kratosIdentityProvider,
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
	sCtx.Claims = srvutils.GenerateJWTForUser(userID, orgID, "test@test.com", time.Now(), "pixie")
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

func TestServer_Login_UnverifiedUser_FailsLogin(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    false,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		FirstName:        "first",
		LastName:         "last",
		Email:            "abc@gmail.com",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doLoginRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.Regexp(t, "please verify your email before proceeding", err)
}

func TestServer_Signup_UnverifiedUser_FailsLogin(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    false,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))
	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		FirstName:        "first",
		LastName:         "last",
		Email:            "abc@gmail.com",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := doSignupRequest(getTestContext(), t, s)
	assert.Nil(t, resp)
	assert.Regexp(t, "please verify your email before proceeding", err)
}

func TestServer_Signup_WithInviteToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	authProviderID := "github|abcdefg"
	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		EmailVerified:  true,
		FirstName:      "first",
		LastName:       "last",
		Picture:        "something",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "invitedorg",
	}
	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          orgPb,
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@abcorg.com",
		AuthProviderID: "github|abcdefg",
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil(orgID),
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	resp, err := s.Signup(getTestContext(), &authpb.SignupRequest{
		AccessToken: "tokenabc",
		InviteToken: "invite-token",
	})
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.False(t, resp.OrgCreated)
	assert.Equal(t, orgPb, resp.OrgID)
	assert.Equal(t, "invitedorg", resp.OrgName)
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt <= maxExpiryTime)
	assert.Equal(t, utils.ProtoFromUUIDStrOrNil(userID), resp.UserInfo.UserID)
	assert.Equal(t, "first", resp.UserInfo.FirstName)
	assert.Equal(t, "last", resp.UserInfo.LastName)
	assert.Equal(t, "abc@abcorg.com", resp.UserInfo.Email)
}

func TestServer_Login_WithInviteToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "invitedorg",
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:            orgPb,
			FirstName:        "first",
			LastName:         "last",
			Email:            "abc@gmail.com",
			AuthProviderID:   authProviderID,
			IdentityProvider: auth0IdentityProvider,
		}).
		Return(userPb, nil)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil(orgID),
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

	resp, err := s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		InviteToken:           "invite-token",
	})
	require.NoError(t, err)

	// Make sure expiry time is in the future.
	currentTime := time.Now().Unix()
	maxExpiryTime := time.Now().Add(120 * 24 * time.Hour).Unix()
	assert.True(t, resp.ExpiresAt > currentTime && resp.ExpiresAt < maxExpiryTime)
	assert.True(t, resp.UserCreated)
	assert.Equal(t, userID, utils.UUIDFromProtoOrNil(resp.UserInfo.UserID).String())
	assert.Equal(t, "first", resp.UserInfo.FirstName)
	assert.Equal(t, "last", resp.UserInfo.LastName)
	assert.Equal(t, "abc@gmail.com", resp.UserInfo.Email)
	assert.Equal(t, orgID, resp.OrgInfo.OrgID)
	assert.Equal(t, "invitedorg", resp.OrgInfo.OrgName)
}

func TestServer_Login_WithInvalidInviteToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: false,
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		InviteToken:           "invite-token",
	})
	assert.Regexp(t, "received invalid or expired invite", err)
}

func TestServer_Signup_WithInvalidInviteToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)
	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: false,
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = s.Signup(getTestContext(), &authpb.SignupRequest{
		AccessToken: "tokenabc",
		InviteToken: "invite-token",
	})
	assert.Regexp(t, "received invalid or expired invite", err)
}

func TestServer_Login_InviteErrorsIfUserAlreadyBelongsToOrg(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)
	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(&profilepb.UserInfo{
			ID:    userPb,
			OrgID: orgPb,
		}, nil)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			// This org is differnet than the one they are a part of.
			OrgID: utils.ProtoFromUUIDStrOrNil("5ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		InviteToken:           "invite-token",
	})
	assert.Regexp(t, "cannot join org - user already belongs to another org", err)
}

func TestServer_Login_PreventFollowingInviteIfUserHasHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@abcorg.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "abcorg.com",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil("5ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)
	_, err = s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		InviteToken:           "invite-token",
	})
	assert.Regexp(t, "This email is managed by Google Workspace and can only be used to join its associated Google Workspace managed org. Please retry the invite link using another email.", err)
}

func TestServer_Signup_PreventFollowingInviteIfUserHasHostedDomain(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@abcorg.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: googleIdentityProvider,
		HostedDomain:     "abcorg.com",
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil("5ba7b810-9dad-11d1-80b4-00c04fd430c8"),
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)
	_, err = s.Signup(getTestContext(), &authpb.SignupRequest{
		AccessToken: "tokenabc",
		InviteToken: "invite-token",
	})
	assert.Regexp(t, "gsuite users are not allowed to follow invites. Please join the org with another account", err)
}

func TestServer_Login_UnverifiedUserWithInviteLink_CreatesUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    false,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "invitedorg",
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
			OrgID:            orgPb,
			FirstName:        "first",
			LastName:         "last",
			Email:            "abc@gmail.com",
			AuthProviderID:   authProviderID,
			IdentityProvider: auth0IdentityProvider,
		}).
		Return(userPb, nil)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).
		Return(nil, status.Error(codes.NotFound, "user not found"))

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil(orgID),
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

	_, err = s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		InviteToken:           "invite-token",
	})
	assert.Regexp(t, "please verify your email before proceeding", err)
}

func TestServer_Signup_UnverifiedUserWithInviteLink_CreatesUser(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"

	authProviderID := "github|abcdefg"
	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)

	fakeUserInfo := &controllers.UserInfo{
		Email:          "abc@abcorg.com",
		EmailVerified:  false,
		FirstName:      "first",
		LastName:       "last",
		AuthProviderID: authProviderID,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: authProviderID}).
		Return(nil, errors.New("user does not exist"))

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:      orgPb,
		OrgName: "invitedorg",
	}

	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().CreateUser(gomock.Any(), &profilepb.CreateUserRequest{
		OrgID:          orgPb,
		FirstName:      "first",
		LastName:       "last",
		Email:          "abc@abcorg.com",
		AuthProviderID: "github|abcdefg",
	}).Return(utils.ProtoFromUUIDStrOrNil(userID), nil)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil(orgID),
		}, nil)

	viper.Set("jwt_signing_key", "jwtkey")
	viper.Set("domain_name", "withpixie.ai")

	env, err := authenv.New(mockProfile, mockOrg)
	require.NoError(t, err)
	s, err := controllers.NewServer(env, a, nil)
	require.NoError(t, err)

	_, err = s.Signup(getTestContext(), &authpb.SignupRequest{
		AccessToken: "tokenabc",
		InviteToken: "invite-token",
	})
	assert.Regexp(t, "please verify your email before proceeding", err)
}

func TestServer_Login_ExistingOrglessUserWithInviteToken(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	userID := "7ba7b810-9dad-11d1-80b4-00c04fd430c8"
	userPb := utils.ProtoFromUUIDStrOrNil(userID)

	orgID := "6ba7b810-9dad-11d1-80b4-00c04fd430c8"
	orgPb := utils.ProtoFromUUIDStrOrNil(orgID)

	// Setup expectations for the mocks.
	a := mock_controllers.NewMockAuthProvider(ctrl)
	authProviderID := "github|abc123"

	fakeUserInfo := &controllers.UserInfo{
		Email:            "abc@gmail.com",
		EmailVerified:    true,
		FirstName:        "first",
		LastName:         "last",
		Picture:          "something",
		AuthProviderID:   authProviderID,
		IdentityProvider: auth0IdentityProvider,
	}

	a.EXPECT().GetUserInfoFromAccessToken("tokenabc").Return(fakeUserInfo, nil)

	mockProfile := mock_profile.NewMockProfileServiceClient(ctrl)
	mockOrg := mock_profile.NewMockOrgServiceClient(ctrl)

	fakeOrgInfo := &profilepb.OrgInfo{
		ID:              orgPb,
		OrgName:         "invitedorg",
		EnableApprovals: true,
	}
	mockOrg.EXPECT().
		GetOrg(gomock.Any(), orgPb).
		Return(fakeOrgInfo, nil)

	mockProfile.EXPECT().
		GetUserByAuthProviderID(gomock.Any(), &profilepb.GetUserByAuthProviderIDRequest{
			AuthProviderID: authProviderID,
		}).Times(2).
		Return(&profilepb.UserInfo{
			ID:         userPb,
			IsApproved: true,
		}, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:    userPb,
			OrgID: orgPb,
			IsApproved: &types.BoolValue{
				Value: false,
			},
		}).
		Return(nil, nil)

	mockProfile.EXPECT().
		UpdateUser(gomock.Any(), &profilepb.UpdateUserRequest{
			ID:             userPb,
			DisplayPicture: &types.StringValue{Value: "something"},
		}).
		Return(nil, nil)

	mockOrg.EXPECT().
		VerifyInviteToken(gomock.Any(), &profilepb.InviteToken{
			SignedClaims: "invite-token",
		}).
		Return(&profilepb.VerifyInviteTokenResponse{
			Valid: true,
			OrgID: utils.ProtoFromUUIDStrOrNil(orgID),
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

	resp, err := s.Login(getTestContext(), &authpb.LoginRequest{
		AccessToken:           "tokenabc",
		CreateUserIfNotExists: true,
		InviteToken:           "invite-token",
	})
	require.NoError(t, err)
	assert.Equal(t, orgID, resp.OrgInfo.OrgID)
}
