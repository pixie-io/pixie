package controllers

import "pixielabs.ai/pixielabs/src/cloud/auth/authenv"

// Server defines an gRPC server type.
type Server struct {
	env authenv.AuthEnv
	a   Auth0Connector
}

// NewServer creates GRPC handlers.
func NewServer(env authenv.AuthEnv, a Auth0Connector) (*Server, error) {
	return &Server{
		env: env,
		a:   a,
	}, nil
}
