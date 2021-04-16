package idmanager

import "context"

// CreateInviteLinkRequest is the request value for the IdentityProvider interface.
type CreateInviteLinkRequest struct {
	Email    string
	PLOrgID  string
	PLUserID string
}

// CreateInviteLinkResponse is the response value for the IdentityProvider interface.
type CreateInviteLinkResponse struct {
	Email      string
	InviteLink string
}

// Manager is the interface for an identity provider's user management API.
type Manager interface {
	CreateInviteLink(ctx context.Context, req *CreateInviteLinkRequest) (*CreateInviteLinkResponse, error)
}
