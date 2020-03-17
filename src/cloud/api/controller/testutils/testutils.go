package testutils

import (
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

// CreateTestGraphQLEnv creates a test graphql environment and mock clients.
func CreateTestGraphQLEnv(t *testing.T) (controller.GraphQLEnv, *mock_cloudapipb.MockArtifactTrackerServiceServer, *mock_cloudapipb.MockVizierClusterServiceServer, func()) {
	ctrl := gomock.NewController(t)
	ats := mock_cloudapipb.NewMockArtifactTrackerServiceServer(ctrl)
	vcs := mock_cloudapipb.NewMockVizierClusterServiceServer(ctrl)
	gqlEnv := controller.GraphQLEnv{
		ArtifactTrackerServer: ats,
		VizierClusterServer:   vcs,
	}
	cleanup := func() {
		ctrl.Finish()
	}
	return gqlEnv, ats, vcs, cleanup
}

// CreateTestAPIEnv creates a test environment and mock clients.
func CreateTestAPIEnv(t *testing.T) (apienv.APIEnv, *mock_auth.MockAuthServiceClient, *mock_profilepb.MockProfileServiceClient, *mock_vzmgrpb.MockVZMgrServiceClient, *mock_artifacttrackerpb.MockArtifactTrackerClient, func()) {
	ctrl := gomock.NewController(t)
	viper.Set("session_key", "fake-session-key")
	viper.Set("jwt_signing_key", "jwt-key")
	viper.Set("domain_name", "example.com")

	mockAuthClient := mock_auth.NewMockAuthServiceClient(ctrl)
	mockProfileClient := mock_profilepb.NewMockProfileServiceClient(ctrl)
	mockVzMgrClient := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)
	mockArtifactTrackerClient := mock_artifacttrackerpb.NewMockArtifactTrackerClient(ctrl)
	apiEnv, err := apienv.New(mockAuthClient, mockProfileClient, mockVzMgrClient, mockArtifactTrackerClient)
	if err != nil {
		t.Fatal("failed to init api env")
	}
	cleanup := func() {
		ctrl.Finish()
	}

	return apiEnv, mockAuthClient, mockProfileClient, mockVzMgrClient, mockArtifactTrackerClient, cleanup
}
