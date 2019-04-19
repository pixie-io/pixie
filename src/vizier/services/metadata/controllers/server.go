package controllers

import (
	"context"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

// Server defines an gRPC server type.
type Server struct {
	env metadataenv.MetadataEnv
}

// NewServer creates GRPC handlers.
func NewServer(env metadataenv.MetadataEnv) (*Server, error) {
	return &Server{
		env: env,
	}, nil
}

// GetSchemas returns the schemas in the system.
func (s *Server) GetSchemas(ctx context.Context, req *metadatapb.SchemaRequest) (*metadatapb.SchemaResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// GetSchemaByAgent returns the schemas in the system.
func (s *Server) GetSchemaByAgent(ctx context.Context, req *metadatapb.SchemaByAgentRequest) (*metadatapb.SchemaByAgentResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// GetAgentInfo returns information about registered agents.
func (s *Server) GetAgentInfo(ctx context.Context, req *metadatapb.AgentInfoRequest) (*metadatapb.AgentInfoResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}
