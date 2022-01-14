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

	"github.com/gofrs/uuid"
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
		return nil, rpcErrorHelper(err)
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
		return nil, rpcErrorHelper(err)
	}

	userResolvers := make([]*UserInfoResolver, len(resp.Users))
	for idx, user := range resp.Users {
		userResolvers[idx] = &UserInfoResolver{ctx, &q.Env, &cloudpb.UserInfo{
			ID:             user.ID,
			OrgID:          user.OrgID,
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
	ctx     context.Context
	OrgInfo *cloudpb.OrgInfo
	gqlEnv  *GraphQLEnv
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

// DomainName returns the domain name (if this org is a GSuite based org) for the given org.
func (u *OrgInfoResolver) DomainName() string {
	return u.OrgInfo.DomainName
}

type createOrgArgs struct {
	OrgName string
}

// CreateOrg creates an org with the given name and associates the current user with the
// newly created org.
func (q *QueryResolver) CreateOrg(ctx context.Context, args *createOrgArgs) (graphql.ID, error) {
	grpcAPI := q.Env.OrgServer
	resp, err := grpcAPI.CreateOrg(ctx, &cloudpb.CreateOrgRequest{
		OrgName: args.OrgName,
	})
	if err != nil {
		return graphql.ID(uuid.Nil.String()), rpcErrorHelper(err)
	}
	return graphql.ID(utils.ProtoToUUIDStr(resp)), nil
}

// IDEPathResolver is a resolver for an IDE path.
type IDEPathResolver struct {
	IDEName string
	Path    string
}

// IDEPaths returns the configured IDE paths for the org, which can be used to navigate to
// a symbol in an IDE.
func (u *OrgInfoResolver) IDEPaths() []IDEPathResolver {
	sCtx, err := authcontext.FromContext(u.ctx)
	if err != nil {
		return []IDEPathResolver{}
	}

	resp, err := u.gqlEnv.OrgServer.GetOrgIDEConfigs(u.ctx,
		&cloudpb.GetOrgIDEConfigsRequest{
			OrgID: utils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().OrgID),
		},
	)
	if err != nil {
		return []IDEPathResolver{}
	}

	configResolvers := make([]IDEPathResolver, len(resp.Configs))
	for i, c := range resp.Configs {
		configResolvers[i] = IDEPathResolver{
			IDEName: c.IDEName,
			Path:    c.Path,
		}
	}

	return configResolvers
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

	return &OrgInfoResolver{ctx, orgInfo, &q.Env}, nil
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
		return nil, rpcErrorHelper(err)
	}

	orgInfo, err := grpcAPI.GetOrg(ctx, idPb)
	if err != nil {
		orgInfo = &cloudpb.OrgInfo{
			ID: idPb,
		}
	}
	return &OrgInfoResolver{ctx, orgInfo, &q.Env}, nil
}

type createInviteTokenArgs struct {
	OrgID graphql.ID
}

// CreateInviteToken creates a signed invite JWT for the given org with an expiration of 1 week.
func (q *QueryResolver) CreateInviteToken(ctx context.Context, args *createInviteTokenArgs) (string, error) {
	grpcAPI := q.Env.OrgServer

	resp, err := grpcAPI.CreateInviteToken(ctx, &cloudpb.CreateInviteTokenRequest{
		OrgID: utils.ProtoFromUUIDStrOrNil(string(args.OrgID)),
	})

	if err != nil {
		return "", rpcErrorHelper(err)
	}

	return resp.SignedClaims, nil
}

type revokeAllInviteTokensArgs struct {
	OrgID graphql.ID
}

// RevokeAllInviteTokens revokes all pending invited for the given org by rotating the JWT signing key.
func (q *QueryResolver) RevokeAllInviteTokens(ctx context.Context, args *revokeAllInviteTokensArgs) (bool, error) {
	grpcAPI := q.Env.OrgServer

	_, err := grpcAPI.RevokeAllInviteTokens(ctx, utils.ProtoFromUUIDStrOrNil(string(args.OrgID)))

	if err != nil {
		return false, rpcErrorHelper(err)
	}

	return true, nil
}

type verifyInviteTokenArgs struct {
	InviteToken string
}

// VerifyInviteToken verifies that the given invite JWT is still valid by performing expiration and
// signing key checks.
func (q *QueryResolver) VerifyInviteToken(ctx context.Context, args *verifyInviteTokenArgs) (bool, error) {
	grpcAPI := q.Env.OrgServer

	resp, err := grpcAPI.VerifyInviteToken(ctx, &cloudpb.InviteToken{SignedClaims: args.InviteToken})

	if err != nil {
		return false, rpcErrorHelper(err)
	}

	return resp.Valid, nil
}

type removeUserFromOrg struct {
	UserID graphql.ID
}

// RemoveUserFromOrg removes the given user from the current org.
func (q *QueryResolver) RemoveUserFromOrg(ctx context.Context, args *removeUserFromOrg) (bool, error) {
	grpcAPI := q.Env.OrgServer

	resp, err := grpcAPI.RemoveUserFromOrg(ctx, &cloudpb.RemoveUserFromOrgRequest{UserID: utils.ProtoFromUUIDStrOrNil(string(args.UserID))})

	if err != nil {
		return false, rpcErrorHelper(err)
	}

	return resp.Success, nil
}
