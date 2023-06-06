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
	"errors"
	"fmt"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/types"
	"github.com/lestrrat-go/jwx/jwa"
	"github.com/lestrrat-go/jwx/jwk"
	"github.com/lestrrat-go/jwx/jwt"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/profileenv"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	"px.dev/pixie/src/shared/services/authcontext"
	claimsutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

// DefaultProjectName is the name of the default project we automatically assign to every org.
const DefaultProjectName string = "default"

// UserDatastore is the interface used to the backing store for user profile information.
type UserDatastore interface {
	// CreateUser creates a new user.
	CreateUser(*datastore.UserInfo) (uuid.UUID, error)
	// GetUser gets a user by ID.
	GetUser(uuid.UUID) (*datastore.UserInfo, error)
	// GetUserByEmail gets a user by email.
	GetUserByEmail(string) (*datastore.UserInfo, error)
	// GetUserByAuthProviderID returns the user that matches the AuthProviderID.
	GetUserByAuthProviderID(string) (*datastore.UserInfo, error)
	// CreateUserAndOrg creates a user and org for creating a new org with specified user as owner.
	CreateUserAndOrg(*datastore.OrgInfo, *datastore.UserInfo) (orgID uuid.UUID, userID uuid.UUID, err error)
	// UpdateUser updates the user info.
	UpdateUser(*datastore.UserInfo) error
	// DeleteUser deletes the user.
	DeleteUser(uuid.UUID) error
}

// OrgDatastore is the interface used as the backing store for org information.
type OrgDatastore interface {
	// ApproveAllOrgUsers sets is_approved for all users.
	ApproveAllOrgUsers(uuid.UUID) error
	// UpdateOrg updates the orgs info.
	UpdateOrg(*datastore.OrgInfo) error
	// CreateOrg creates a new org.
	CreateOrg(*datastore.OrgInfo) (uuid.UUID, error)
	// GetOrgs gets all the orgs.
	GetOrgs() ([]*datastore.OrgInfo, error)
	// GetUsersInOrg gets all of the users in the given org.
	GetUsersInOrg(uuid.UUID) ([]*datastore.UserInfo, error)
	// NumUsersInOrg gets the count of users in the given org.
	NumUsersInOrg(uuid.UUID) (int, error)
	// GetOrg gets and org by ID.
	GetOrg(uuid.UUID) (*datastore.OrgInfo, error)
	// GetOrgByName gets an org by name.
	GetOrgByName(string) (*datastore.OrgInfo, error)
	// GetOrgByDomain gets an org by domain name.
	GetOrgByDomain(string) (*datastore.OrgInfo, error)
	// Delete Org and all of its users
	DeleteOrgAndUsers(uuid.UUID) error
	// GetInviteSigningKey gets the invite signing key for the given orgID.
	GetInviteSigningKey(uuid.UUID) (string, error)
	// CreateInviteSigningKey creates an invite signing key for the given orgID.
	CreateInviteSigningKey(uuid.UUID) (string, error)
}

// UserSettingsDatastore is the interface used to the backing store for user settings.
type UserSettingsDatastore interface {
	// GetUserSettings gets the user settings for the given user and keys.
	GetUserSettings(uuid.UUID) (*datastore.UserSettings, error)
	// UpdateUserSettings updates the keys and values for the given user.
	UpdateUserSettings(*datastore.UserSettings) error
	// GetUserAttributes gets the attributes for the given user.
	GetUserAttributes(uuid.UUID) (*datastore.UserAttributes, error)
	// SetUserAttributes sets the attributes for the given user.
	SetUserAttributes(*datastore.UserAttributes) error
}

// OrgSettingsDatastore is the interface used as the backing store for org settings.
// This includes IDE configs and various other settings that users can configure for orgs.
type OrgSettingsDatastore interface {
	// AddIDEConfig adds the IDE config to the org.
	AddIDEConfig(uuid.UUID, *datastore.IDEConfig) error
	// DeleteIDEConfig deletes the IDE config from the org.
	DeleteIDEConfig(uuid.UUID, string) error
	// GetIDEConfigs gets all IDE configs for the org.
	GetIDEConfigs(uuid.UUID) ([]*datastore.IDEConfig, error)
	// GetIDEConfig gets the IDE config for the IDE with the given name.
	GetIDEConfig(uuid.UUID, string) (*datastore.IDEConfig, error)
}

// Server is an implementation of GRPC server for profile service.
type Server struct {
	env  profileenv.ProfileEnv
	uds  UserDatastore
	usds UserSettingsDatastore
	ods  OrgDatastore
	osds OrgSettingsDatastore
}

// NewServer creates a new GRPC profile server.
func NewServer(env profileenv.ProfileEnv, uds UserDatastore, usds UserSettingsDatastore, ods OrgDatastore, osds OrgSettingsDatastore) *Server {
	return &Server{env: env, uds: uds, usds: usds, ods: ods, osds: osds}
}

func userInfoToProto(u *datastore.UserInfo) *profilepb.UserInfo {
	profilePicture := ""
	if u.ProfilePicture != nil {
		profilePicture = *u.ProfilePicture
	}
	var orgID *uuidpb.UUID
	if u.OrgID != nil {
		orgID = utils.ProtoFromUUID(*u.OrgID)
	}
	return &profilepb.UserInfo{
		ID:               utils.ProtoFromUUID(u.ID),
		OrgID:            orgID,
		FirstName:        u.FirstName,
		LastName:         u.LastName,
		Email:            u.Email,
		ProfilePicture:   profilePicture,
		IsApproved:       u.IsApproved,
		IdentityProvider: u.IdentityProvider,
		AuthProviderID:   u.AuthProviderID,
	}
}

func orgInfoToProto(o *datastore.OrgInfo) *profilepb.OrgInfo {
	var domainName *types.StringValue
	if o.DomainName != nil {
		domainName = &types.StringValue{Value: o.GetDomainName()}
	}
	return &profilepb.OrgInfo{
		ID:              utils.ProtoFromUUID(o.ID),
		OrgName:         o.OrgName,
		DomainName:      domainName,
		EnableApprovals: o.EnableApprovals,
	}
}

func toExternalError(err error) error {
	if err == datastore.ErrOrgNotFound {
		return status.Error(codes.NotFound, "no such org")
	} else if err == datastore.ErrUserNotFound {
		return status.Error(codes.NotFound, "no such user")
	}
	return err
}

// CreateUser is the GRPC method to create  new user.
func (s *Server) CreateUser(ctx context.Context, req *profilepb.CreateUserRequest) (*uuidpb.UUID, error) {
	// Users with no org are considered approved by default.
	userInfo := &datastore.UserInfo{
		FirstName:        req.FirstName,
		LastName:         req.LastName,
		Email:            req.Email,
		IsApproved:       true,
		IdentityProvider: req.IdentityProvider,
		AuthProviderID:   req.AuthProviderID,
	}
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if orgID != uuid.Nil {
		orgInfo, err := s.ods.GetOrg(orgID)
		if err != nil {
			return nil, status.Error(codes.Internal, "failed to get org info")
		}
		userInfo.OrgID = &orgID
		// Mark user as needing approval if this org requires approvals.
		userInfo.IsApproved = !orgInfo.EnableApprovals
	}
	if userInfo.Email == "" {
		return nil, status.Error(codes.InvalidArgument, "email must not be empty")
	}
	if userInfo.IdentityProvider == "" {
		return nil, status.Error(codes.InvalidArgument, "identity provider must not be empty")
	}
	uid, err := s.uds.CreateUser(userInfo)
	return utils.ProtoFromUUID(uid), err
}

// GetUser is the GRPC method to get a user.
func (s *Server) GetUser(ctx context.Context, req *uuidpb.UUID) (*profilepb.UserInfo, error) {
	uid := utils.UUIDFromProtoOrNil(req)
	userInfo, err := s.uds.GetUser(uid)
	if err != nil {
		return nil, err
	}
	if userInfo == nil {
		return nil, status.Error(codes.NotFound, "no such user")
	}
	return userInfoToProto(userInfo), nil
}

// GetUserByEmail is the GRPC method to get a user by email.
func (s *Server) GetUserByEmail(ctx context.Context, req *profilepb.GetUserByEmailRequest) (*profilepb.UserInfo, error) {
	userInfo, err := s.uds.GetUserByEmail(req.Email)
	if err != nil {
		return nil, toExternalError(err)
	}
	return userInfoToProto(userInfo), nil
}

// GetUserByAuthProviderID returns the user identified by the AuthProviderID.
func (s *Server) GetUserByAuthProviderID(ctx context.Context, req *profilepb.GetUserByAuthProviderIDRequest) (*profilepb.UserInfo, error) {
	userInfo, err := s.uds.GetUserByAuthProviderID(req.AuthProviderID)
	if err != nil {
		return nil, toExternalError(err)
	}
	return userInfoToProto(userInfo), nil
}

// CreateOrgAndUser is the GRPC method to create a new org and user.
func (s *Server) CreateOrgAndUser(ctx context.Context, req *profilepb.CreateOrgAndUserRequest) (*profilepb.CreateOrgAndUserResponse, error) {
	orgInfo := &datastore.OrgInfo{
		DomainName: &req.Org.DomainName,
		OrgName:    req.Org.OrgName,
	}

	userInfo := &datastore.UserInfo{
		FirstName:        req.User.FirstName,
		LastName:         req.User.LastName,
		Email:            req.User.Email,
		IdentityProvider: req.User.IdentityProvider,
		// By default, the creating user is the owner and should be approved.
		IsApproved:     true,
		AuthProviderID: req.User.AuthProviderID,
	}
	if len(orgInfo.OrgName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid org name")
	}
	if userInfo.Email == "" {
		return nil, status.Error(codes.InvalidArgument, "email must not be empty")
	}
	if userInfo.IdentityProvider == "" {
		return nil, status.Error(codes.InvalidArgument, "identity provider must not be empty")
	}
	orgID, userID, err := s.uds.CreateUserAndOrg(orgInfo, userInfo)
	if err != nil {
		return nil, err
	}

	md, _ := metadata.FromIncomingContext(ctx)
	ctx = metadata.NewOutgoingContext(ctx, md)

	projectResp, err := s.env.ProjectManagerClient().RegisterProject(ctx, &projectmanagerpb.RegisterProjectRequest{
		OrgID:       utils.ProtoFromUUID(orgID),
		ProjectName: DefaultProjectName,
	})

	if err != nil {
		deleteErr := s.ods.DeleteOrgAndUsers(orgID)
		if deleteErr != nil {
			return nil, status.Error(codes.Internal,
				fmt.Sprintf("Could not delete org and users after create default project failed: %s", err.Error()))
		}
		return nil, err
	}
	if !projectResp.ProjectRegistered {
		return nil, status.Error(codes.Internal, fmt.Sprintf("Could not register project %s", DefaultProjectName))
	}

	resp := &profilepb.CreateOrgAndUserResponse{
		UserID: utils.ProtoFromUUID(userID),
		OrgID:  utils.ProtoFromUUID(orgID),
	}

	return resp, nil
}

// CreateOrg is the GRPC method to create a new org.
func (s *Server) CreateOrg(ctx context.Context, req *profilepb.CreateOrgRequest) (*uuidpb.UUID, error) {
	orgInfo := &datastore.OrgInfo{
		OrgName: req.OrgName,
	}
	if req.DomainName != nil {
		orgInfo.DomainName = &req.DomainName.Value
	}

	if len(orgInfo.OrgName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid org name")
	}

	oid, err := s.ods.CreateOrg(orgInfo)
	if err == datastore.ErrDuplicateOrgName {
		return nil, status.Error(codes.AlreadyExists, err.Error())
	}
	return utils.ProtoFromUUID(oid), err
}

// GetOrg is the GRPC method to get an org by ID.
func (s *Server) GetOrg(ctx context.Context, req *uuidpb.UUID) (*profilepb.OrgInfo, error) {
	orgID := utils.UUIDFromProtoOrNil(req)
	orgInfo, err := s.ods.GetOrg(orgID)
	if err != nil {
		return nil, err
	}
	if orgInfo == nil {
		return nil, status.Error(codes.NotFound, "no such org")
	}
	return orgInfoToProto(orgInfo), nil
}

// GetOrgs is the GRPC method to get all orgs. This should only be used internally.
func (s *Server) GetOrgs(ctx context.Context, req *profilepb.GetOrgsRequest) (*profilepb.GetOrgsResponse, error) {
	orgs, err := s.ods.GetOrgs()
	if err != nil {
		return nil, err
	}
	orgProtos := make([]*profilepb.OrgInfo, len(orgs))
	for i, o := range orgs {
		orgProtos[i] = orgInfoToProto(o)
	}

	return &profilepb.GetOrgsResponse{Orgs: orgProtos}, nil
}

// GetOrgByName gets an org by name.
// This is the org_name field, and currently happens to be either the entire email of a user or
// just the domain from a user's email depending on whether said user is in a self org or not.
func (s *Server) GetOrgByName(ctx context.Context, req *profilepb.GetOrgByNameRequest) (*profilepb.OrgInfo, error) {
	orgInfo, err := s.ods.GetOrgByName(req.Name)
	if err != nil {
		return nil, toExternalError(err)
	}
	return orgInfoToProto(orgInfo), nil
}

// GetOrgByDomain gets an org by domain name.
// This is the domain_name field which is auto populated by the hosted domain returned from the auth provider.
// This might be an empty string for auth users that don't have a hosted domain or
// NULL for orgs that haven't been backfilled.
func (s *Server) GetOrgByDomain(ctx context.Context, req *profilepb.GetOrgByDomainRequest) (*profilepb.OrgInfo, error) {
	orgInfo, err := s.ods.GetOrgByDomain(req.DomainName)
	if err != nil {
		return nil, toExternalError(err)
	}
	return orgInfoToProto(orgInfo), nil
}

// DeleteOrgAndUsers deletes an org and all of its users.
func (s *Server) DeleteOrgAndUsers(ctx context.Context, req *uuidpb.UUID) error {
	_, err := s.GetOrg(ctx, req)
	if err != nil {
		return err
	}
	return s.ods.DeleteOrgAndUsers(utils.UUIDFromProtoOrNil(req))
}

// UpdateUser updates a user's info.
func (s *Server) UpdateUser(ctx context.Context, req *profilepb.UpdateUserRequest) (*profilepb.UserInfo, error) {
	userID := utils.UUIDFromProtoOrNil(req.ID)
	userInfo, err := s.uds.GetUser(userID)
	if err != nil {
		return nil, toExternalError(err)
	}

	if req.OrgID != nil {
		newOrgID := utils.UUIDFromProtoOrNil(req.OrgID)
		if newOrgID == uuid.Nil {
			userInfo.OrgID = nil
		} else {
			userInfo.OrgID = &newOrgID
		}
	}

	if req.DisplayPicture != nil {
		userInfo.ProfilePicture = &req.DisplayPicture.Value
	}

	if req.IsApproved != nil {
		userInfo.IsApproved = req.IsApproved.Value
	}

	err = s.uds.UpdateUser(userInfo)
	if err != nil {
		return nil, toExternalError(err)
	}

	return userInfoToProto(userInfo), nil
}

// DeleteUser deletes a user. If they are the last user in the org, also deletes the org.
func (s *Server) DeleteUser(ctx context.Context, req *profilepb.DeleteUserRequest) (*profilepb.DeleteUserResponse, error) {
	userID := utils.UUIDFromProtoOrNil(req.ID)
	userInfo, err := s.uds.GetUser(userID)
	if err != nil {
		return nil, err
	}
	if userInfo == nil {
		return nil, status.Error(codes.NotFound, "no such user")
	}

	// Check whether user is the last user in the org.
	users, err := s.ods.GetUsersInOrg(*userInfo.OrgID)
	if err != nil {
		return nil, err
	}

	// Only delete the user, not the org.
	if len(users) > 1 {
		err = s.uds.DeleteUser(userID)
	} else { // Otherwise, delete the user and org.
		err = s.ods.DeleteOrgAndUsers(*userInfo.OrgID)
	}

	if err != nil {
		return nil, err
	}

	return &profilepb.DeleteUserResponse{}, nil
}

// GetUserSettings gets the user settings for the given user.
func (s *Server) GetUserSettings(ctx context.Context, req *profilepb.GetUserSettingsRequest) (*profilepb.GetUserSettingsResponse, error) {
	userID := utils.UUIDFromProtoOrNil(req.ID)

	settings, err := s.usds.GetUserSettings(userID)
	if err != nil {
		return nil, err
	}

	return &profilepb.GetUserSettingsResponse{
		AnalyticsOptout: *settings.AnalyticsOptout,
	}, nil
}

// UpdateUserSettings sets the user settings for the given user.
func (s *Server) UpdateUserSettings(ctx context.Context, req *profilepb.UpdateUserSettingsRequest) (*profilepb.UpdateUserSettingsResponse, error) {
	userSettings := &datastore.UserSettings{
		UserID: utils.UUIDFromProtoOrNil(req.ID),
	}

	if req.AnalyticsOptout != nil {
		userSettings.AnalyticsOptout = &req.AnalyticsOptout.Value
	}

	err := s.usds.UpdateUserSettings(userSettings)
	if err != nil {
		return nil, err
	}

	return &profilepb.UpdateUserSettingsResponse{}, nil
}

// GetUserAttributes gets the user attributes for the given user.
func (s *Server) GetUserAttributes(ctx context.Context, req *profilepb.GetUserAttributesRequest) (*profilepb.GetUserAttributesResponse, error) {
	userID := utils.UUIDFromProtoOrNil(req.ID)

	userAttrs, err := s.usds.GetUserAttributes(userID)
	if err != nil {
		return nil, err
	}

	return &profilepb.GetUserAttributesResponse{
		TourSeen: *userAttrs.TourSeen,
	}, nil
}

// SetUserAttributes sets the user attributes for the given user.
func (s *Server) SetUserAttributes(ctx context.Context, req *profilepb.SetUserAttributesRequest) (*profilepb.SetUserAttributesResponse, error) {
	userAttrs := &datastore.UserAttributes{
		UserID: utils.UUIDFromProtoOrNil(req.ID),
	}

	if req.TourSeen != nil {
		userAttrs.TourSeen = &req.TourSeen.Value
	}

	err := s.usds.SetUserAttributes(userAttrs)
	if err != nil {
		return nil, err
	}

	return &profilepb.SetUserAttributesResponse{}, nil
}

// GetUsersInOrg gets the users in the requested org, given that the requestor has permissions.
func (s *Server) GetUsersInOrg(ctx context.Context, req *profilepb.GetUsersInOrgRequest) (*profilepb.GetUsersInOrgResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	reqOrgID := utils.UUIDFromProtoOrNil(req.OrgID)

	if claimsOrgID != reqOrgID {
		return nil, errors.New("Unauthorized to get users for org")
	}

	users, err := s.ods.GetUsersInOrg(reqOrgID)
	if err != nil {
		return nil, err
	}

	usersProto := make([]*profilepb.UserInfo, len(users))
	for i, u := range users {
		usersProto[i] = userInfoToProto(u)
	}

	return &profilepb.GetUsersInOrgResponse{
		Users: usersProto,
	}, nil
}

// UpdateOrg updates an orgs info.
func (s *Server) UpdateOrg(ctx context.Context, req *profilepb.UpdateOrgRequest) (*profilepb.OrgInfo, error) {
	id := utils.UUIDFromProtoOrNil(req.ID)
	if id == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "org ID improperly formatted")
	}
	// Check to make sure the user is authorized to update org.
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	// Only check the claims type for users.
	if claimsutils.GetClaimsType(sCtx.Claims) == claimsutils.UserClaimType {
		claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)

		if id != claimsOrgID {
			return nil, status.Error(codes.PermissionDenied, "user does not have permissions to update org field")
		}
	}

	// Get OrgInfo.
	orgInfo, err := s.ods.GetOrg(id)
	if err != nil {
		return nil, toExternalError(err)
	}

	var hasUpdate bool
	if req.EnableApprovals != nil && orgInfo.EnableApprovals != req.EnableApprovals.Value {
		hasUpdate = true
		orgInfo.EnableApprovals = req.EnableApprovals.Value
	}
	if req.DomainName != nil {
		if orgInfo.DomainName == nil || orgInfo.GetDomainName() != req.DomainName.Value {
			hasUpdate = true
			orgInfo.DomainName = &req.DomainName.Value
		}
	}
	// If the values are the same, no need to update.
	if !hasUpdate {
		return orgInfoToProto(orgInfo), nil
	}

	if err := s.ods.UpdateOrg(orgInfo); err != nil {
		return nil, toExternalError(err)
	}
	// If EnableApprovals has changed to false, we flip the flag for all users to approve them.
	if req.EnableApprovals != nil && !orgInfo.EnableApprovals {
		err = s.ods.ApproveAllOrgUsers(id)
		if err != nil {
			return nil, toExternalError(err)
		}
	}
	return orgInfoToProto(orgInfo), nil
}

// AddOrgIDEConfig adds the IDE config for the given org.
func (s *Server) AddOrgIDEConfig(ctx context.Context, req *profilepb.AddOrgIDEConfigRequest) (*profilepb.AddOrgIDEConfigResponse, error) {
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)

	err := s.osds.AddIDEConfig(orgID, &datastore.IDEConfig{
		Name: req.Config.IDEName,
		Path: req.Config.Path,
	})

	if err != nil {
		return nil, err
	}

	return &profilepb.AddOrgIDEConfigResponse{
		Config: req.Config,
	}, nil
}

// DeleteOrgIDEConfig deletes the IDE config from the given org.
func (s *Server) DeleteOrgIDEConfig(ctx context.Context, req *profilepb.DeleteOrgIDEConfigRequest) (*profilepb.DeleteOrgIDEConfigResponse, error) {
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)

	err := s.osds.DeleteIDEConfig(orgID, req.IDEName)
	if err != nil {
		return nil, err
	}

	return &profilepb.DeleteOrgIDEConfigResponse{}, nil
}

// GetOrgIDEConfigs gets all IDE configs from the given org.
func (s *Server) GetOrgIDEConfigs(ctx context.Context, req *profilepb.GetOrgIDEConfigsRequest) (*profilepb.GetOrgIDEConfigsResponse, error) {
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)

	configs := make([]*datastore.IDEConfig, 0)
	if req.IDEName != "" {
		conf, err := s.osds.GetIDEConfig(orgID, req.IDEName)
		if err != nil {
			return nil, err
		}

		configs = append(configs, conf)
	} else {
		confs, err := s.osds.GetIDEConfigs(orgID)
		if err != nil {
			return nil, err
		}
		configs = confs
	}

	configPbs := make([]*profilepb.IDEConfig, len(configs))
	for i, c := range configs {
		configPbs[i] = &profilepb.IDEConfig{
			IDEName: c.Name,
			Path:    c.Path,
		}
	}

	return &profilepb.GetOrgIDEConfigsResponse{
		Configs: configPbs,
	}, nil
}

// CreateInviteToken creates a signed invite JWT for the given org with an expiration of 1 week.
func (s *Server) CreateInviteToken(ctx context.Context, req *profilepb.CreateInviteTokenRequest) (*profilepb.InviteToken, error) {
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if orgID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "org ID improperly formatted")
	}
	var inviteSigningKey string
	inviteSigningKey, err := s.ods.GetInviteSigningKey(orgID)
	if err != nil || inviteSigningKey == "" {
		inviteSigningKey, err = s.ods.CreateInviteSigningKey(orgID)
		if err != nil {
			return nil, err
		}
	}

	builder := jwt.NewBuilder()
	builder.
		Expiration(time.Now().Add(7 * 24 * time.Hour)).
		Subject(orgID.String())
	token, err := builder.Build()
	if err != nil {
		return nil, err
	}

	signed, err := claimsutils.SignToken(token, inviteSigningKey)
	if err != nil {
		return nil, err
	}

	return &profilepb.InviteToken{SignedClaims: string(signed)}, nil
}

// RevokeAllInviteTokens revokes all pending invited for the given org by rotating the JWT signing key.
func (s *Server) RevokeAllInviteTokens(ctx context.Context, req *uuidpb.UUID) (*types.Empty, error) {
	orgID := utils.UUIDFromProtoOrNil(req)
	if orgID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "org ID improperly formatted")
	}
	_, err := s.ods.CreateInviteSigningKey(orgID)
	if err != nil {
		return nil, err
	}
	return &types.Empty{}, nil
}

// VerifyInviteToken verifies that the given invite JWT is still valid by performing expiration and
// signing key checks.
func (s *Server) VerifyInviteToken(ctx context.Context, req *profilepb.InviteToken) (*profilepb.VerifyInviteTokenResponse, error) {
	signedClaims := req.GetSignedClaims()
	if signedClaims == "" {
		return nil, status.Error(codes.InvalidArgument, "invite token misformatted")
	}

	// Parse without verification to pull out the orgID first.
	token, err := jwt.Parse([]byte(signedClaims), jwt.WithValidate(true))
	if err != nil {
		return &profilepb.VerifyInviteTokenResponse{Valid: false}, nil
	}

	// Get the signing key for the orgID.
	orgID := uuid.FromStringOrNil(token.Subject())
	if orgID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "invite token misformatted")
	}
	inviteSigningKey, err := s.ods.GetInviteSigningKey(orgID)
	if err != nil {
		return nil, err
	}

	key, err := jwk.New([]byte(inviteSigningKey))
	if err != nil {
		return nil, err
	}
	_, err = jwt.Parse([]byte(signedClaims), jwt.WithVerify(jwa.HS256, key), jwt.WithValidate(true))
	if err != nil {
		return &profilepb.VerifyInviteTokenResponse{Valid: false}, nil
	}

	return &profilepb.VerifyInviteTokenResponse{Valid: true, OrgID: utils.ProtoFromUUID(orgID)}, nil
}
