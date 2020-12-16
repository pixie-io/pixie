package testutils

import (
	"fmt"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	"pixielabs.ai/pixielabs/src/cloud/api/controller"
	mock_artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb/mock"
	mock_auth "pixielabs.ai/pixielabs/src/cloud/auth/proto/mock"
	mock_cloudapipb "pixielabs.ai/pixielabs/src/cloud/cloudapipb/mock"
	mock_profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb/mock"
	mock_vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb/mock"
)

// MockCloudClients provides the mock grpc clients for the graphql test env.
type MockCloudClients struct {
	MockArtifact          *mock_cloudapipb.MockArtifactTrackerServer
	MockVizierClusterInfo *mock_cloudapipb.MockVizierClusterInfoServer
	MockVizierDeployKey   *mock_cloudapipb.MockVizierDeploymentKeyManagerServer
	MockScriptMgr         *mock_cloudapipb.MockScriptMgrServer
	MockAutocomplete      *mock_cloudapipb.MockAutocompleteServiceServer
}

// CreateTestGraphQLEnv creates a test graphql environment and mock clients.
func CreateTestGraphQLEnv(t *testing.T) (controller.GraphQLEnv, *MockCloudClients, func()) {
	ctrl := gomock.NewController(t)
	ats := mock_cloudapipb.NewMockArtifactTrackerServer(ctrl)
	vcs := mock_cloudapipb.NewMockVizierClusterInfoServer(ctrl)
	vds := mock_cloudapipb.NewMockVizierDeploymentKeyManagerServer(ctrl)
	sms := mock_cloudapipb.NewMockScriptMgrServer(ctrl)
	as := mock_cloudapipb.NewMockAutocompleteServiceServer(ctrl)
	gqlEnv := controller.GraphQLEnv{
		ArtifactTrackerServer: ats,
		VizierClusterInfo:     vcs,
		VizierDeployKeyMgr:    vds,
		ScriptMgrServer:       sms,
		AutocompleteServer:    as,
	}
	cleanup := func() {
		if r := recover(); r != nil {
			fmt.Println("Panicked with error: ", r)
		}
		ctrl.Finish()
	}
	return gqlEnv, &MockCloudClients{
		MockArtifact:          ats,
		MockVizierClusterInfo: vcs,
		MockVizierDeployKey:   vds,
		MockScriptMgr:         sms,
		MockAutocomplete:      as,
	}, cleanup
}

// MockAPIClients is a struct containing all of the mock clients for the api env.
type MockAPIClients struct {
	MockAuth        *mock_auth.MockAuthServiceClient
	MockProfile     *mock_profilepb.MockProfileServiceClient
	MockVzDeployKey *mock_vzmgrpb.MockVZDeploymentKeyServiceClient
	MockAPIKey      *mock_auth.MockAPIKeyServiceClient
	MockVzMgr       *mock_vzmgrpb.MockVZMgrServiceClient
	MockArtifact    *mock_artifacttrackerpb.MockArtifactTrackerClient
}

// CreateTestAPIEnv creates a test environment and mock clients.
func CreateTestAPIEnv(t *testing.T) (apienv.APIEnv, *MockAPIClients, func()) {
	ctrl := gomock.NewController(t)
	viper.Set("session_key", "fake-session-key")
	viper.Set("jwt_signing_key", "jwt-key")
	viper.Set("domain_name", "example.com")

	mockAuthClient := mock_auth.NewMockAuthServiceClient(ctrl)
	mockProfileClient := mock_profilepb.NewMockProfileServiceClient(ctrl)
	mockVzMgrClient := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	mockVzDeployKey := mock_vzmgrpb.NewMockVZDeploymentKeyServiceClient(ctrl)
	mockAPIKey := mock_auth.NewMockAPIKeyServiceClient(ctrl)
	mockArtifactTrackerClient := mock_artifacttrackerpb.NewMockArtifactTrackerClient(ctrl)
	apiEnv, err := apienv.New(mockAuthClient, mockProfileClient, mockVzDeployKey, mockAPIKey, mockVzMgrClient, mockArtifactTrackerClient)
	if err != nil {
		t.Fatal("failed to init api env")
	}
	cleanup := func() {
		if r := recover(); r != nil {
			fmt.Println("Panicked with error: ", r)
		}
		ctrl.Finish()
	}

	return apiEnv, &MockAPIClients{
		MockAuth:        mockAuthClient,
		MockProfile:     mockProfileClient,
		MockVzMgr:       mockVzMgrClient,
		MockAPIKey:      mockAPIKey,
		MockVzDeployKey: mockVzDeployKey,
		MockArtifact:    mockArtifactTrackerClient,
	}, cleanup
}
