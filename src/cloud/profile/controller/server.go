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

package controller

import (
	"context"
	"errors"
	"fmt"
	"strings"

	"github.com/badoux/checkmail"
	"github.com/gofrs/uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/controller/idmanager"
	"px.dev/pixie/src/cloud/profile/datastore"
	"px.dev/pixie/src/cloud/profile/profileenv"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/project_manager/projectmanagerpb"
	"px.dev/pixie/src/shared/services/authcontext"
	claimsutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

var emailDomainBlockedList = map[string]bool{
	"blocklist.com": true,
}

// DefaultProjectName is the name of the default project we automatically assign to every org.
const DefaultProjectName string = "default"

// Datastore is the interface used to the backing store for profile information.
type Datastore interface {
	// CreateUser creates a new user.
	CreateUser(*datastore.UserInfo) (uuid.UUID, error)
	// GetUser gets a user by ID.
	GetUser(uuid.UUID) (*datastore.UserInfo, error)
	// GetUserByEmail gets a user by email.
	GetUserByEmail(string) (*datastore.UserInfo, error)

	// CreateUserAndOrg creates a user and org for creating a new org with specified user as owner.
	CreateUserAndOrg(*datastore.OrgInfo, *datastore.UserInfo) (orgID uuid.UUID, userID uuid.UUID, err error)
	// GetOrg gets and org by ID.
	GetOrg(uuid.UUID) (*datastore.OrgInfo, error)
	// GetOrgByDomain gets an org by domain name.
	GetOrgByDomain(string) (*datastore.OrgInfo, error)
	// Delete Org and all of its users
	DeleteOrgAndUsers(uuid.UUID) error
	// UpdateUser updates the user info.
	UpdateUser(*datastore.UserInfo) error

	// GetOrgs gets all the orgs.
	GetOrgs() ([]*datastore.OrgInfo, error)

	// GetUsersInOrg gets all of the users in the given org.
	GetUsersInOrg(uuid.UUID) ([]*datastore.UserInfo, error)
}

// UserSettingsDatastore is the interface used to the backing store for user settings.
type UserSettingsDatastore interface {
	// GetUserSettings gets the user settings for the given user and keys.
	GetUserSettings(uuid.UUID, []string) ([]string, error)
	// UpdateUserSettings updates the keys and values for the given user.
	UpdateUserSettings(uuid.UUID, []string, []string) error
}

// Server is an implementation of GRPC server for profile service.
type Server struct {
	env       profileenv.ProfileEnv
	d         Datastore
	uds       UserSettingsDatastore
	IDManager idmanager.Manager
}

// NewServer creates a new GRPC profile server.
func NewServer(env profileenv.ProfileEnv, d Datastore, uds UserSettingsDatastore, idm idmanager.Manager) *Server {
	return &Server{env: env, d: d, uds: uds, IDManager: idm}
}

func userInfoToProto(u *datastore.UserInfo) *profilepb.UserInfo {
	profilePicture := ""
	if u.ProfilePicture != nil {
		profilePicture = *u.ProfilePicture
	}
	return &profilepb.UserInfo{
		ID:             utils.ProtoFromUUID(u.ID),
		OrgID:          utils.ProtoFromUUID(u.OrgID),
		Username:       u.Username,
		FirstName:      u.FirstName,
		LastName:       u.LastName,
		Email:          u.Email,
		ProfilePicture: profilePicture,
		IsApproved:     u.IsApproved,
	}
}

func orgInfoToProto(o *datastore.OrgInfo) *profilepb.OrgInfo {
	return &profilepb.OrgInfo{
		ID:         utils.ProtoFromUUID(o.ID),
		OrgName:    o.OrgName,
		DomainName: o.DomainName,
	}
}

func checkValidEmail(email string) error {
	if len(email) == 0 || checkmail.ValidateFormat(email) != nil {
		return errors.New("failed validation")
	}

	components := strings.Split(email, "@")
	if len(components) != 2 {
		return errors.New("malformed email")
	}
	_, domain := components[0], components[1]

	if _, exists := emailDomainBlockedList[domain]; exists {
		return errors.New("disallowed email domain")
	}
	return nil
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
	orgID := utils.UUIDFromProtoOrNil(req.OrgID)
	if orgID == uuid.Nil {
		return nil, status.Error(codes.InvalidArgument, "invalid org id")
	}
	orgInfo, err := s.d.GetOrg(orgID)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to get org info")
	}
	// If enable approvals is true, that means new users by default will not be approved. (approval = false).
	defaultIsApproved := !orgInfo.EnableApprovals
	userInfo := &datastore.UserInfo{
		OrgID:      orgID,
		Username:   req.Username,
		FirstName:  req.FirstName,
		LastName:   req.LastName,
		Email:      req.Email,
		IsApproved: defaultIsApproved,
	}
	if len(userInfo.Username) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid username")
	}
	if err := checkValidEmail(userInfo.Email); err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}
	uid, err := s.d.CreateUser(userInfo)
	return utils.ProtoFromUUID(uid), err
}

// GetUser is the GRPC method to get a user.
func (s *Server) GetUser(ctx context.Context, req *uuidpb.UUID) (*profilepb.UserInfo, error) {
	uid := utils.UUIDFromProtoOrNil(req)
	userInfo, err := s.d.GetUser(uid)
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
	userInfo, err := s.d.GetUserByEmail(req.Email)
	if err != nil {
		return nil, toExternalError(err)
	}
	return userInfoToProto(userInfo), nil
}

// CreateOrgAndUser is the GRPC method to create a new org and user.
func (s *Server) CreateOrgAndUser(ctx context.Context, req *profilepb.CreateOrgAndUserRequest) (*profilepb.CreateOrgAndUserResponse, error) {
	orgInfo := &datastore.OrgInfo{
		DomainName: req.Org.DomainName,
		OrgName:    req.Org.OrgName,
	}

	userInfo := &datastore.UserInfo{
		Username:  req.User.Username,
		FirstName: req.User.FirstName,
		LastName:  req.User.LastName,
		Email:     req.User.Email,
		// By default, the creating user is the owner and should be approved.
		IsApproved: true,
	}
	if len(orgInfo.DomainName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid domain name")
	}
	if len(orgInfo.OrgName) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid org name")
	}
	if len(userInfo.Username) == 0 {
		return nil, status.Error(codes.InvalidArgument, "invalid username")
	}
	if err := checkValidEmail(userInfo.Email); err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	orgID, userID, err := s.d.CreateUserAndOrg(orgInfo, userInfo)
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
		deleteErr := s.d.DeleteOrgAndUsers(orgID)
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

// GetOrg is the GRPC method to get an org by ID.
func (s *Server) GetOrg(ctx context.Context, req *uuidpb.UUID) (*profilepb.OrgInfo, error) {
	orgID := utils.UUIDFromProtoOrNil(req)
	orgInfo, err := s.d.GetOrg(orgID)
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
	orgs, err := s.d.GetOrgs()
	if err != nil {
		return nil, err
	}
	orgProtos := make([]*profilepb.OrgInfo, len(orgs))
	for i, o := range orgs {
		orgProtos[i] = orgInfoToProto(o)
	}

	return &profilepb.GetOrgsResponse{Orgs: orgProtos}, nil
}

// GetOrgByDomain gets an org by domain name.
func (s *Server) GetOrgByDomain(ctx context.Context, req *profilepb.GetOrgByDomainRequest) (*profilepb.OrgInfo, error) {
	orgInfo, err := s.d.GetOrgByDomain(req.DomainName)
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
	return s.d.DeleteOrgAndUsers(utils.UUIDFromProtoOrNil(req))
}

// UpdateUser updates a user's info.
func (s *Server) UpdateUser(ctx context.Context, req *profilepb.UpdateUserRequest) (*profilepb.UserInfo, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	var claimsOrgID uuid.UUID
	var claimsUserID uuid.UUID
	checkUserClaims := false

	if claimsutils.GetClaimsType(sCtx.Claims) == claimsutils.UserClaimType {
		claimsOrgID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
		claimsUserID = uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().UserID)
		checkUserClaims = true
	}

	userID := utils.UUIDFromProtoOrNil(req.ID)
	userInfo, err := s.d.GetUser(userID)
	if err != nil {
		return nil, toExternalError(err)
	}

	// Users should be able to update their userInfo, or another user's info only if they are an admin user
	// in the org. Once we have RBAC, this should be updated to account for the latter.
	if req.DisplayPicture != nil {
		if checkUserClaims && userInfo.OrgID != claimsOrgID {
			return nil, status.Error(codes.PermissionDenied, "User does not have permissions to update user field")
		}
		userInfo.ProfilePicture = &req.DisplayPicture.Value
	}

	// Admin users should be able to approve other users in the org. Once RBAC is implemented, we should change this
	// to check whether the user is actually an admin.
	if req.IsApproved != nil {
		if checkUserClaims && (claimsUserID == userID || userInfo.OrgID != claimsOrgID) {
			return nil, status.Error(codes.PermissionDenied, "User does not have permissions to approve user")
		}
		userInfo.IsApproved = req.IsApproved.Value
	}

	err = s.d.UpdateUser(userInfo)
	if err != nil {
		return nil, toExternalError(err)
	}

	return userInfoToProto(userInfo), nil
}

// GetUserSettings gets the user settings for the given user.
func (s *Server) GetUserSettings(ctx context.Context, req *profilepb.GetUserSettingsRequest) (*profilepb.GetUserSettingsResponse, error) {
	userID := utils.UUIDFromProtoOrNil(req.ID)

	values, err := s.uds.GetUserSettings(userID, req.Keys)
	if err != nil {
		return nil, err
	}

	resp := &profilepb.GetUserSettingsResponse{
		Keys:   req.Keys,
		Values: values,
	}

	return resp, nil
}

// UpdateUserSettings updates the given keys and values for the specified user.
func (s *Server) UpdateUserSettings(ctx context.Context, req *profilepb.UpdateUserSettingsRequest) (*profilepb.UpdateUserSettingsResponse, error) {
	userID := utils.UUIDFromProtoOrNil(req.ID)

	if len(req.Keys) != len(req.Values) {
		return nil, status.Error(codes.InvalidArgument, "keys and values lengths must be equal")
	}

	err := s.uds.UpdateUserSettings(userID, req.Keys, req.Values)
	if err != nil {
		return nil, err
	}

	return &profilepb.UpdateUserSettingsResponse{OK: true}, nil
}

// InviteUser implements the Profile interface's InviteUser method.
func (s *Server) InviteUser(ctx context.Context, req *profilepb.InviteUserRequest) (*profilepb.InviteUserResponse, error) {
	userInfo, err := s.d.GetUserByEmail(req.Email)
	var userID uuid.UUID
	if err == datastore.ErrUserNotFound {
		createUserReq := &profilepb.CreateUserRequest{
			OrgID:     req.OrgID,
			Username:  req.Email,
			FirstName: req.FirstName,
			LastName:  req.LastName,
			Email:     req.Email,
		}

		userIDPb, err := s.CreateUser(ctx, createUserReq)
		if err != nil {
			return nil, err
		}
		userID = utils.UUIDFromProtoOrNil(userIDPb)
	} else if err != nil {
		return nil, err
	} else if err == nil && req.MustCreateUser {
		return nil, errors.New("cannot invite a user that already exists")
	} else if err == nil {
		userID = userInfo.ID
	}

	idpCreateAccReq := &idmanager.CreateInviteLinkRequest{
		Email:    req.Email,
		PLOrgID:  utils.ProtoToUUIDStr(req.OrgID),
		PLUserID: userID.String(),
	}

	resp, err := s.IDManager.CreateInviteLink(ctx, idpCreateAccReq)
	if err != nil {
		return nil, err
	}

	return &profilepb.InviteUserResponse{
		Email:      resp.Email,
		InviteLink: resp.InviteLink,
	}, nil
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

	users, err := s.d.GetUsersInOrg(reqOrgID)
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
