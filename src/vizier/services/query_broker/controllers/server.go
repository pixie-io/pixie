package controllers

import (
	"context"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/env"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

// Server defines an gRPC server type.
type Server struct {
	env querybrokerenv.QueryBrokerEnv
}

// NewServer creates GRPC handlers.
func NewServer(env querybrokerenv.QueryBrokerEnv) (*Server, error) {
	return &Server{
		env: env,
	}, nil
}

// ExecuteQuery executes a query on multiple agents and compute node.
func (s *Server) ExecuteQuery(ctx context.Context, req *querybrokerpb.QueryRequest) (*querybrokerpb.VizierQueryResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// GetSchemas returns the schemas in the system.
func (s *Server) GetSchemas(ctx context.Context, req *querybrokerpb.SchemaRequest) (*querybrokerpb.SchemaResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// GetAgentInfo returns information about registered agents.
func (s *Server) GetAgentInfo(ctx context.Context, req *querybrokerpb.AgentInfoRequest) (*querybrokerpb.AgentInfoResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}
