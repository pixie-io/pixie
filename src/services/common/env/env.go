package env

import (
	"github.com/spf13/viper"
)

// Env is the interface that all sub-environments should implement.
type Env interface {
	JWTSigningKey() string
}

// BaseEnv is the struct containing server state that is valid across multiple sessions
// for example, database connections and config information.
type BaseEnv struct {
	jwtSigningKey string
}

// New creates a new base environment use by all our services.
func New() *BaseEnv {
	return &BaseEnv{
		jwtSigningKey: viper.GetString("jwt_signing_key"),
	}
}

// JWTSigningKey returns the JWT key.
func (e *BaseEnv) JWTSigningKey() string {
	return e.jwtSigningKey
}
