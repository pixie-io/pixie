package authenv

import (
	"github.com/spf13/viper"

	profilepb "px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/env"
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
	pc, err := newProfileServiceClient()
	if err != nil {
		return nil, err
	}

	return New(pc)
}

// New creates a new auth authenv.
func New(pc profilepb.ProfileServiceClient) (*Impl, error) {
	return &Impl{env.New(viper.GetString("domain_name")), pc}, nil
}

// ProfileClient returns the authenv's profile client.
func (e *Impl) ProfileClient() profilepb.ProfileServiceClient {
	return e.profileClient
}
