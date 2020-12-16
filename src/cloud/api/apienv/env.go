package apienv

import (
	"errors"

	redistore "github.com/boj/redistore"
	"github.com/gorilla/sessions"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	authpb "pixielabs.ai/pixielabs/src/cloud/auth/proto"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
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
	ProfileClient() profilepb.ProfileServiceClient
	VZMgrClient() vzmgrpb.VZMgrServiceClient
	VZDeploymentKeyClient() vzmgrpb.VZDeploymentKeyServiceClient
	APIKeyClient() authpb.APIKeyServiceClient
	ArtifactTrackerClient() artifacttrackerpb.ArtifactTrackerClient
}

// Impl is an implementation of the APIEnv interface.
type Impl struct {
	*env.BaseEnv
	cookieStore           sessions.Store
	authClient            authpb.AuthServiceClient
	profileClient         profilepb.ProfileServiceClient
	vzDeployKeyClient     vzmgrpb.VZDeploymentKeyServiceClient
	apiKeyClient          authpb.APIKeyServiceClient
	vzMgrClient           vzmgrpb.VZMgrServiceClient
	artifactTrackerClient artifacttrackerpb.ArtifactTrackerClient
}

// New creates a new api env.
func New(ac authpb.AuthServiceClient, pc profilepb.ProfileServiceClient,
	vk vzmgrpb.VZDeploymentKeyServiceClient, ak authpb.APIKeyServiceClient, vc vzmgrpb.VZMgrServiceClient,
	at artifacttrackerpb.ArtifactTrackerClient) (APIEnv, error) {
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

	return &Impl{env.New(), sessionStore, ac, pc, vk, ak, vc, at}, nil
}

// CookieStore returns the CookieStore from the environment.
func (e *Impl) CookieStore() sessions.Store {
	return e.cookieStore
}

// AuthClient returns an auth service client.
func (e *Impl) AuthClient() authpb.AuthServiceClient {
	return e.authClient
}

// ProfileClient returns a profile service client.
func (e *Impl) ProfileClient() profilepb.ProfileServiceClient {
	return e.profileClient
}

// VZDeploymentKeyClient returns a Vizier deploy key client.
func (e *Impl) VZDeploymentKeyClient() vzmgrpb.VZDeploymentKeyServiceClient {
	return e.vzDeployKeyClient
}

// APIKeyClient returns a API key client.
func (e *Impl) APIKeyClient() authpb.APIKeyServiceClient {
	return e.apiKeyClient
}

// VZMgrClient returns a vzmgr client.
func (e *Impl) VZMgrClient() vzmgrpb.VZMgrServiceClient {
	return e.vzMgrClient
}

// ArtifactTrackerClient returns an artifact tracker client.
func (e *Impl) ArtifactTrackerClient() artifacttrackerpb.ArtifactTrackerClient {
	return e.artifactTrackerClient
}
