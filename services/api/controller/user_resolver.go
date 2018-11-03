package controller

import (
	"github.com/graph-gophers/graphql-go"
	"pixielabs.ai/pixielabs/services/common/sessioncontext"
)

// UserInfoResolver resolves user information.
type UserInfoResolver struct {
	SessionCtx *sessioncontext.SessionContext
}

// ID returns the user id.
func (u *UserInfoResolver) ID() graphql.ID {
	return graphql.ID(u.SessionCtx.Claims.UserID)
}

// Name returns the user name.
func (u *UserInfoResolver) Name() string {
	return "UNKNOWN"
}

// Email returns the user email.
func (u *UserInfoResolver) Email() string {
	return u.SessionCtx.Claims.Email
}

// Picture returns the users picture/avatar.
func (u *UserInfoResolver) Picture() string {
	return "UNKNOWN"
}
