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
	"regexp"
	"strings"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/spf13/viper"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/authcontext"
	srvutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

const (
	// RefreshTokenValidDuration is duration that the refresh token is valid from current time.
	RefreshTokenValidDuration = 90 * 24 * time.Hour
	// AugmentedTokenValidDuration is the duration that the augmented token is valid from the current time.
	AugmentedTokenValidDuration = 90 * time.Minute
	// AuthConnectorTokenValidDuration is the duration that the auth connector token is valid from the current time.
	AuthConnectorTokenValidDuration = 30 * time.Minute
)

func (s *Server) getUserInfoFromToken(accessToken string) (*UserInfo, error) {
	if accessToken == "" {
		return nil, status.Error(codes.Unauthenticated, "missing access token")
	}

	// Make request to get user info.
	userInfo, err := s.a.GetUserInfoFromAccessToken(accessToken)
	if err != nil {
		return nil, status.Error(codes.Unauthenticated, "failed to get user info")
	}

	return userInfo, nil
}

func (s *Server) getUser(ctx context.Context, userInfo *UserInfo) (*profilepb.UserInfo, error) {
	return s.env.ProfileClient().GetUserByAuthProviderID(ctx, &profilepb.GetUserByAuthProviderIDRequest{AuthProviderID: userInfo.AuthProviderID})
}

func (s *Server) reconcileOrgAndLogin(ctx context.Context, userInfo *UserInfo, user *profilepb.UserInfo, orgInfo *profilepb.OrgInfo) (*authpb.LoginReply, error) {
	if user.OrgID == nil {
		// Add user to org.
		_, err := s.env.ProfileClient().UpdateUser(ctx, &profilepb.UpdateUserRequest{
			ID:    user.ID,
			OrgID: orgInfo.ID,
			IsApproved: &types.BoolValue{
				// User should only be auto-approved if the org doesn't require
				// approvals (EnableApprovals = false).
				Value: !orgInfo.EnableApprovals,
			},
		})
		if err != nil {
			return nil, err
		}
		return s.loginUser(ctx, userInfo, orgInfo, false)
	}

	if !utils.AreSameUUID(user.OrgID, orgInfo.ID) {
		return nil, status.Errorf(codes.PermissionDenied, "Our system found an issue with your account. Please contact support and include your email '%s' and this error in your message", userInfo.Email)
	}

	return s.loginUser(ctx, userInfo, orgInfo, false)
}

// Login uses the AuthProvider to authenticate and login the user. Errors out if their org doesn't exist.
func (s *Server) Login(ctx context.Context, in *authpb.LoginRequest) (*authpb.LoginReply, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	inviteOrgID, err := s.getInviteOrgID(ctx, in.InviteToken)
	if err != nil {
		return nil, err
	}

	userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}
	if userInfo.HostedDomain != "" && !utils.IsNilUUIDProto(inviteOrgID) {
		return nil, status.Error(codes.PermissionDenied, "This email is managed by Google Workspace and can only be used to join its associated Google Workspace managed org. Please retry the invite link using another email.")
	}

	user, err := s.getUser(ctx, userInfo)
	// If we can't find the user and aren't in auto create mode.
	if (err != nil || user == nil) && !in.CreateUserIfNotExists {
		return nil, status.Error(codes.NotFound, "user not found, please register.")
	}
	if user != nil && !utils.IsNilUUIDProto(inviteOrgID) && !utils.IsNilUUIDProto(user.OrgID) {
		return nil, status.Error(codes.PermissionDenied, "cannot join org - user already belongs to another org")
	}

	if !utils.IsNilUUIDProto(inviteOrgID) {
		return s.loginInvited(ctx, userInfo, user, inviteOrgID)
	}

	if userInfo.HostedDomain != "" {
		return s.loginHostedDomain(ctx, userInfo, user)
	}

	return s.loginSimple(ctx, userInfo, user)
}

func (s *Server) loginInvited(ctx context.Context, userInfo *UserInfo, user *profilepb.UserInfo, inviteOrgID *uuidpb.UUID) (*authpb.LoginReply, error) {
	newUser := user == nil
	orgInfo, err := s.env.OrgClient().GetOrg(ctx, inviteOrgID)
	if err != nil {
		return nil, err
	}

	if newUser {
		return s.loginUser(ctx, userInfo, orgInfo, newUser)
	}

	return s.reconcileOrgAndLogin(ctx, userInfo, user, orgInfo)
}

func (s *Server) loginHostedDomain(ctx context.Context, userInfo *UserInfo, user *profilepb.UserInfo) (*authpb.LoginReply, error) {
	newUser := user == nil
	orgInfo, err := s.getMatchingOrgForUser(ctx, userInfo)

	if status.Code(err) == codes.NotFound {
		if newUser {
			return nil, status.Error(codes.NotFound, "organization not found, please register.")
		}
		return nil, status.Error(codes.NotFound, "matching org for existing user does not exist, contact support")
	}
	if err != nil {
		return nil, err
	}

	if newUser {
		return s.loginUser(ctx, userInfo, orgInfo, newUser)
	}

	return s.reconcileOrgAndLogin(ctx, userInfo, user, orgInfo)
}

// loginSimple is the fallback login system. This handles users that are neither invited to a specific org
// nor in a Google HostedDomain org.
func (s *Server) loginSimple(ctx context.Context, userInfo *UserInfo, user *profilepb.UserInfo) (*authpb.LoginReply, error) {
	newUser := user == nil

	var orgInfo *profilepb.OrgInfo
	var err error

	if !newUser && !utils.IsNilUUIDProto(user.OrgID) {
		orgInfo, err = s.env.OrgClient().GetOrg(ctx, user.OrgID)
		if status.Code(err) == codes.NotFound {
			return nil, status.Error(codes.NotFound, "organization ID does not exist, please contact support")
		}
		if err != nil {
			return nil, status.Errorf(codes.Internal, "%v", err)
		}
	}

	// We've switched over to use HostedDomain instead of email domain to determine org membership.
	// Some running systems have users who are in their email domain org, but not their org according to HostedDomain.
	// This flags those users and informs them that they should contact support to fix their org info in the database.
	if orgInfo != nil && userInfo.Email != orgInfo.OrgName && orgInfo.DomainName == nil {
		return nil, status.Errorf(codes.PermissionDenied, "Our system found an issue with your account. Please contact support and include your email '%s' and this error in your message", userInfo.Email)
	}
	return s.loginUser(ctx, userInfo, orgInfo, newUser)
}

func (s *Server) loginUser(ctx context.Context, userInfo *UserInfo, orgInfo *profilepb.OrgInfo, newUser bool) (*authpb.LoginReply, error) {
	var orgName string
	var orgID *uuidpb.UUID
	var orgIDStr string
	if orgInfo != nil {
		orgID = orgInfo.ID
		orgIDStr = utils.UUIDFromProtoOrNil(orgID).String()
		orgName = orgInfo.OrgName
	}
	var err error
	if newUser {
		userInfo, err = s.createUser(ctx, userInfo, orgID)
		if err != nil {
			return nil, err
		}
	}
	if orgID != nil {
		_, _ = s.env.OrgClient().UpdateOrg(ctx, &profilepb.UpdateOrgRequest{
			ID:         orgID,
			DomainName: &types.StringValue{Value: userInfo.HostedDomain},
		})
	}
	tkn, err := s.completeUserLogin(ctx, userInfo, orgInfo)
	if err != nil {
		return nil, err
	}

	return &authpb.LoginReply{
		Token:       tkn.token,
		ExpiresAt:   tkn.expiresAt.Unix(),
		UserCreated: newUser,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    tkn.id,
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgInfo: &authpb.LoginReply_OrgInfo{
			OrgName: orgName,
			OrgID:   orgIDStr,
		},
		IdentityProvider: userInfo.IdentityProvider,
	}, nil
}

type tokenData struct {
	id        *uuidpb.UUID
	token     string
	expiresAt time.Time
}

// completeUserLogin does the final login steps and generates a token for the user.
func (s *Server) completeUserLogin(ctx context.Context, userInfo *UserInfo, orgInfo *profilepb.OrgInfo) (*tokenData, error) {
	pc := s.env.ProfileClient()
	var orgID string
	if orgInfo != nil {
		orgID = utils.ProtoToUUIDStr(orgInfo.ID)
	}

	if !userInfo.EmailVerified {
		return nil, status.Error(codes.PermissionDenied, "please verify your email before proceeding")
	}
	user, err := s.getUser(ctx, userInfo)
	if err != nil {
		return nil, status.Error(codes.Internal, err.Error())
	}

	// Check to make sure the user is approved to login. They are default approved
	// if the org does not EnableApprovals.
	if orgInfo != nil && orgInfo.EnableApprovals {
		if !user.IsApproved {
			return nil, status.Error(codes.PermissionDenied, "You are not approved to log in to the org. Please request approval from your org admin")
		}
	}

	// Update user's profile photo.
	_, err = pc.UpdateUser(ctx, &profilepb.UpdateUserRequest{
		ID:             user.ID,
		DisplayPicture: &types.StringValue{Value: userInfo.Picture},
	})
	if err != nil {
		return nil, err
	}

	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := srvutils.GenerateJWTForUser(utils.ProtoToUUIDStr(user.ID), orgID, userInfo.Email, expiresAt, viper.GetString("domain_name"))
	tkn, err := srvutils.SignJWTClaims(claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}
	return &tokenData{
		id:        user.ID,
		token:     tkn,
		expiresAt: expiresAt,
	}, nil
}

func (s *Server) signupUser(ctx context.Context, userInfo *UserInfo, orgInfo *profilepb.OrgInfo, newOrg bool) (*authpb.SignupReply, error) {
	tkn, err := s.completeUserLogin(ctx, userInfo, orgInfo)
	if err != nil {
		return nil, err
	}
	var orgID *uuidpb.UUID
	var orgName string
	if orgInfo != nil {
		orgID = orgInfo.ID
		orgName = orgInfo.OrgName
	}

	return &authpb.SignupReply{
		Token:      tkn.token,
		ExpiresAt:  tkn.expiresAt.Unix(),
		OrgCreated: newOrg,
		UserInfo: &authpb.AuthenticatedUserInfo{
			UserID:    tkn.id,
			FirstName: userInfo.FirstName,
			LastName:  userInfo.LastName,
			Email:     userInfo.Email,
		},
		OrgID:            orgID,
		OrgName:          orgName,
		IdentityProvider: userInfo.IdentityProvider,
	}, nil
}

func (s *Server) getMatchingOrgForUser(ctx context.Context, userInfo *UserInfo) (*profilepb.OrgInfo, error) {
	if userInfo.HostedDomain == "" {
		return s.env.OrgClient().GetOrgByName(ctx, &profilepb.GetOrgByNameRequest{Name: userInfo.Email})
	}

	// Case 1: Search for an org that has a domain matching the HostedDomain.
	orgInfo, err := s.env.OrgClient().GetOrgByDomain(ctx, &profilepb.GetOrgByDomainRequest{
		DomainName: userInfo.HostedDomain,
	})
	if err == nil || status.Code(err) != codes.NotFound {
		return orgInfo, err
	}

	// Case 2: Search for an org that matches the email domain.
	// Users who have an HostedDomain might be able to join a matching domainName org.
	emailComponents := strings.Split(userInfo.Email, "@")
	if len(emailComponents) != 2 {
		return nil, status.Error(codes.InvalidArgument, "bad format email received from auth")
	}
	emailDomain := emailComponents[1]

	return s.env.OrgClient().GetOrgByName(ctx, &profilepb.GetOrgByNameRequest{Name: emailDomain})
}

func (s *Server) getInviteOrgID(ctx context.Context, inviteToken string) (*uuidpb.UUID, error) {
	if inviteToken == "" {
		return nil, nil
	}
	verifiedToken, err := s.env.OrgClient().VerifyInviteToken(ctx, &profilepb.InviteToken{
		SignedClaims: inviteToken,
	})
	if err != nil {
		return nil, err
	}
	if !verifiedToken.Valid {
		return nil, status.Error(codes.PermissionDenied, "received invalid or expired invite")
	}
	return verifiedToken.OrgID, nil
}

// Signup uses the AuthProvider to authenticate and sign up the user. It autocreates the org if the org doesn't exist.
func (s *Server) Signup(ctx context.Context, in *authpb.SignupRequest) (*authpb.SignupReply, error) {
	inviteOrgID, err := s.getInviteOrgID(ctx, in.InviteToken)
	if err != nil {
		return nil, err
	}

	userInfo, err := s.getUserInfoFromToken(in.AccessToken)
	if err != nil {
		return nil, err
	}

	if userInfo.HostedDomain != "" && !utils.IsNilUUIDProto(inviteOrgID) {
		return nil, status.Error(codes.PermissionDenied, "gsuite users are not allowed to follow invites. Please join the org with another account")
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	_, err = s.getUser(ctx, userInfo)
	if err == nil {
		return nil, status.Error(codes.AlreadyExists, "user already exists, please login.")
	}

	// Case 1: User was invited to join an organization.
	if !utils.IsNilUUIDProto(inviteOrgID) {
		orgInfoPb, err := s.env.OrgClient().GetOrg(ctx, inviteOrgID)
		if err != nil {
			return nil, status.Errorf(codes.Internal, err.Error())
		}
		if orgInfoPb == nil {
			return nil, status.Errorf(codes.InvalidArgument, "misformatted invite link")
		}
		updatedUserInfo, err := s.createUser(ctx, userInfo, inviteOrgID)
		if err != nil {
			return nil, err
		}
		return s.signupUser(ctx, updatedUserInfo, orgInfoPb, false /* newOrg */)
	}

	// Case 2: An empty HostedDomain means this user will be created without an org.
	if userInfo.HostedDomain == "" {
		updatedUserInfo, err := s.createUser(ctx, userInfo, nil)
		if err != nil {
			return nil, err
		}
		return s.signupUser(ctx, updatedUserInfo, nil, false /* newOrg */)
	}

	// Case 3: We go through all permutations of orgs that might exist for a user and find any that exist.
	orgInfo, err := s.getMatchingOrgForUser(ctx, userInfo)
	if err != nil && status.Code(err) != codes.NotFound {
		return nil, err
	}
	if orgInfo != nil {
		updatedUserInfo, err := s.createUser(ctx, userInfo, orgInfo.ID)
		if err != nil {
			return nil, err
		}
		return s.signupUser(ctx, updatedUserInfo, orgInfo, false /* newOrg */)
	}

	// Final case: User is the first to join and their org will be created with them.
	// Note HostedDomain will never be empty because of case 2.
	updatedUserInfo, orgID, err := s.createUserAndOrg(ctx, userInfo.HostedDomain, userInfo.HostedDomain, userInfo)
	if err != nil {
		return nil, err
	}
	newOrgInfo, err := s.env.OrgClient().GetOrg(ctx, orgID)
	if err != nil {
		return nil, err
	}
	return s.signupUser(ctx, updatedUserInfo, newOrgInfo, true /* newOrg */)
}

// Creates a user as well as an org.
func (s *Server) createUserAndOrg(ctx context.Context, domainName string, orgName string, userInfo *UserInfo) (*UserInfo, *uuidpb.UUID, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	rpcReq := &profilepb.CreateOrgAndUserRequest{
		Org: &profilepb.CreateOrgAndUserRequest_Org{
			OrgName:    orgName,
			DomainName: domainName,
		},
		User: &profilepb.CreateOrgAndUserRequest_User{
			FirstName:        userInfo.FirstName,
			LastName:         userInfo.LastName,
			Email:            userInfo.Email,
			IdentityProvider: userInfo.IdentityProvider,
			AuthProviderID:   userInfo.AuthProviderID,
		},
	}

	pc := s.env.ProfileClient()
	resp, err := pc.CreateOrgAndUser(ctx, rpcReq)
	if err != nil {
		return nil, nil, status.Error(codes.Internal, fmt.Sprintf("failed to create user/org: %v", err))
	}
	orgIDpb := resp.OrgID

	return userInfo, orgIDpb, nil
}

// Creates a user for the orgID.
func (s *Server) createUser(ctx context.Context, userInfo *UserInfo, orgID *uuidpb.UUID) (*UserInfo, error) {
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	// Create a new user to register them.
	userCreateReq := &profilepb.CreateUserRequest{
		OrgID:            orgID,
		FirstName:        userInfo.FirstName,
		LastName:         userInfo.LastName,
		Email:            userInfo.Email,
		IdentityProvider: userInfo.IdentityProvider,
		AuthProviderID:   userInfo.AuthProviderID,
	}

	_, err := s.env.ProfileClient().CreateUser(ctx, userCreateReq)
	if err != nil {
		return nil, err
	}

	return userInfo, nil
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

	// Fetch org to validate it exists.
	_, err = s.env.OrgClient().GetOrg(ctxWithSvcCreds, utils.ProtoFromUUID(orgID))
	if err != nil {
		return nil, status.Errorf(codes.Internal, "Failed to generate auth token")
	}

	// Create JWT for user/org.
	claims := srvutils.GenerateJWTForAPIUser(userID.String(), orgID.String(), time.Now().Add(AugmentedTokenValidDuration), viper.GetString("domain_name"))
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

		// If the OrgID is empty, we will approve the user but they will have low
		// functionality in Pixie.
		orgIDstr := aCtx.Claims.GetUserClaims().OrgID
		if uuid.FromStringOrNil(orgIDstr) != uuid.Nil {
			_, err := s.env.OrgClient().GetOrg(ctx, utils.ProtoFromUUIDStrOrNil(orgIDstr))
			if err != nil {
				return nil, status.Error(codes.Unauthenticated, "Invalid auth/org")
			}
		}

		if !aCtx.Claims.GetUserClaims().IsAPIUser {
			userIDstr := aCtx.Claims.GetUserClaims().UserID
			userInfo, err := pc.GetUser(ctx, utils.ProtoFromUUIDStrOrNil(userIDstr))
			if err != nil || userInfo == nil {
				return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
			}

			if uuid.FromStringOrNil(orgIDstr) != utils.UUIDFromProtoOrNil(userInfo.OrgID) {
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

func (s *Server) createInvitedUser(ctx context.Context, req *authpb.InviteUserRequest) (*UserInfo, error) {
	// Create the Identity in the AuthProvider.
	ident, err := s.a.CreateIdentity(req.Email)
	if err != nil {
		return nil, fmt.Errorf("error while creating identity for '%s': %v", req.Email, err)
	}

	// Create the user inside of Pixie.
	userInfo, err := s.createUser(ctx, &UserInfo{
		Email:            req.Email,
		FirstName:        req.FirstName,
		LastName:         req.LastName,
		IdentityProvider: ident.IdentityProvider,
		AuthProviderID:   ident.AuthProviderID,
	}, req.OrgID)

	if err != nil {
		return nil, err
	}
	user, err := s.getUser(ctx, userInfo)
	if err != nil {
		return nil, err
	}

	// Auto-approve user.
	_, err = s.env.ProfileClient().UpdateUser(ctx, &profilepb.UpdateUserRequest{
		ID: user.ID,
		IsApproved: &types.BoolValue{
			Value: true,
		},
	})
	if err != nil {
		return nil, err
	}
	return userInfo, nil
}

var noSuchUserMatcher = regexp.MustCompile("no such user")

// InviteUser creates an invite link for the specified user.
func (s *Server) InviteUser(ctx context.Context, req *authpb.InviteUserRequest) (*authpb.InviteUserResponse, error) {
	var authProviderID string
	// Try to look up the user.
	pc := s.env.ProfileClient()
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)
	userPb, err := pc.GetUserByEmail(ctx, &profilepb.GetUserByEmailRequest{Email: req.Email})
	if err == nil {
		authProviderID = userPb.AuthProviderID
	} else if err != nil && noSuchUserMatcher.MatchString(err.Error()) {
		// Create a user if no user found.
		user, err := s.createInvitedUser(ctx, req)
		if err != nil {
			return nil, err
		}
		authProviderID = user.AuthProviderID
	} else if err != nil {
		return nil, err
	}
	// Create invite link for the user.
	resp, err := s.a.CreateInviteLink(authProviderID)
	if err != nil {
		return nil, err
	}

	return &authpb.InviteUserResponse{
		InviteLink: resp.InviteLink,
	}, nil
}

// CreateOrgAndInviteUser creates an org and user, then returns an invite link for the user to set that user's password.
func (s *Server) CreateOrgAndInviteUser(ctx context.Context, req *authpb.CreateOrgAndInviteUserRequest) (*authpb.CreateOrgAndInviteUserResponse, error) {
	// Update context with auth.
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	ident, err := s.a.CreateIdentity(req.User.Email)
	if err != nil {
		return nil, fmt.Errorf("error while creating identity for '%s': %v", req.User.Email, err)
	}

	_, _, err = s.createUserAndOrg(ctx, req.Org.DomainName, req.Org.OrgName, &UserInfo{
		Email:            req.User.Email,
		FirstName:        req.User.FirstName,
		LastName:         req.User.LastName,
		IdentityProvider: ident.IdentityProvider,
		AuthProviderID:   ident.AuthProviderID,
	})

	if err != nil {
		return nil, fmt.Errorf("unable to create org and user: %v", err)
	}
	// Create invite link for the user.
	resp, err := s.a.CreateInviteLink(ident.AuthProviderID)
	if err != nil {
		return nil, err
	}

	return &authpb.CreateOrgAndInviteUserResponse{
		InviteLink: resp.InviteLink,
	}, nil
}

// GetAuthConnectorToken uses the AuthProvider to generate a short-lived token that can be used to authenticate as a user.
func (s *Server) GetAuthConnectorToken(ctx context.Context, req *authpb.GetAuthConnectorTokenRequest) (*authpb.GetAuthConnectorTokenResponse, error) {
	// Find the org/user currently logged in.
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	if sCtx.Claims.GetUserClaims() == nil {
		return nil, fmt.Errorf("unable to create authConnector token for invalid user")
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	// Fetch userInfo that needs to be included in token.
	pc := s.env.ProfileClient()
	userInfo, err := pc.GetUser(ctx, utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID))
	if err != nil {
		return nil, fmt.Errorf("unable to create authConnector token for invalid user")
	}

	// Create access token.
	now := time.Now()
	expiresAt := now.Add(AuthConnectorTokenValidDuration)
	claims := srvutils.GenerateJWTForUser(utils.UUIDFromProtoOrNil(userInfo.ID).String(), utils.UUIDFromProtoOrNil(userInfo.OrgID).String(), userInfo.Email, expiresAt, viper.GetString("domain_name"))
	token, err := srvutils.ProtoToToken(claims)
	if err != nil {
		return nil, fmt.Errorf("unable to create authConnector token")
	}

	// Add custom fields for auth connector token.
	err = token.Set("ClusterName", req.ClusterName)
	if err != nil {
		return nil, fmt.Errorf("unable to create authConnector token")
	}

	signed, err := srvutils.SignToken(token, s.env.JWTSigningKey())
	if err != nil {
		return nil, fmt.Errorf("unable to sign authConnector token")
	}

	return &authpb.GetAuthConnectorTokenResponse{
		Token:     string(signed),
		ExpiresAt: expiresAt.Unix(),
	}, nil
}

// RefetchToken takes in a valid token updates the claims with new data then returns a new token.
func (s *Server) RefetchToken(ctx context.Context, in *authpb.RefetchTokenRequest) (*authpb.RefetchTokenResponse, error) {
	// Validate the passed in token.
	aCtx := authcontext.New()
	if err := aCtx.UseJWTAuth(s.env.JWTSigningKey(), in.Token, viper.GetString("domain_name")); err != nil {
		return nil, status.Errorf(codes.Unauthenticated, "Invalid auth token")
	}
	if !aCtx.ValidClaims() {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
	}
	if srvutils.GetClaimsType(aCtx.Claims) != srvutils.UserClaimType {
		return nil, status.Error(codes.InvalidArgument, "can only call on user claims")
	}

	// Populate the new claims with up to date user info.
	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	user, err := s.env.ProfileClient().GetUser(ctx, utils.ProtoFromUUIDStrOrNil(aCtx.Claims.GetUserClaims().UserID))
	if err != nil || user == nil {
		return nil, status.Error(codes.Unauthenticated, "Invalid auth/user")
	}
	orgIDStr := ""
	if !utils.IsNilUUIDProto(user.OrgID) {
		orgIDStr = utils.UUIDFromProtoOrNil(user.OrgID).String()
	}

	expiresAt := time.Now().Add(RefreshTokenValidDuration)
	claims := srvutils.GenerateJWTForUser(
		utils.UUIDFromProtoOrNil(user.ID).String(),
		orgIDStr,
		user.Email,
		expiresAt,
		viper.GetString("domain_name"),
	)
	tkn, err := srvutils.SignJWTClaims(claims, s.env.JWTSigningKey())
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to generate token")
	}
	return &authpb.RefetchTokenResponse{
		Token:     tkn,
		ExpiresAt: expiresAt.Unix(),
	}, nil
}
