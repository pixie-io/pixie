package testutils

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/cloud/api/apienv"
	mock_auth "pixielabs.ai/pixielabs/src/cloud/auth/proto/mock"
	mock_sitemanagerpb "pixielabs.ai/pixielabs/src/cloud/site_manager/sitemanagerpb/mock"
)

// CreateTestAPIEnv creates a test environment and mock clients.
func CreateTestAPIEnv(t *testing.T) (apienv.APIEnv, *mock_auth.MockAuthServiceClient, *mock_sitemanagerpb.MockSiteManagerServiceClient, func()) {
	ctrl := gomock.NewController(t)
	viper.Set("session_key", "fake-session-key")
	viper.Set("jwt_signing_key", "jwt-key")

	mockAuthClient := mock_auth.NewMockAuthServiceClient(ctrl)
	mockSiteManagerClient := mock_sitemanagerpb.NewMockSiteManagerServiceClient(ctrl)
	apiEnv, err := apienv.New(mockAuthClient, mockSiteManagerClient)
	if err != nil {
		t.Fatal("failed to init api env")
	}
	cleanup := func() {
		ctrl.Finish()
	}

	return apiEnv, mockAuthClient, mockSiteManagerClient, cleanup
}
