package querybrokerenv

import "pixielabs.ai/pixielabs/src/shared/services/env"

// QueryBrokerEnv is the interface for the Query Broker service environment.
type QueryBrokerEnv interface {
	// The address of the query broker
	Address() string
	// The SSL target hostname of the query broker
	SSLTargetName() string
	env.Env
}

// Impl is an implementation of the ApiEnv interface
type Impl struct {
	address       string
	sslTargetName string
	*env.BaseEnv
}

// New creates a new api env.
func New(qbAddress string, sslTargetName string, audience string) (*Impl, error) {
	return &Impl{
		qbAddress,
		sslTargetName,
		env.New(audience),
	}, nil
}

// Address returns the address of the query broker.
func (e *Impl) Address() string {
	return e.address
}

// SSLTargetName returns the SSL target hostname of the query broker.
func (e *Impl) SSLTargetName() string {
	return e.sslTargetName
}
