package querybrokerenv

import "pixielabs.ai/pixielabs/src/shared/services/env"

// QueryBrokerEnv is the interface for the Query Broker service environment.
type QueryBrokerEnv interface {
	// The address of the query broker
	Address() string
	env.Env
}

// Impl is an implementation of the ApiEnv interface
type Impl struct {
	address string
	*env.BaseEnv
}

// New creates a new api env.
func New(qbAddress string) (*Impl, error) {
	return &Impl{
		qbAddress,
		env.New(),
	}, nil
}

// Address returns the address of the query broker.
func (e *Impl) Address() string {
	return e.address
}
