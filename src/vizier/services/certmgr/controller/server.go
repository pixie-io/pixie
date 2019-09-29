package controller

import (
	"context"
	"errors"

	"pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrenv"
	certmgrpb "pixielabs.ai/pixielabs/src/vizier/services/certmgr/certmgrpb"
)

// Server is an implementation of GRPC server for certmgr service.
type Server struct {
	env certmgrenv.CertMgrEnv
}

// NewServer creates a new GRPC certmgr server.
func NewServer(env certmgrenv.CertMgrEnv) *Server {
	return &Server{env: env}
}

// UpdateCerts updates the proxy certs with the given DNS address.
func (s *Server) UpdateCerts(ctx context.Context, req *certmgrpb.UpdateCertsRequest) (*certmgrpb.UpdateCertsResponse, error) {
	return nil, errors.New("not implemented")
}
