package authenv

import (
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/shared/services/env"
)

func init() {
	pflag.String("site_manager_service", "site-manager-service.plc.svc.cluster.local:50300", "The sitemanager service url (load balancer/list is ok)")
}

// AuthEnv is the authenv use for the Authentication service.
type AuthEnv interface {
	env.Env
	ProfileClient() profilepb.ProfileServiceClient
	SiteManagerClient() sitemanagerpb.SiteManagerServiceClient
}

// Impl is an implementation of the AuthEnv interface
type Impl struct {
	*env.BaseEnv
	profileClient     profilepb.ProfileServiceClient
	siteManagerClient sitemanagerpb.SiteManagerServiceClient
}

// NewWithDefaults creates a new auth authenv with defaults.
func NewWithDefaults() (*Impl, error) {
	pc, err := newProfileServiceClient()
	if err != nil {
		return nil, err
	}

	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}
	siteChannel, err := grpc.Dial(viper.GetString("site_manager_service"), dialOpts...)
	if err != nil {
		return nil, err
	}
	sc := sitemanagerpb.NewSiteManagerServiceClient(siteChannel)

	return New(pc, sc)
}

// New creates a new auth authenv.
func New(pc profilepb.ProfileServiceClient, sc sitemanagerpb.SiteManagerServiceClient) (*Impl, error) {
	return &Impl{env.New(), pc, sc}, nil
}

// ProfileClient returns the authenv's profile client.
func (e *Impl) ProfileClient() profilepb.ProfileServiceClient {
	return e.profileClient
}

// SiteManagerClient returns an site manager  service client.
func (e *Impl) SiteManagerClient() sitemanagerpb.SiteManagerServiceClient {
	return e.siteManagerClient
}
