package apienv

import (
	"errors"
	"net/http"

	"github.com/gorilla/sessions"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb"
	"pixielabs.ai/pixielabs/src/cloud/auth/authpb"
	profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/shared/services/env"
)

func init() {
	pflag.String("session_key", "", "Cookie session key")
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
	IdentityProviderClient() IdentityProviderClient
}

// IdentityProviderClient is the interface for IdentityProvider clients that require endpoints.
type IdentityProviderClient interface {
	// HandleLogin handles the login for a user into the Identity Provider.
	HandleLogin(session *sessions.Session, w http.ResponseWriter, r *http.Request) error
	// The key to use for the session.
	SessionKey() string
}

// Impl is an implementation of the APIEnv interface.
type Impl struct {
	*env.BaseEnv
	cookieStore            sessions.Store
	authClient             authpb.AuthServiceClient
	profileClient          profilepb.ProfileServiceClient
	vzDeployKeyClient      vzmgrpb.VZDeploymentKeyServiceClient
	apiKeyClient           authpb.APIKeyServiceClient
	vzMgrClient            vzmgrpb.VZMgrServiceClient
	artifactTrackerClient  artifacttrackerpb.ArtifactTrackerClient
	identityProviderClient IdentityProviderClient
}

// New creates a new api env.
func New(ac authpb.AuthServiceClient, pc profilepb.ProfileServiceClient,
	vk vzmgrpb.VZDeploymentKeyServiceClient, ak authpb.APIKeyServiceClient, vc vzmgrpb.VZMgrServiceClient,
	at artifacttrackerpb.ArtifactTrackerClient, oa IdentityProviderClient) (APIEnv, error) {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		return nil, errors.New("session_key is required for cookie store")
	}

	sessionStore := sessions.NewCookieStore([]byte(sessionKey))
	return &Impl{env.New(viper.GetString("domain_name")), sessionStore, ac, pc, vk, ak, vc, at, oa}, nil
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

// IdentityProviderClient returns a client that interfaces with an identity provider.
func (e *Impl) IdentityProviderClient() IdentityProviderClient {
	return e.identityProviderClient
}
