package controllers

import (
	"context"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerenv"
	"pixielabs.ai/pixielabs/src/services/site_manager/sitemanagerpb"
)

// Server defines an gRPC server type.
type Server struct {
	env sitemanagerenv.SiteManagerEnv
}

// NewServer creates GRPC handlers.
func NewServer(env sitemanagerenv.SiteManagerEnv) (*Server, error) {
	return &Server{
		env: env,
	}, nil
}

// IsSiteAvailable checks to see if a site domain is available.
func (s *Server) IsSiteAvailable(ctx context.Context, req *sitemanagerpb.IsSiteAvailableRequest) (*sitemanagerpb.IsSiteAvailableResponse, error) {
	return nil, status.Error(codes.Unimplemented, "not implemented yet")
}
