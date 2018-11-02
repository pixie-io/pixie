package gwenv_test

import (
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/services/gateway/gwenv"
)

func TestNew(t *testing.T) {
	viper.Set("session_key", "a-key")
	env, err := gwenv.New(nil)
	assert.Nil(t, err)
	assert.NotNil(t, env)
	assert.NotNil(t, env.CookieStore())
}

func TestNew_MissingSessionKey(t *testing.T) {
	viper.Set("session_key", "")
	env, err := gwenv.New(nil)
	assert.NotNil(t, err)
	assert.Nil(t, env)
}
