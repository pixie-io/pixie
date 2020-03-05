package testutils

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	mock_artifacttrackerpb "pixielabs.ai/pixielabs/src/cloud/artifact_tracker/artifacttrackerpb/mock"
	mock_auth "pixielabs.ai/pixielabs/src/cloud/auth/proto/mock"
	mock_profilepb "pixielabs.ai/pixielabs/src/cloud/profile/profilepb/mock"
	mock_vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb/mock"
)

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
