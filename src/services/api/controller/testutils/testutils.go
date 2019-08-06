package testutils

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/services/api/apienv"
	"pixielabs.ai/pixielabs/src/services/auth/proto/mock"
)

// CreateTestGatewayEnv creates a test environment and mock auth client.
func CreateTestAPIEnv(t *testing.T) (apienv.APIEnv, *mock_auth.MockAuthServiceClient, func()) {
	ctrl := gomock.NewController(t)
	viper.Set("session_key", "fake-session-key")
	viper.Set("jwt_signing_key", "jwt-key")
	mockAuthClient := mock_auth.NewMockAuthServiceClient(ctrl)
	apiEnv, err := apienv.New(mockAuthClient)
	if err != nil {
		t.Fatal("failed to init api env")
	}
	cleanup := func() {
		ctrl.Finish()
	}

	return apiEnv, mockAuthClient, cleanup
}
