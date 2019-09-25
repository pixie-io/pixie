package apienv

import (
	"errors"

	redistore "github.com/boj/redistore"
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
	pflag.String("redis_address", "", "Address of redis to store the session keys (optional, defaults to in memory)")
	pflag.String("redis_network", "tcp", "If redis_address is set, network to use (optional)")
	pflag.Int("redis_max_idle_connections", 10, "Max idle connections for Redis (optional)")
}

// APIEnv store the contextual authenv used for API server requests.
type APIEnv interface {
	env.Env
	CookieStore() sessions.Store
	AuthClient() authpb.AuthServiceClient
	SiteManagerClient() sitemanagerpb.SiteManagerServiceClient
	ProfileClient() profilepb.ProfileServiceClient
	VZMgrClient() vzmgrpb.VZMgrServiceClient
}

// Impl is an implementation of the APIEnv interface.
type Impl struct {
	*env.BaseEnv
	cookieStore       sessions.Store
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

	var sessionStore sessions.Store
	redisAddress := viper.GetString("redis_address")
	if len(redisAddress) == 0 {
		sessionStore = sessions.NewCookieStore([]byte(sessionKey))
	} else {
		network := viper.GetString("redis_network")
		maxIdle := viper.GetInt("redis_max_idle_connections")
		store, err := redistore.NewRediStore(maxIdle, network, redisAddress, "", []byte(sessionKey))
		if err != nil {
			return nil, err
		}
		sessionStore = store
	}

	return &Impl{env.New(), sessionStore, ac, sc, pc, vc}, nil
}

// CookieStore returns the CookieStore from the environment.
func (e *Impl) CookieStore() sessions.Store {
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
