package authenv

import (
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/shared/services/env"
)

// AuthEnv is the authenv use for the Authentication service.
type AuthEnv interface {
	env.Env
	ProfileClient() profilepb.ProfileServiceClient
}

// Impl is an implementation of the AuthEnv interface
type Impl struct {
	*env.BaseEnv
	profileClient profilepb.ProfileServiceClient
}

// NewWithDefaults creates a new auth authenv with defaults.
func NewWithDefaults() (*Impl, error) {
	client, err := newProfileServiceClient()
	if err != nil {
		return nil, err
	}
	return New(client)
}

// New creates a new auth authenv.
func New(client profilepb.ProfileServiceClient) (*Impl, error) {
	return &Impl{env.New(), client}, nil
}

// ProfileClient returns the authenv's profile client.
func (e *Impl) ProfileClient() profilepb.ProfileServiceClient {
	return e.profileClient
}
