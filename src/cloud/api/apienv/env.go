/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package apienv

import (
	"errors"
	"net/http"

	"github.com/gorilla/sessions"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/auth/authpb"
	"px.dev/pixie/src/cloud/config_manager/configmanagerpb"
	"px.dev/pixie/src/cloud/plugin/pluginpb"
	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/services/env"
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
	OrgClient() profilepb.OrgServiceClient
	VZMgrClient() vzmgrpb.VZMgrServiceClient
	VZDeploymentKeyClient() vzmgrpb.VZDeploymentKeyServiceClient
	APIKeyClient() authpb.APIKeyServiceClient
	ArtifactTrackerClient() artifacttrackerpb.ArtifactTrackerClient
	IdentityProviderClient() IdentityProviderClient
	PluginClient() pluginpb.PluginServiceClient
	DataRetentionPluginClient() pluginpb.DataRetentionPluginServiceClient
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
	orgClient              profilepb.OrgServiceClient
	vzDeployKeyClient      vzmgrpb.VZDeploymentKeyServiceClient
	apiKeyClient           authpb.APIKeyServiceClient
	vzMgrClient            vzmgrpb.VZMgrServiceClient
	artifactTrackerClient  artifacttrackerpb.ArtifactTrackerClient
	identityProviderClient IdentityProviderClient
	configClient           configmanagerpb.ConfigManagerServiceClient
	pluginClient           pluginpb.PluginServiceClient
	retentionClient        pluginpb.DataRetentionPluginServiceClient
}

// New creates a new api env.
func New(ac authpb.AuthServiceClient, pc profilepb.ProfileServiceClient, oc profilepb.OrgServiceClient,
	vk vzmgrpb.VZDeploymentKeyServiceClient, ak authpb.APIKeyServiceClient, vc vzmgrpb.VZMgrServiceClient,
	at artifacttrackerpb.ArtifactTrackerClient, oa IdentityProviderClient,
	cm configmanagerpb.ConfigManagerServiceClient, pm pluginpb.PluginServiceClient, rm pluginpb.DataRetentionPluginServiceClient) (APIEnv, error) {
	sessionKey := viper.GetString("session_key")
	if len(sessionKey) == 0 {
		return nil, errors.New("session_key is required for cookie store")
	}

	sessionStore := sessions.NewCookieStore([]byte(sessionKey))
	return &Impl{env.New(viper.GetString("domain_name")), sessionStore, ac, pc, oc, vk, ak, vc, at, oa, cm, pm, rm}, nil
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

// OrgClient returns a org service client.
func (e *Impl) OrgClient() profilepb.OrgServiceClient {
	return e.orgClient
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

// PluginClient returns a plugin client.
func (e *Impl) PluginClient() pluginpb.PluginServiceClient {
	return e.pluginClient
}

// DataRetentionPluginClient returns a data retention plugin client.
func (e *Impl) DataRetentionPluginClient() pluginpb.DataRetentionPluginServiceClient {
	return e.retentionClient
}
