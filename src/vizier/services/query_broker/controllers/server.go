package controllers

import (
	"context"
	"errors"
	"sync"

	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

// Server defines an gRPC server type.
type Server struct {
	env       querybrokerenv.QueryBrokerEnv
	mdsClient metadatapb.MetadataServiceClient
	natsConn  *nats.Conn

	newExecutor func(*nats.Conn, uuid.UUID, *[]uuid.UUID) Executor
	executors   map[uuid.UUID]Executor
	// Mutex is used for managing query executor instances.
	mux sync.Mutex
}

// NewServer creates GRPC handlers.
func NewServer(env querybrokerenv.QueryBrokerEnv, mdsClient metadatapb.MetadataServiceClient, natsConn *nats.Conn) (*Server, error) {
	return newServer(env, mdsClient, natsConn, NewQueryExecutor)
}

func newServer(env querybrokerenv.QueryBrokerEnv, mdsClient metadatapb.MetadataServiceClient, natsConn *nats.Conn, newExecutor func(*nats.Conn, uuid.UUID, *[]uuid.UUID) Executor) (*Server, error) {
	return &Server{
		env:         env,
		mdsClient:   mdsClient,
		natsConn:    natsConn,
		newExecutor: newExecutor,
		executors:   make(map[uuid.UUID]Executor),
	}, nil
}

func (s *Server) trackExecutorForQuery(executor Executor) {
	s.mux.Lock()
	defer s.mux.Unlock()

	s.executors[executor.GetQueryID()] = executor
}

func (s *Server) deleteExecutorForQuery(queryID uuid.UUID) {
	s.mux.Lock()
	defer s.mux.Unlock()

	delete(s.executors, queryID)
}

// ExecuteQuery executes a query on multiple agents and compute node.
func (s *Server) ExecuteQuery(ctx context.Context, req *querybrokerpb.QueryRequest) (*querybrokerpb.VizierQueryResponse, error) {
	// Get all available agents. In the future, we would want to update this to get only the agents relevant to the query.
	mdsReq := &metadatapb.AgentInfoRequest{}
	mdsResp, err := s.mdsClient.GetAgentInfo(ctx, mdsReq)
	if err != nil {
		return nil, err
	}

	var agentList []uuid.UUID

	for _, info := range mdsResp.Info {
		agentIDPB := info.Info.AgentID
		agentID, err := utils.UUIDFromProto(agentIDPB)
		if err != nil {
			return nil, err
		}
		agentList = append(agentList, agentID)
	}

	queryID := uuid.NewV4()
	queryExecutor := s.newExecutor(s.natsConn, queryID, &agentList)

	s.trackExecutorForQuery(queryExecutor)
	queryExecutor.ExecuteQuery(req.QueryStr)

	responses, err := queryExecutor.WaitForCompletion()
	if err != nil {
		return nil, err
	}

	queryResponse := &querybrokerpb.VizierQueryResponse{
		Responses: responses,
	}

	s.deleteExecutorForQuery(queryID)

	return queryResponse, nil
}

// GetSchemas returns the schemas in the system.
func (s *Server) GetSchemas(ctx context.Context, req *querybrokerpb.SchemaRequest) (*querybrokerpb.SchemaResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// GetAgentInfo returns information about registered agents.
func (s *Server) GetAgentInfo(ctx context.Context, req *querybrokerpb.AgentInfoRequest) (*querybrokerpb.AgentInfoResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// ReceiveAgentQueryResult gets the query result from an agent and stores the results until all
// relevant agents have responded.
func (s *Server) ReceiveAgentQueryResult(ctx context.Context, req *querybrokerpb.AgentQueryResultRequest) (*querybrokerpb.AgentQueryResultResponse, error) {
	queryIDPB := req.Result.QueryID
	queryID, err := utils.UUIDFromProto(queryIDPB)
	if err != nil {
		return nil, err
	}

	setExecutor := func(queryID uuid.UUID) (Executor, bool) {
		s.mux.Lock()
		exec, ok := s.executors[queryID]
		defer s.mux.Unlock()
		return exec, ok
	}

	exec, ok := setExecutor(queryID)

	if !ok {
		return nil, errors.New("Query ID not present")
	}

	exec.AddResult(req)

	queryResponse := &querybrokerpb.AgentQueryResultResponse{}

	return queryResponse, nil
}
