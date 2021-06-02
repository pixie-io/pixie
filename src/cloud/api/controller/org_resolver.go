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

	"github.com/gogo/protobuf/types"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

type inviteUserArgs struct {
	Email     string
	FirstName string
	LastName  string
}

// UserInviteResolver resolves a user invite.
type UserInviteResolver struct {
	Email      string
	InviteLink string
}

// InviteUser invites the user with the given name and email address to the org by providing
// an invite link.
func (q *QueryResolver) InviteUser(ctx context.Context, args *inviteUserArgs) (*UserInviteResolver, error) {
	grpcAPI := q.Env.OrgServer

	resp, err := grpcAPI.InviteUser(ctx, &cloudpb.InviteUserRequest{
		Email:     args.Email,
		FirstName: args.FirstName,
		LastName:  args.LastName,
	})

	if err != nil {
		return nil, err
	}

	return &UserInviteResolver{
		Email:      resp.Email,
		InviteLink: resp.InviteLink,
	}, nil
}

// OrgUsers gets the users in the org in the given context.
func (q *QueryResolver) OrgUsers(ctx context.Context) ([]*UserInfoResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	grpcAPI := q.Env.OrgServer
	resp, err := grpcAPI.GetUsersInOrg(ctx, &cloudpb.GetUsersInOrgRequest{
		OrgID: utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().OrgID),
	})
	if err != nil {
		return nil, err
	}

	userResolvers := make([]*UserInfoResolver, len(resp.Users))
	for idx, user := range resp.Users {
		userResolvers[idx] = &UserInfoResolver{ctx, &q.Env, &cloudpb.UserInfo{
			ID:             user.ID,
			OrgID:          user.OrgID,
			Username:       user.Username,
			FirstName:      user.FirstName,
			LastName:       user.LastName,
			Email:          user.Email,
			ProfilePicture: user.ProfilePicture,
			IsApproved:     user.IsApproved,
		}}
	}

	return userResolvers, nil
}

// OrgInfoResolver resolves org information.
type OrgInfoResolver struct {
	OrgInfo *cloudpb.OrgInfo
}

// ID returns the org id.
func (u *OrgInfoResolver) ID() graphql.ID {
	return graphql.ID(utils.ProtoToUUIDStr(u.OrgInfo.ID))
}

// Name returns the org name.
func (u *OrgInfoResolver) Name() string {
	return u.OrgInfo.OrgName
}

// EnableApprovals returns whether the org requires admin approval for new users or not.
func (u *OrgInfoResolver) EnableApprovals() bool {
	return u.OrgInfo.EnableApprovals
}

// Org resolves org information.
func (q *QueryResolver) Org(ctx context.Context) (*OrgInfoResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	grpcAPI := q.Env.OrgServer
	idPb := utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().OrgID)
	orgInfo, err := grpcAPI.GetOrg(ctx, idPb)
	if err != nil {
		orgInfo = &cloudpb.OrgInfo{
			ID: idPb,
		}
	}
	return &OrgInfoResolver{orgInfo}, nil
}

type updateOrgSettingsArgs struct {
	OrgID       graphql.ID
	OrgSettings editableOrgSettings
}

type editableOrgSettings struct {
	EnableApprovals *bool
}

// UpdateOrgSettings updates settings for the given org.
func (q *QueryResolver) UpdateOrgSettings(ctx context.Context, args updateOrgSettingsArgs) (*OrgInfoResolver, error) {
	idPb := utils.ProtoFromUUIDStrOrNil(string(args.OrgID))
	req := &cloudpb.UpdateOrgRequest{
		ID: idPb,
	}

	if args.OrgSettings.EnableApprovals != nil {
		req.EnableApprovals = &types.BoolValue{
			Value: *args.OrgSettings.EnableApprovals,
		}
	}

	grpcAPI := q.Env.OrgServer
	_, err := grpcAPI.UpdateOrg(ctx, req)
	if err != nil {
		return nil, err
	}

	orgInfo, err := grpcAPI.GetOrg(ctx, idPb)
	if err != nil {
		orgInfo = &cloudpb.OrgInfo{
			ID: idPb,
		}
	}
	return &OrgInfoResolver{orgInfo}, nil
}
