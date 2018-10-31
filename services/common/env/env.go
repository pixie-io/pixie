package env

import (
	"github.com/spf13/viper"
)

// Env is the interface that all sub-environments should implement.
type Env interface {
	ExternalAddress() string
	JWTSigningKey() string
}

// BaseEnv is the struct containing server state that is valid across multiple sessions
// for example, database connections and config information.
type BaseEnv struct {
	externalAddress string
	jwtSigningKey   string
}

// New creates a new base environment use by all our services.
func New() *BaseEnv {
	return &BaseEnv{
		externalAddress: viper.GetString("external_address"),
		jwtSigningKey:   viper.GetString("jwt_signing_key"),
	}
}

// ExternalAddress returns the external address of the app.
func (e *BaseEnv) ExternalAddress() string {
	return e.externalAddress
}

// JWTSigningKey returns the JWT key.
func (e *BaseEnv) JWTSigningKey() string {
	return e.jwtSigningKey
}
