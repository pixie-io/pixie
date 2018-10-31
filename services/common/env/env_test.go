package env_test

import (
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/services/common/env"
)

func TestNew(t *testing.T) {
	viper.Set("external_address", "http://external.test.com")
	viper.Set("jwt_signing_key", "the-jwt-key")

	env := env.New()
	assert.Equal(t, "http://external.test.com", env.ExternalAddress())
	assert.Equal(t, "the-jwt-key", env.JWTSigningKey())
}
