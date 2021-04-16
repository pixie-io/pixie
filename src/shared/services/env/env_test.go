package env_test

import (
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/shared/services/env"
)

func TestNew(t *testing.T) {
	viper.Set("jwt_signing_key", "the-jwt-key")

	env := env.New("audience")
	assert.Equal(t, "the-jwt-key", env.JWTSigningKey())
	assert.Equal(t, "audience", env.Audience())
}
