package controller

import (
	"context"
	"errors"

	"px.dev/pixie/src/cloud/profile/controller/idmanager"
)

// Auth0Manager implements the idmanager.Manager interface for Auth0.
type Auth0Manager struct{}

// NewAuth0Manager creates a new Auth0Manager.
func NewAuth0Manager() (*Auth0Manager, error) {
	return &Auth0Manager{}, nil
}

// CreateInviteLink implements the idmanager.Manager interface. We have yet to implement this flow so it fails in the mean time.
func (c *Auth0Manager) CreateInviteLink(context.Context, *idmanager.CreateInviteLinkRequest) (*idmanager.CreateInviteLinkResponse, error) {
	return nil, errors.New("pixie's Auth0 implementation does not support InviteLinks yet")
}
