package apienv

import (
	"errors"

	"github.com/gorilla/sessions"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb"
	vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/env"
)

func init() {
	pflag.String("session_key", "", "Cookie session key")
}

// APIEnv store the contextual authenv used for API server requests.
type APIEnv interface {
	env.Env
	CookieStore() *sessions.CookieStore
	AuthClient() authpb.AuthServiceClient
	SiteManagerClient() sitemanagerpb.SiteManagerServiceClient
	ProfileClient() profilepb.ProfileServiceClient
	VZMgrClient() vzmgrpb.VZMgrServiceClient
}

// Impl is an implementation of the APIEnv interface.
type Impl struct {
	*env.BaseEnv
	cookieStore       *sessions.CookieStore
	authClient        authpb.AuthServiceClient
	siteManagerClient sitemanagerpb.SiteManagerServiceClient
	profileClient     profilepb.ProfileServiceClient
	vzMgrClient       vzmgrpb.VZMgrServiceClient
}

// New creates a new api env.
func New(ac authpb.AuthServiceClient, sc sitemanagerpb.SiteManagerServiceClient, pc profilepb.ProfileServiceClient, vc vzmgrpb.VZMgrServiceClient) (APIEnv, error) {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		return nil, errors.New("session_key is required for cookie store")
	}
	sessionStore := sessions.NewCookieStore([]byte(sessionKey))
	return &Impl{env.New(), sessionStore, ac, sc, pc, vc}, nil
}

// CookieStore returns the CookieStore from the environment.
func (e *Impl) CookieStore() *sessions.CookieStore {
	return e.cookieStore

}

// AuthClient returns an auth service client.
func (e *Impl) AuthClient() authpb.AuthServiceClient {
	return e.authClient
}

// SiteManagerClient returns an site manager  service client.
func (e *Impl) SiteManagerClient() sitemanagerpb.SiteManagerServiceClient {
	return e.siteManagerClient
}

// ProfileClient returns a profile service client.
func (e *Impl) ProfileClient() profilepb.ProfileServiceClient {
	return e.profileClient
}

// VZMgrClient returns a vzmgr client.
func (e *Impl) VZMgrClient() vzmgrpb.VZMgrServiceClient {
	return e.vzMgrClient
}
