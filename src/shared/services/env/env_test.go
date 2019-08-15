package env_test

import (
	"testing"

	"pixielabs.ai/pixielabs/src/shared/services/env"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
)

func TestNew(t *testing.T) {
	viper.Set("jwt_signing_key", "the-jwt-key")

	env := env.New()
	assert.Equal(t, "the-jwt-key", env.JWTSigningKey())
}
