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
	"fmt"

	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// UserInfoResolver resolves user information.
type UserInfoResolver struct {
	SessionCtx *authcontext.AuthContext
	GQLEnv     *GraphQLEnv
	ctx        context.Context
	UserInfo   *profilepb.UserInfo
}

// User resolves user information.
func (q *QueryResolver) User(ctx context.Context) (*UserInfoResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	grpcAPI := q.Env.ProfileServiceClient
	userInfo, err := grpcAPI.GetUser(ctx, utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID))
	if err != nil {
		userInfo = nil
	}

	return &UserInfoResolver{sCtx, &q.Env, ctx, userInfo}, nil
}

// ID returns the user id.
func (u *UserInfoResolver) ID() graphql.ID {
	return graphql.ID(u.SessionCtx.Claims.GetUserClaims().UserID)
}

// Name returns the user name.
func (u *UserInfoResolver) Name() string {
	if u.UserInfo == nil {
		return ""
	}
	return fmt.Sprintf("%s %s", u.UserInfo.FirstName, u.UserInfo.LastName)
}

// Email returns the user email.
func (u *UserInfoResolver) Email() string {
	return u.SessionCtx.Claims.GetUserClaims().Email
}

// Picture returns the users picture/avatar.
func (u *UserInfoResolver) Picture() string {
	if u.UserInfo == nil {
		return ""
	}
	return u.UserInfo.ProfilePicture
}

// OrgID returns the user's org id.
func (u *UserInfoResolver) OrgID() string {
	return u.SessionCtx.Claims.GetUserClaims().OrgID
}

// OrgName returns the user's org name.
func (u *UserInfoResolver) OrgName() string {
	orgID := u.SessionCtx.Claims.GetUserClaims().OrgID

	org, err := u.GQLEnv.ProfileServiceClient.GetOrg(u.ctx, utils.ProtoFromUUIDStrOrNil(orgID))
	if err != nil {
		return ""
	}

	return org.OrgName
}

// UserSettingResolver resolves a user setting.
type UserSettingResolver struct {
	key   string
	value string
}

// Key gets the key for the user setting.
func (u *UserSettingResolver) Key() string {
	return u.key
}

// Value gets the value for the user setting.
func (u *UserSettingResolver) Value() string {
	return u.value
}

type userSettingsArgs struct {
	Keys []*string
}

// UserSettings resolves user settings information.
func (q *QueryResolver) UserSettings(ctx context.Context, args *userSettingsArgs) ([]*UserSettingResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	keys := make([]string, len(args.Keys))
	for i := range args.Keys {
		keys[i] = *args.Keys[i]
	}

	grpcAPI := q.Env.ProfileServiceClient
	resp, err := grpcAPI.GetUserSettings(ctx, &profilepb.GetUserSettingsRequest{
		ID:   utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID),
		Keys: keys,
	})
	if err != nil {
		return nil, err
	}

	resolvers := make([]*UserSettingResolver, len(args.Keys))
	for i, k := range resp.Keys {
		resolvers[i] = &UserSettingResolver{k, resp.Values[i]}
	}

	return resolvers, nil
}

type updateUserSettingsArgs struct {
	Keys   []*string
	Values []*string
}

// UpdateUserSettings updates the user settings for the current user.
func (q *QueryResolver) UpdateUserSettings(ctx context.Context, args *updateUserSettingsArgs) (bool, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return false, err
	}
	grpcAPI := q.Env.ProfileServiceClient

	keys := make([]string, len(args.Keys))
	for i := range args.Keys {
		keys[i] = *args.Keys[i]
	}
	values := make([]string, len(args.Values))
	for i := range args.Values {
		values[i] = *args.Values[i]
	}

	resp, err := grpcAPI.UpdateUserSettings(ctx, &profilepb.UpdateUserSettingsRequest{
		ID:     utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID),
		Keys:   keys,
		Values: values,
	})
	if err != nil {
		return false, err
	}

	return resp.OK, nil
}

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
