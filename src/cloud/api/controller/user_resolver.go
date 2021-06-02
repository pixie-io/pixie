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

	"github.com/gogo/protobuf/types"
	"github.com/graph-gophers/graphql-go"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// UserInfoResolver resolves user information.
type UserInfoResolver struct {
	ctx      context.Context
	GQLEnv   *GraphQLEnv
	UserInfo *cloudpb.UserInfo
}

// User resolves user information.
func (q *QueryResolver) User(ctx context.Context) (*UserInfoResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	userID := utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID)
	userInfo, err := q.Env.UserServer.GetUser(ctx, userID)
	if err != nil {
		// Normally we'd return a nil resolver with an error here but if we get here,
		// it means that this is a support account, so grab info from the UserClaims and return that.
		userInfo = &cloudpb.UserInfo{
			ID:    userID,
			Email: sCtx.Claims.GetUserClaims().Email,
			OrgID: utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().OrgID),
		}
	}
	return &UserInfoResolver{ctx, &q.Env, userInfo}, nil
}

// ID returns the user id.
func (u *UserInfoResolver) ID() graphql.ID {
	return graphql.ID(utils.ProtoToUUIDStr(u.UserInfo.ID))
}

// Name returns the user name.
func (u *UserInfoResolver) Name() string {
	return fmt.Sprintf("%s %s", u.UserInfo.FirstName, u.UserInfo.LastName)
}

// Email returns the user email.
func (u *UserInfoResolver) Email() string {
	return u.UserInfo.Email
}

// Picture returns the users picture/avatar.
func (u *UserInfoResolver) Picture() string {
	return u.UserInfo.ProfilePicture
}

// OrgID returns the user's org id.
func (u *UserInfoResolver) OrgID() string {
	return utils.ProtoToUUIDStr(u.UserInfo.OrgID)
}

// OrgName returns the user's org name.
func (u *UserInfoResolver) OrgName() string {
	org, err := u.GQLEnv.UserServer.GetOrg(u.ctx, u.UserInfo.OrgID)
	if err != nil {
		return ""
	}

	return org.OrgName
}

// IsApproved returns whether the user has been approved by an admin user.
func (u *UserInfoResolver) IsApproved() bool {
	return u.UserInfo.IsApproved
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

	resp, err := q.Env.UserServer.GetUserSettings(ctx, &cloudpb.GetUserSettingsRequest{
		ID:   utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID),
		Keys: keys,
	})
	if err != nil {
		return nil, err
	}

	resolvers := make([]*UserSettingResolver, len(args.Keys))
	for i, k := range args.Keys {
		resolvers[i] = &UserSettingResolver{*k, resp.SettingMap[*k]}
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

	if len(args.Values) != len(args.Keys) {
		return false, fmt.Errorf("length of Keys and Values does not match")
	}

	settingsMap := make(map[string]string)
	for idx := range args.Keys {
		settingsMap[*args.Keys[idx]] = *args.Values[idx]
	}

	_, err = q.Env.UserServer.UpdateUserSettings(ctx, &cloudpb.UpdateUserSettingsRequest{
		ID:         utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID),
		SettingMap: settingsMap,
	})
	if err != nil {
		return false, err
	}

	return true, nil
}

type updateUserPermissionsArgs struct {
	UserID          graphql.ID
	UserPermissions *editableUserPermissions
}

type editableUserPermissions struct {
	IsApproved *bool
}

// UpdateUser updates the user info.
func (q *QueryResolver) UpdateUserPermissions(ctx context.Context, args *updateUserPermissionsArgs) (*UserInfoResolver, error) {
	userID := utils.ProtoFromUUIDStrOrNil(string(args.UserID))
	req := &cloudpb.UpdateUserRequest{
		ID: userID,
	}

	if args.UserPermissions.IsApproved != nil {
		req.IsApproved = &types.BoolValue{Value: *args.UserPermissions.IsApproved}
	}

	_, err := q.Env.UserServer.UpdateUser(ctx, req)
	if err != nil {
		return nil, err
	}

	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	userInfo, err := q.Env.UserServer.GetUser(ctx, userID)
	if err != nil {
		// Normally we'd return a nil resolver with an error here but if we get here,
		// it means that this is a support account, so grab info from the UserClaims and return that.
		userInfo = &cloudpb.UserInfo{
			ID:    userID,
			Email: sCtx.Claims.GetUserClaims().Email,
			OrgID: utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().OrgID),
		}
	}
	return &UserInfoResolver{ctx, &q.Env, userInfo}, nil
}
