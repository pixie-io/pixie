package controllers

// Server defines an gRPC server type.
type Server struct {
	a Auth0Connector
}

// NewServer creates GRPC handlers.
func NewServer(a Auth0Connector) (*Server, error) {
	return &Server{
		a: a,
	}, nil
}
