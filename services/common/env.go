package common

type ctxKey string

// EnvKey is the context key used for the environment.
const EnvKey ctxKey = "Env"

// Env is the struct containing server state that might be relevant to
// executing a particualar API call (ie. DB to lookup user information).
type Env struct {
	ExternalAddress string
	SigningKey      string
}
