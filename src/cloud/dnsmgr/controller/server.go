package controller

import (
	"context"
	"errors"

	"pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrenv"
	dnsmgrpb "pixielabs.ai/pixielabs/src/cloud/dnsmgr/dnsmgrpb"
)

// Server is an implementation of GRPC server for dnsmgr service.
type Server struct {
	env dnsmgrenv.DNSMgrEnv
}

// NewServer creates a new GRPC dnsmgr server.
func NewServer(env dnsmgrenv.DNSMgrEnv) *Server {
	return &Server{env: env}
}

// GetDNSAddress assigns and returns a DNS address for the given cluster ID and IP.
func (s *Server) GetDNSAddress(ctx context.Context, req *dnsmgrpb.GetDNSAddressRequest) (*dnsmgrpb.GetDNSAddressResponse, error) {
	return nil, errors.New("not implemented")
}
