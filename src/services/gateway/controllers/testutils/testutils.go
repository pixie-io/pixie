package testutils

import (
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/spf13/viper"
	"pixielabs.ai/pixielabs/src/services/auth/proto/mock"
	"pixielabs.ai/pixielabs/src/services/gateway/gwenv"
)

// CreateTestGatewayEnv creates a test environment and mock auth client.
func CreateTestGatewayEnv(t *testing.T) (gwenv.GatewayEnv, *mock_auth.MockAuthServiceClient, func()) {
	ctrl := gomock.NewController(t)
	viper.Set("session_key", "fake-session-key")
	viper.Set("jwt_signing_key", "jwt-key")
	mockAuthClient := mock_auth.NewMockAuthServiceClient(ctrl)
	gw, err := gwenv.New(mockAuthClient)
	if err != nil {
		t.Fatal("failed to init gateway env")
	}
	cleanup := func() {
		ctrl.Finish()
	}

	return gw, mockAuthClient, cleanup
}
