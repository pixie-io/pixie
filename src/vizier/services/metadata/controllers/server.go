package controllers

import (
	"context"

	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

// Server defines an gRPC server type.
type Server struct {
	env          metadataenv.MetadataEnv
	agentManager AgentManager
}

// NewServer creates GRPC handlers.
func NewServer(env metadataenv.MetadataEnv, agtMgr AgentManager) (*Server, error) {
	return &Server{
		env:          env,
		agentManager: agtMgr,
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
	agents, err := s.agentManager.GetActiveAgents()
	if err != nil {
		return nil, err
	}

	// Populate AgentInfoResponse.
	agentResponses := make([]*metadatapb.AgentStatus, 0)
	for _, agent := range agents {
		uuidPb, err := utils.ProtoFromUUID(&agent.AgentID)
		if err != nil {
			log.WithError(err).Error("Could not parse proto from UUID")
		}
		resp := metadatapb.AgentStatus{
			Info: &metadatapb.AgentInfo{
				AgentID: uuidPb,
				HostInfo: &metadatapb.HostInfo{
					Hostname: agent.Hostname,
				},
			},
			LastHeartbeatNs: agent.LastHeartbeatNS,
			State:           metadatapb.AGENT_STATE_HEALTHY,
		}
		agentResponses = append(agentResponses, &resp)
	}

	resp := metadatapb.AgentInfoResponse{
		Info: agentResponses,
	}

	return &resp, nil
}
