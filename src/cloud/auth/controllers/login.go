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

package controllers

import (
	"context"
	"fmt"
	"time"

	"github.com/gofrs/uuid"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/authcontext"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

const (
	// RefreshTokenValidDuration is duration that the refresh token is valid from current time.
	RefreshTokenValidDuration = 90 * 24 * time.Hour
	// AugmentedTokenValidDuration is the duration that the augmented token is valid from the current time.
	AugmentedTokenValidDuration = 30 * time.Minute
	// SupportAccountDomain is the domain name of the Pixie support account which can access the org provided at login.
	SupportAccountDomain = "pixie.support"
)

func (s *Server) getUserInfoFromToken(accessToken string) (string, *UserInfo, error) {
	if accessToken == "" {
		return "", nil, status.Error(codes.Unauthenticated, "missing access token")
	}

	userID, err := s.a.GetUserIDFromToken(accessToken)
	if err != nil {
		return "", nil, status.Error(codes.Unauthenticated, "failed to get user ID")
	}

	// Make request to get user info.
	userInfo, err := s.a.GetUserInfo(userID)
	if err != nil {
		return "", nil, status.Error(codes.Internal, "failed to get user info")
	}

	return userID, userInfo, nil
}

func (s *Server) updateAuthProviderUser(authUserID string, orgID string, userID string) (*UserInfo, error) {
	// Write user and org info to the AuthProvider.
	err := s.a.SetPLMetadata(authUserID, orgID, userID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to set user ID")
	}

	// Read updated user info.
	userInfo, err := s.a.GetUserInfo(authUserID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read updated user info")
	}

	return userInfo, nil
}

// Login uses the AuthProvider to authenticate and login the user. Errors out if their org doesn't exist.
func (s *Server) Login(ctx context.Context, in *authpb.LoginRequest) (*authpb.LoginReply, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	// If account is a Pixie support account, we don't want to create a new user.
	if domainName == SupportAccountDomain {
		accessEnabled := viper.GetBool("support_access_enabled")
		if !accessEnabled {
			return nil, status.Error(codes.PermissionDenied, "Support account does not have access credentials.")
		}
		return s.loginSupportUser(ctx, in, userInfo)
	} else if in.OrgName != "" {
		return nil, status.Error(codes.InvalidArgument, "orgName param not permitted for non Pixie support accounts")
	}

	pc := s.env.ProfileClient()

	// If user does not exist in the AuthProvider, then create a new user if specified.
	newUser := userInfo.PLUserID == ""

	// A user can exist in the AuthProvider, but not the profile service. If that's the case, we want to create a new user.
	if !newUser {
		upb := utils.ProtoFromUUIDStrOrNil(userInfo.PLUserID)
		_, err := pc.GetUser(ctx, upb)
		if err != nil {
			newUser = true
		}
	}

	// If we can't find the user and aren't in auto create mode.
	if newUser && !in.CreateUserIfNotExists {
		return nil, status.Error(codes.NotFound, "user not found, please register.")
	}

	hasOrg := userInfo.PLOrgID != ""
	var orgInfo *profilepb.OrgInfo
	// If the account has an org, use that instead of trying their domain.
	if hasOrg {
		orgPb := utils.ProtoFromUUIDStrOrNil(userInfo.PLOrgID)
		orgInfo, err = pc.GetOrg(ctx, orgPb)
		if err != nil {
			return nil, status.Errorf(codes.PermissionDenied, "org info from user is corrupted. Talk to administrator to fix this. %v", err)
		}
	} else {
		// Users can login without registering if their org already exists. If org doesn't exist, they must complete sign up flow.
		orgInfo, err = pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
		if err != nil || orgInfo == nil {
			return nil, status.Error(codes.NotFound, "organization not found, please register.")
		}
	}

	if newUser {
		userInfo, err = s.createUser(ctx, userID, userInfo, orgInfo)
		if err != nil {
			return nil, err
		}
	}

	// Update user's profile photo.
	upb := utils.ProtoFromUUIDStrOrNil(userInfo.PLUserID)
	_, err = pc.UpdateUser(ctx, &profilepb.UpdateUserRequest{ID: upb, ProfilePicture: userInfo.Picture})
	if err != nil {
		return nil, err
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	userID = userInfo.PLUserID

	return &authpb.LoginReply{
		Token:       token,
		ExpiresAt:   expiresAt.Unix(),
		UserCreated: newUser,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUIDStrOrNil(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgName: orgInfo.OrgName,
			OrgID:   utils.UUIDFromProtoOrNil(orgInfo.ID).String(),
		},
	}, nil
}

func (s *Server) loginSupportUser(ctx context.Context, in *authpb.LoginRequest, userInfo *UserInfo) (*authpb.LoginReply, error) {
	if in.OrgName == "" {
		return nil, status.Error(codes.InvalidArgument, "orgName is required for Pixie Support accounts")
	}

	pc := s.env.ProfileClient()

	orgInfo, err := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: in.OrgName})
	if err != nil || orgInfo == nil {
		return nil, status.Error(codes.InvalidArgument, "organization not found")
	}

	// Generate token for impersonated support account.
	userID := uuid.FromStringOrNil("") // No account actually exists, so this should be a nil UUID.
	orgID := utils.UUIDFromProtoOrNil(orgInfo.ID)
	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := srvutils.GenerateJWTForUser(userID.String(), orgID.String(), userInfo.Email, expiresAt, viper.GetString("domain_name"))
	token, err := srvutils.SignJWTClaims(claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &authpb.LoginReply{
		Token:       token,
		ExpiresAt:   expiresAt.Unix(),
		UserCreated: false,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUID(userID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgName: orgInfo.OrgName,
			OrgID:   orgID.String(),
		},
	}, nil
}

// Signup uses the AuthProvider to authenticate and sign up the user. It autocreates the org if the org doesn't exist.
func (s *Server) Signup(ctx context.Context, in *authpb.SignupRequest) (*authpb.SignupReply, error) {
	userID, userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	pc := s.env.ProfileClient()

	_, err = pc.GetUserByEmail(ctx, &profilepb.GetUserByEmailRequest{Email: userInfo.Email})
	if err == nil {
		return nil, status.Error(codes.PermissionDenied, "user already exists, please login.")
	}

	domainName, err := GetDomainNameFromEmail(userInfo.Email)
	if err != nil {
		return nil, services.HTTPStatusFromError(err, "Failed to get domain from email")
	}

	orgInfo, _ := pc.GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{DomainName: domainName})
	var orgID *uuidpb.UUID
	newOrg := orgInfo == nil
	if newOrg {
		userInfo, orgID, err = s.createUserAndOrg(ctx, domainName, userID, userInfo)
		if err != nil {
			return nil, err
		}
	} else {
		userInfo, err = s.createUser(ctx, userID, userInfo, orgInfo)
		orgID = orgInfo.ID
		if err != nil {
			return nil, err
		}
	}

	token, expiresAt, err := generateJWTTokenForUser(userInfo, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}

	return &authpb.SignupReply{
		Token:      token,
		ExpiresAt:  expiresAt.Unix(),
		OrgCreated: newOrg,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    utils.ProtoFromUUIDStrOrNil(userInfo.PLUserID),
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgID: orgID,
	}, nil
}

// Creates a user as well as an org if the orgInfo passed in is nil.
func (s *Server) createUserAndOrg(ctx context.Context, domainName string, userID string, userInfo *UserInfo) (*UserInfo, *uuidpb.UUID, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	orgName := domainName
	rpcReq := &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    orgName,
			DomainName: domainName,
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			Username:  userInfo.Email,
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
	}

	pc := s.env.ProfileClient()
	resp, err := pc.CreateOrgAndUser(ctx, rpcReq)
	if err != nil {
		return nil, nil, status.Error(codes.Internal, fmt.Sprintf("failed to create user/org: %v", err))
	}
	orgIDpb := resp.OrgID
	userIDpb := resp.UserID

	userInfo, err = s.updateAuthProviderUser(userID, utils.UUIDFromProtoOrNil(orgIDpb).String(), utils.UUIDFromProtoOrNil(userIDpb).String())
	return userInfo, orgIDpb, err
}

// Creates a user in the passed in orgname.
func (s *Server) createUser(ctx context.Context, userID string, userInfo *UserInfo, orgInfo *profilepb.OrgInfo) (*UserInfo, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	if orgInfo == nil {
		return nil, fmt.Errorf("orgInfo should not be nil")
	}
	// Create a new user to register them.
	userCreateReq := &profilepb.CreateUserRequest{
		OrgID:     orgInfo.ID,
		Username:  userInfo.Email,
		FirstName: userInfo.FirstName,
		LastName:  userInfo.LastName,
		Email:     userInfo.Email,
	}

	userIDpb, err := s.env.ProfileClient().CreateUser(ctx, userCreateReq)
	if err != nil {
		return nil, err
	}
	userInfo, err = s.updateAuthProviderUser(userID, utils.UUIDFromProtoOrNil(orgInfo.ID).String(), utils.UUIDFromProtoOrNil(userIDpb).String())
	return userInfo, err
}

// GetAugmentedTokenForAPIKey produces an augmented token for the user given a API key.
func (s *Server) GetAugmentedTokenForAPIKey(ctx context.Context, in *authpb.GetAugmentedTokenForAPIKeyRequest) (*authpb.GetAugmentedTokenForAPIKeyResponse, error) {
	// Find the org/user associated with the token.
	orgID, userID, err := s.apiKeyMgr.FetchOrgUserIDUsingAPIKey(ctx, in.APIKey)
	if err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Invalid API key")
	}

	// Generate service token, so that we can make a call to the Profile service.
	svcJWT := srvutils.GenerateJWTForService("AuthService", viper.GetString("domain_name"))
	svcClaims, err := srvutils.SignJWTClaims(svcJWT, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}
	ctxWithSvcCreds := metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", svcClaims))

	// Fetch user's email.
	pc := s.env.ProfileClient()
	user, err := pc.GetUser(ctxWithSvcCreds, utils.ProtoFromUUID(userID))
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}

	// Create JWT for user/org.
	claims := srvutils.GenerateJWTForUser(userID.String(), orgID.String(), user.Email, time.Now().Add(AugmentedTokenValidDuration), viper.GetString("domain_name"))
	token, err := srvutils.SignJWTClaims(claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}

	resp := &authpb.GetAugmentedTokenForAPIKeyResponse{
		Token:     token,
		ExpiresAt: claims.ExpiresAt,
	}
	return resp, nil
}

// GetAugmentedToken produces augmented tokens for the user based on passed in credentials.
func (s *Server) GetAugmentedToken(
	ctx context.Context, in *authpb.GetAugmentedAuthTokenRequest) (
	*authpb.GetAugmentedAuthTokenResponse, error) {
	// Check the incoming token and make sure it's valid.
	aCtx := authcontext.New()

	if err := aCtx.UseJWTAuth(s.env.JWTSigningKey(), in.Token, viper.GetString("domain_name")); err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Invalid auth token")
	}

	if !aCtx.ValidClaims() {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
	}

	// We perform extra checks for user tokens.
	if srvutils.GetClaimsType(aCtx.Claims) == srvutils.UserClaimType {
		// Check to make sure that the org and user exist in the system.
		pc := s.env.ProfileClient()

		md, _ := metadata.FromIncomingContext(ctx)
		ctx = metadata.NewOutgoingContext(ctx, md)

		orgIDstr := aCtx.Claims.GetUserClaims().OrgID
		_, err := pc.GetOrg(ctx, utils.ProtoFromUUIDStrOrNil(orgIDstr))
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "Invalid auth/org")
		}

		domainName, err := GetDomainNameFromEmail(aCtx.Claims.GetUserClaims().Email)
		if err != nil {
			return nil, status.Error(codes.Unauthenticated, "Invalid email")
		}

		if domainName != SupportAccountDomain {
			userIDstr := aCtx.Claims.GetUserClaims().UserID
			userInfo, err := pc.GetUser(ctx, utils.ProtoFromUUIDStrOrNil(userIDstr))
			if err != nil || userInfo == nil {
				return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
			}

			if orgIDstr != utils.UUIDFromProtoOrNil(userInfo.OrgID).String() {
				return nil, status.Error(codes.Unauthenticated, "Mismatched org")
			}
		}
	}

	// TODO(zasgar): This step should be to generate a new token base on what we get from a database.
	claims := *aCtx.Claims
	claims.IssuedAt = time.Now().Unix()
	claims.ExpiresAt = time.Now().Add(AugmentedTokenValidDuration).Unix()

	augmentedToken, err := srvutils.SignJWTClaims(&claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate auth token")
	}
	// TODO(zasgar): This should actually do ACL's, etc. at some point.
	// Generate a new augmented auth token with additional information.
	resp := &authpb.GetAugmentedAuthTokenResponse{
		Token:     augmentedToken,
		ExpiresAt: claims.ExpiresAt,
	}
	return resp, nil
}

func generateJWTTokenForUser(userInfo *UserInfo, signingKey string) (string, time.Time, error) {
	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := srvutils.GenerateJWTForUser(userInfo.PLUserID, userInfo.PLOrgID, userInfo.Email, expiresAt, viper.GetString("domain_name"))
	token, err := srvutils.SignJWTClaims(claims, signingKey)

	return token, expiresAt, err
}
