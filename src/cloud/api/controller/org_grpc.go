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

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// OrganizationServiceServer is the server that implements the OrganizationService gRPC service.
type OrganizationServiceServer struct {
	ProfileServiceClient profilepb.ProfileServiceClient
	AuthServiceClient    authpb.AuthServiceClient
}

// InviteUser creates and returns an invite link for the org for the specified user info.
func (o *OrganizationServiceServer) InviteUser(ctx context.Context, externalReq *cloudpb.InviteUserRequest) (*cloudpb.InviteUserResponse, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	claimsOrgID := sCtx.Claims.GetUserClaims().OrgID
	orgIDPb := utils.ProtoFromUUIDStrOrNil(claimsOrgID)
	if orgIDPb == nil {
		return nil, status.Errorf(codes.InvalidArgument, "Could not identify user's org")
	}

	internalReq := &authpb.InviteUserRequest{
		OrgID:     orgIDPb,
		Email:     externalReq.Email,
		FirstName: externalReq.FirstName,
		LastName:  externalReq.LastName,
	}

	resp, err := o.AuthServiceClient.InviteUser(ctx, internalReq)
	if err != nil {
		return nil, err
	}

	return &cloudpb.InviteUserResponse{
		Email:      externalReq.Email,
		InviteLink: resp.InviteLink,
	}, nil
}

// GetOrg will retrieve org based on uuid.
func (o *OrganizationServiceServer) GetOrg(ctx context.Context, req *uuidpb.UUID) (*cloudpb.OrgInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := o.ProfileServiceClient.GetOrg(ctx, req)
	if err != nil {
		return nil, err
	}

	return &cloudpb.OrgInfo{
		ID:              resp.ID,
		OrgName:         resp.OrgName,
		DomainName:      resp.DomainName,
		EnableApprovals: resp.EnableApprovals,
	}, nil
}

// UpdateOrg will update org approval details.
func (o *OrganizationServiceServer) UpdateOrg(ctx context.Context, req *cloudpb.UpdateOrgRequest) (*cloudpb.OrgInfo, error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	resp, err := o.ProfileServiceClient.UpdateOrg(ctx, &profilepb.UpdateOrgRequest{
		ID:              req.ID,
		EnableApprovals: req.EnableApprovals,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.OrgInfo{
		ID:              resp.ID,
		OrgName:         resp.OrgName,
		DomainName:      resp.DomainName,
		EnableApprovals: resp.EnableApprovals,
	}, nil
}

// GetUsersInOrg will get users given an org id.
func (o *OrganizationServiceServer) GetUsersInOrg(ctx context.Context, req *cloudpb.GetUsersInOrgRequest) (*cloudpb.GetUsersInOrgResponse,
	error) {
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	inReq := &profilepb.GetUsersInOrgRequest{
		OrgID: req.OrgID,
	}

	resp, err := o.ProfileServiceClient.GetUsersInOrg(ctx, inReq)
	if err != nil {
		return nil, err
	}

	userList := make([]*cloudpb.UserInfo, len(resp.Users))
	for idx, user := range resp.Users {
		userList[idx] = &cloudpb.UserInfo{
			ID:             user.ID,
			OrgID:          user.OrgID,
			Username:       user.Username,
			FirstName:      user.FirstName,
			LastName:       user.LastName,
			Email:          user.Email,
			ProfilePicture: user.ProfilePicture,
			IsApproved:     user.IsApproved,
		}
	}

	return &cloudpb.GetUsersInOrgResponse{
		Users: userList,
	}, nil
}
