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
		return nil, rpcErrorHelper(err)
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
	org, err := u.GQLEnv.OrgServer.GetOrg(u.ctx, u.UserInfo.OrgID)
	if err != nil {
		return ""
	}

	return org.OrgName
}

// IsApproved returns whether the user has been approved by an admin user.
func (u *UserInfoResolver) IsApproved() bool {
	return u.UserInfo.IsApproved
}

// UserSettingsResolver resolves user settings.
type UserSettingsResolver struct {
	AnalyticsOptout bool
	ID              graphql.ID
}

type updateUserSettingsArgs struct {
	Settings *editableUserSettings
}

type editableUserSettings struct {
	AnalyticsOptout *bool
}

// UserSettings resolves user settings information.
func (q *QueryResolver) UserSettings(ctx context.Context) (*UserSettingsResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	id := sCtx.Claims.GetUserClaims().UserID
	resp, err := q.Env.UserServer.GetUserSettings(ctx, &cloudpb.GetUserSettingsRequest{
		ID: utils.ProtoFromUUIDStrOrNil(id),
	})
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	return &UserSettingsResolver{AnalyticsOptout: resp.AnalyticsOptout, ID: graphql.ID(id)}, nil
}

// UpdateUserSettings updates the user settings for the current user.
func (q *QueryResolver) UpdateUserSettings(ctx context.Context, args *updateUserSettingsArgs) (*UserSettingsResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	id := sCtx.Claims.GetUserClaims().UserID
	req := &cloudpb.UpdateUserSettingsRequest{
		ID: utils.ProtoFromUUIDStrOrNil(id),
	}

	resp := &UserSettingsResolver{ID: graphql.ID(id)}

	if args.Settings.AnalyticsOptout != nil {
		req.AnalyticsOptout = &types.BoolValue{Value: *args.Settings.AnalyticsOptout}
		resp.AnalyticsOptout = *args.Settings.AnalyticsOptout
	}

	_, err = q.Env.UserServer.UpdateUserSettings(ctx, req)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	return resp, nil
}

type updateUserPermissionsArgs struct {
	UserID          graphql.ID
	UserPermissions *editableUserPermissions
}

type editableUserPermissions struct {
	IsApproved *bool
}

// UpdateUserPermissions updates user permissions.
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
		return nil, rpcErrorHelper(err)
	}

	userInfo, err := q.Env.UserServer.GetUser(ctx, userID)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}
	return &UserInfoResolver{ctx, &q.Env, userInfo}, nil
}

// UserAttributesResolver is a resolver for user attributes.
type UserAttributesResolver struct {
	TourSeen bool
	ID       graphql.ID
}

// UserAttributes resolves user attributes information.
func (q *QueryResolver) UserAttributes(ctx context.Context) (*UserAttributesResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	id := sCtx.Claims.GetUserClaims().UserID
	resp, err := q.Env.UserServer.GetUserAttributes(ctx, &cloudpb.GetUserAttributesRequest{
		ID: utils.ProtoFromUUIDStrOrNil(id),
	})
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	return &UserAttributesResolver{TourSeen: resp.TourSeen, ID: graphql.ID(id)}, nil
}

type setUserAttributesArgs struct {
	Attributes *editableUserAttributes
}

type editableUserAttributes struct {
	TourSeen *bool
}

// SetUserAttributes updates the user settings for the current user.
func (q *QueryResolver) SetUserAttributes(ctx context.Context, args *setUserAttributesArgs) (*UserAttributesResolver, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}

	id := sCtx.Claims.GetUserClaims().UserID
	req := &cloudpb.SetUserAttributesRequest{
		ID: utils.ProtoFromUUIDStrOrNil(id),
	}

	resp := &UserAttributesResolver{ID: graphql.ID(id)}

	if args.Attributes.TourSeen != nil {
		req.TourSeen = &types.BoolValue{Value: *args.Attributes.TourSeen}
		resp.TourSeen = *args.Attributes.TourSeen
	}

	_, err = q.Env.UserServer.SetUserAttributes(ctx, req)
	if err != nil {
		return nil, rpcErrorHelper(err)
	}

	return resp, nil
}

// DeleteUser deletes the user with the current credentials.
func (q *QueryResolver) DeleteUser(ctx context.Context) (bool, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return false, err
	}

	id := sCtx.Claims.GetUserClaims().UserID
	req := &cloudpb.DeleteUserRequest{
		ID: utils.ProtoFromUUIDStrOrNil(id),
	}

	_, err = q.Env.UserServer.DeleteUser(ctx, req)
	if err != nil {
		return false, rpcErrorHelper(err)
	}

	return true, nil
}
