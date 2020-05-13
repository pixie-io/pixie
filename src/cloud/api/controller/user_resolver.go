package controller

import (
	"context"
	"fmt"

	"github.com/graph-gophers/graphql-go"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	pbutils "pixielabs.ai/pixielabs/src/utils"
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
	userInfo, err := grpcAPI.GetUser(ctx, pbutils.ProtoFromUUIDStrOrNil(sCtx.Claims.GetUserClaims().UserID))
	if err != nil {
		return nil, err
	}

	return &UserInfoResolver{sCtx, &q.Env, ctx, userInfo}, nil
}

// ID returns the user id.
func (u *UserInfoResolver) ID() graphql.ID {
	return graphql.ID(u.SessionCtx.Claims.GetUserClaims().UserID)
}

// Name returns the user name.
func (u *UserInfoResolver) Name() string {
	return fmt.Sprintf("%s %s", u.UserInfo.FirstName, u.UserInfo.LastName)
}

// Email returns the user email.
func (u *UserInfoResolver) Email() string {
	return u.SessionCtx.Claims.GetUserClaims().Email
}

// Picture returns the users picture/avatar.
func (u *UserInfoResolver) Picture() string {
	return u.UserInfo.ProfilePicture
}

// OrgName returns the user's org name.
func (u *UserInfoResolver) OrgName() string {
	orgID := u.SessionCtx.Claims.GetUserClaims().OrgID

	org, err := u.GQLEnv.ProfileServiceClient.GetOrg(u.ctx, pbutils.ProtoFromUUIDStrOrNil(orgID))
	if err != nil {
		return ""
	}

	return org.OrgName
}
