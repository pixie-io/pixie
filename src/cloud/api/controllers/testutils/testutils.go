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

package testutils

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"

	mock_cloudpb "px.dev/pixie/src/api/proto/cloudpb/mock"
	"px.dev/pixie/src/cloud/api/apienv"
	"px.dev/pixie/src/cloud/api/controllers"
	mock_artifacttrackerpb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb/mock"
	mock_auth "px.dev/pixie/src/cloud/auth/authpb/mock"
	mock_configmanagerpb "px.dev/pixie/src/cloud/config_manager/configmanagerpb/mock"
	mock_pluginpb "px.dev/pixie/src/cloud/plugin/pluginpb/mock"
	mock_profilepb "px.dev/pixie/src/cloud/profile/profilepb/mock"
	mock_vzmgrpb "px.dev/pixie/src/cloud/vzmgr/vzmgrpb/mock"
)

// MockCloudClients provides the mock grpc clients for the graphql test env.
type MockCloudClients struct {
	MockArtifact          *mock_cloudpb.MockArtifactTrackerServer
	MockVizierClusterInfo *mock_cloudpb.MockVizierClusterInfoServer
	MockVizierDeployKey   *mock_cloudpb.MockVizierDeploymentKeyManagerServer
	MockScriptMgr         *mock_cloudpb.MockScriptMgrServer
	MockAutocomplete      *mock_cloudpb.MockAutocompleteServiceServer
	MockOrg               *mock_cloudpb.MockOrganizationServiceServer
	MockUser              *mock_cloudpb.MockUserServiceServer
	MockAPIKey            *mock_cloudpb.MockAPIKeyManagerServer
	MockPlugin            *mock_cloudpb.MockPluginServiceServer
}

// CreateTestGraphQLEnv creates a test graphql environment and mock clients.
func CreateTestGraphQLEnv(t *testing.T) (controllers.GraphQLEnv, *MockCloudClients, func()) {
	ctrl := gomock.NewController(t)
	ats := mock_cloudpb.NewMockArtifactTrackerServer(ctrl)
	vcs := mock_cloudpb.NewMockVizierClusterInfoServer(ctrl)
	aps := mock_cloudpb.NewMockAPIKeyManagerServer(ctrl)
	vds := mock_cloudpb.NewMockVizierDeploymentKeyManagerServer(ctrl)
	sms := mock_cloudpb.NewMockScriptMgrServer(ctrl)
	as := mock_cloudpb.NewMockAutocompleteServiceServer(ctrl)
	os := mock_cloudpb.NewMockOrganizationServiceServer(ctrl)
	us := mock_cloudpb.NewMockUserServiceServer(ctrl)
	ps := mock_cloudpb.NewMockPluginServiceServer(ctrl)
	gqlEnv := controllers.GraphQLEnv{
		APIKeyMgr:             aps,
		ArtifactTrackerServer: ats,
		VizierClusterInfo:     vcs,
		VizierDeployKeyMgr:    vds,
		ScriptMgrServer:       sms,
		AutocompleteServer:    as,
		OrgServer:             os,
		UserServer:            us,
		PluginServer:          ps,
	}
	return gqlEnv, &MockCloudClients{
		MockAPIKey:            aps,
		MockArtifact:          ats,
		MockVizierClusterInfo: vcs,
		MockVizierDeployKey:   vds,
		MockScriptMgr:         sms,
		MockAutocomplete:      as,
		MockOrg:               os,
		MockUser:              us,
		MockPlugin:            ps,
	}, ctrl.Finish
}

// MockAPIClients is a struct containing all of the mock clients for the api env.
type MockAPIClients struct {
	MockAuth                *mock_auth.MockAuthServiceClient
	MockProfile             *mock_profilepb.MockProfileServiceClient
	MockOrg                 *mock_profilepb.MockOrgServiceClient
	MockVzDeployKey         *mock_vzmgrpb.MockVZDeploymentKeyServiceClient
	MockAPIKey              *mock_auth.MockAPIKeyServiceClient
	MockVzMgr               *mock_vzmgrpb.MockVZMgrServiceClient
	MockArtifact            *mock_artifacttrackerpb.MockArtifactTrackerClient
	MockConfigMgr           *mock_configmanagerpb.MockConfigManagerServiceClient
	MockPlugin              *mock_pluginpb.MockPluginServiceClient
	MockDataRetentionPlugin *mock_pluginpb.MockDataRetentionPluginServiceClient
}

// CreateTestAPIEnv creates a test environment and mock clients.
func CreateTestAPIEnv(t *testing.T) (apienv.APIEnv, *MockAPIClients, func()) {
	ctrl := gomock.NewController(t)
	viper.Set("session_key", "fake-session-key")
	viper.Set("jwt_signing_key", "jwt-key")
	viper.Set("domain_name", "withpixie.ai")

	mockAuthClient := mock_auth.NewMockAuthServiceClient(ctrl)
	mockProfileClient := mock_profilepb.NewMockProfileServiceClient(ctrl)
	mockOrgClient := mock_profilepb.NewMockOrgServiceClient(ctrl)
	mockVzMgrClient := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	mockVzDeployKey := mock_vzmgrpb.NewMockVZDeploymentKeyServiceClient(ctrl)
	mockAPIKey := mock_auth.NewMockAPIKeyServiceClient(ctrl)
	mockArtifactTrackerClient := mock_artifacttrackerpb.NewMockArtifactTrackerClient(ctrl)
	mockConfigMgrClient := mock_configmanagerpb.NewMockConfigManagerServiceClient(ctrl)
	mockPluginClient := mock_pluginpb.NewMockPluginServiceClient(ctrl)
	mockRetentionClient := mock_pluginpb.NewMockDataRetentionPluginServiceClient(ctrl)
	apiEnv, err := apienv.New(mockAuthClient, mockProfileClient, mockOrgClient, mockVzDeployKey, mockAPIKey, mockVzMgrClient, mockArtifactTrackerClient, nil, mockConfigMgrClient, mockPluginClient, mockRetentionClient)
	if err != nil {
		t.Fatal("failed to init api env")
	}

	return apiEnv, &MockAPIClients{
		MockAuth:                mockAuthClient,
		MockProfile:             mockProfileClient,
		MockOrg:                 mockOrgClient,
		MockVzMgr:               mockVzMgrClient,
		MockAPIKey:              mockAPIKey,
		MockVzDeployKey:         mockVzDeployKey,
		MockArtifact:            mockArtifactTrackerClient,
		MockConfigMgr:           mockConfigMgrClient,
		MockPlugin:              mockPluginClient,
		MockDataRetentionPlugin: mockRetentionClient,
	}, ctrl.Finish
}
