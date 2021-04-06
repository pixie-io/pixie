package env

import (
	"github.com/spf13/viper"
)

// Env is the interface that all sub-environments should implement.
type Env interface {
	JWTSigningKey() string
	Audience() string
}

// BaseEnv is the struct containing server state that is valid across multiple sessions
// for example, database connections and config information.
type BaseEnv struct {
	jwtSigningKey string
	audience      string
}

// New creates a new base environment use by all our services.
func New(audience string) *BaseEnv {
	return &BaseEnv{
		jwtSigningKey: viper.GetString("jwt_signing_key"),
		audience:      audience,
	}
}

// JWTSigningKey returns the JWT key.
func (e *BaseEnv) JWTSigningKey() string {
	return e.jwtSigningKey
}

// Audience returns the audience that should be associated with any JWT keys.
func (e *BaseEnv) Audience() string {
	return e.audience
}
