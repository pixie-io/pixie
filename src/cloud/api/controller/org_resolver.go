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
		userResolvers[idx] = &UserInfoResolver{sCtx, &q.Env, ctx, &cloudpb.UserInfo{
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
	SessionCtx *authcontext.AuthContext
	GQLEnv     *GraphQLEnv
	ctx        context.Context
	OrgInfo    *cloudpb.OrgInfo
}

// ID returns the org id.
func (u *OrgInfoResolver) ID() graphql.ID {
	if u.OrgInfo != nil && u.OrgInfo.ID != nil {
		return graphql.ID(utils.ProtoToUUIDStr(u.OrgInfo.ID))
	}
	return graphql.ID(u.SessionCtx.Claims.GetUserClaims().OrgID)
}

// Name returns the org name.
func (u *OrgInfoResolver) Name() string {
	if u.OrgInfo == nil {
		return ""
	}
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
	orgInfo, err := grpcAPI.GetOrg(ctx, utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().OrgID))
	if err != nil {
		orgInfo = nil
	}
	return &OrgInfoResolver{sCtx, &q.Env, ctx, orgInfo}, nil
}

type updateOrgArgs struct {
	OrgInfo *editableOrgInfo
}

type editableOrgInfo struct {
	ID              graphql.ID
	EnableApprovals *bool
}

// UpdateOrg updates the org info.
func (q *QueryResolver) UpdateOrg(ctx context.Context, args *updateOrgArgs) (bool, error) {
	req := &cloudpb.UpdateOrgRequest{
		ID: utils.ProtoFromUUIDStrOrNil(string(args.OrgInfo.ID)),
	}

	if args.OrgInfo.EnableApprovals != nil {
		req.EnableApprovals = &types.BoolValue{
			Value: *args.OrgInfo.EnableApprovals,
		}
	}

	grpcAPI := q.Env.OrgServer
	_, err := grpcAPI.UpdateOrg(ctx, req)
	if err != nil {
		return false, err
	}
	return true, nil
}
