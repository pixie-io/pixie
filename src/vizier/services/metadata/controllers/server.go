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
	clock        utils.Clock
}

// NewServerWithClock creates a new server with a clock.
func NewServerWithClock(env metadataenv.MetadataEnv, agtMgr AgentManager, clock utils.Clock) (*Server, error) {
	return &Server{
		env:          env,
		agentManager: agtMgr,
		clock:        clock,
	}, nil
}

// NewServer creates GRPC handlers.
func NewServer(env metadataenv.MetadataEnv, agtMgr AgentManager) (*Server, error) {
	clock := utils.SystemClock{}
	return NewServerWithClock(env, agtMgr, clock)
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

	currentTime := s.clock.Now().UnixNano()

	// Populate AgentInfoResponse.
	agentResponses := make([]*metadatapb.AgentStatus, 0)
	for _, agent := range agents {
		uuidPb, err := utils.ProtoFromUUID(&agent.AgentID)
		if err != nil {
			log.WithError(err).Error("Could not parse proto from UUID")
			continue
		}
		resp := metadatapb.AgentStatus{
			Info: &metadatapb.AgentInfo{
				AgentID: uuidPb,
				HostInfo: &metadatapb.HostInfo{
					Hostname: agent.Hostname,
				},
			},
			LastHeartbeatNs: currentTime - agent.LastHeartbeatNS,
			State:           metadatapb.AGENT_STATE_HEALTHY,
			CreateTimeNs:    agent.CreateTimeNS,
		}
		agentResponses = append(agentResponses, &resp)
	}

	resp := metadatapb.AgentInfoResponse{
		Info: agentResponses,
	}

	return &resp, nil
}
