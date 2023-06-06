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

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/authcontext"
	claimsutils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
)

// UserServiceServer is the server that implements the UserService gRPC service.
type UserServiceServer struct {
	ProfileServiceClient profilepb.ProfileServiceClient
	OrgServiceClient     profilepb.OrgServiceClient
}

// GetUser will retrieve user based on UUID.
func (u *UserServiceServer) GetUser(ctx context.Context, req *uuidpb.UUID) (*cloudpb.UserInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := u.ProfileServiceClient.GetUser(ctx, req)
	if err != nil {
		return nil, err
	}
	return &cloudpb.UserInfo{
		ID:             resp.ID,
		OrgID:          resp.OrgID,
		FirstName:      resp.FirstName,
		LastName:       resp.LastName,
		Email:          resp.Email,
		ProfilePicture: resp.ProfilePicture,
		IsApproved:     resp.IsApproved,
	}, nil
}

// GetUserSettings will retrieve settings given the user ID.
func (u *UserServiceServer) GetUserSettings(ctx context.Context, req *cloudpb.GetUserSettingsRequest) (*cloudpb.GetUserSettingsResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.GetUserSettingsRequest{
		ID: req.ID,
	}

	resp, err := u.ProfileServiceClient.GetUserSettings(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetUserSettingsResponse{
		AnalyticsOptout: resp.AnalyticsOptout,
	}, nil
}

// UpdateUserSettings will update the settings for the given user.
func (u *UserServiceServer) UpdateUserSettings(ctx context.Context, req *cloudpb.UpdateUserSettingsRequest) (*cloudpb.UpdateUserSettingsResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.UpdateUserSettingsRequest{
		ID:              req.ID,
		AnalyticsOptout: req.AnalyticsOptout,
	}

	_, err = u.ProfileServiceClient.UpdateUserSettings(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.UpdateUserSettingsResponse{}, nil
}

// UpdateUser will update user information.
func (u *UserServiceServer) UpdateUser(ctx context.Context, req *cloudpb.UpdateUserRequest) (*cloudpb.UserInfo,
	error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	if claimsutils.GetClaimsType(sCtx.Claims) != claimsutils.UserClaimType {
		return nil, errors.New("Unauthorized")
	}

	claimsOrgID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().OrgID)
	claimsUserID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().UserID)

	// Check permissions.
	// If user is in the org, they are permitted to update any org user's info, since all
	// users are considered admins.
	userResp, err := u.ProfileServiceClient.GetUser(ctx, req.ID)
	if err != nil {
		return nil, err
	}
	if claimsOrgID != utils.UUIDFromProtoOrNil(userResp.OrgID) {
		return nil, errors.New("Unauthorized")
	}
	// A user cannot update their own "isApproved" status.
	if req.IsApproved != nil && claimsUserID == utils.UUIDFromProtoOrNil(userResp.ID) {
		return nil, errors.New("Unauthorized")
	}

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.UpdateUserRequest{
		ID:             req.ID,
		DisplayPicture: req.DisplayPicture,
		IsApproved:     req.IsApproved,
	}

	resp, err := u.ProfileServiceClient.UpdateUser(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.UserInfo{
		ID:             resp.ID,
		OrgID:          resp.OrgID,
		FirstName:      resp.FirstName,
		LastName:       resp.LastName,
		Email:          resp.Email,
		ProfilePicture: resp.ProfilePicture,
		IsApproved:     resp.IsApproved,
	}, nil
}

// GetUserAttributes will retrieve attributes given the user ID.
func (u *UserServiceServer) GetUserAttributes(ctx context.Context, req *cloudpb.GetUserAttributesRequest) (*cloudpb.GetUserAttributesResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.GetUserAttributesRequest{
		ID: req.ID,
	}

	resp, err := u.ProfileServiceClient.GetUserAttributes(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetUserAttributesResponse{
		TourSeen: resp.TourSeen,
	}, nil
}

// SetUserAttributes will update the attributes for the given user.
func (u *UserServiceServer) SetUserAttributes(ctx context.Context, req *cloudpb.SetUserAttributesRequest) (*cloudpb.SetUserAttributesResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	in := &profilepb.SetUserAttributesRequest{
		ID:       req.ID,
		TourSeen: req.TourSeen,
	}

	_, err = u.ProfileServiceClient.SetUserAttributes(ctx, in)
	if err != nil {
		return nil, err
	}

	return &cloudpb.SetUserAttributesResponse{}, nil
}

// DeleteUser will delete the user. The request must be made by the user being deleted.
func (u *UserServiceServer) DeleteUser(ctx context.Context, req *cloudpb.DeleteUserRequest) (*cloudpb.DeleteUserResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	if claimsutils.GetClaimsType(sCtx.Claims) != claimsutils.UserClaimType {
		return nil, errors.New("Unauthorized")
	}

	claimsUserID := uuid.FromStringOrNil(sCtx.Claims.GetUserClaims().UserID)

	if claimsUserID != utils.UUIDFromProtoOrNil(req.ID) {
		return nil, errors.New("Unauthorized")
	}

	_, err = u.ProfileServiceClient.DeleteUser(ctx, &profilepb.DeleteUserRequest{
		ID: req.ID,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.DeleteUserResponse{}, nil
}
