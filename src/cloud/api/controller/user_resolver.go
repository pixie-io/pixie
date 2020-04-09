package controller

import (
	"context"

	"github.com/graph-gophers/graphql-go"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	pbutils "pixielabs.ai/pixielabs/src/utils"
)

// UserInfoResolver resolves user information.
type UserInfoResolver struct {
	SessionCtx *authcontext.AuthContext
	GQLEnv     *GraphQLEnv
	ctx        context.Context
}

// ID returns the user id.
func (u *UserInfoResolver) ID() graphql.ID {
	return graphql.ID(u.SessionCtx.Claims.GetUserClaims().UserID)
}

// Name returns the user name.
func (u *UserInfoResolver) Name() string {
	return "UNKNOWN"
}

// Email returns the user email.
func (u *UserInfoResolver) Email() string {
	return u.SessionCtx.Claims.GetUserClaims().Email
}

// Picture returns the users picture/avatar.
func (u *UserInfoResolver) Picture() string {
	return "UNKNOWN"
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
