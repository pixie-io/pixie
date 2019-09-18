package controllers

import (
	"context"
	"errors"
	"fmt"
	"sync"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/carnot/compiler"
	"pixielabs.ai/pixielabs/src/carnot/compiler/compilerpb"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
)

// Compiler describes the interface for any compiler.
type Compiler interface {
	Compile(schema *schemapb.Schema, query string) (*compilerpb.CompilerResult, error)
	Free()
}

// Executor is the interface for a query executor.
type Executor interface {
	ExecuteQuery(planMap map[uuid.UUID]*planpb.Plan) error
	WaitForCompletion() ([]*querybrokerpb.VizierQueryResponse_ResponseByAgent, error)
	AddResult(res *querybrokerpb.AgentQueryResultRequest)
	GetQueryID() uuid.UUID
}

// Server defines an gRPC server type.
type Server struct {
	env         querybrokerenv.QueryBrokerEnv
	mdsClient   metadatapb.MetadataServiceClient
	natsConn    *nats.Conn
	newExecutor func(*nats.Conn, uuid.UUID, *[]uuid.UUID) Executor
	compiler    Compiler
	executors   map[uuid.UUID]Executor
	// Mutex is used for managing query executor instances.
	mux sync.Mutex
}

// NewServer creates GRPC handlers.
func NewServer(env querybrokerenv.QueryBrokerEnv, mdsClient metadatapb.MetadataServiceClient, natsConn *nats.Conn, compiler Compiler) (*Server, error) {
	return newServer(env, mdsClient, natsConn, NewQueryExecutor, compiler)
}

func newServer(env querybrokerenv.QueryBrokerEnv,
	mdsClient metadatapb.MetadataServiceClient,
	natsConn *nats.Conn,
	newExecutor func(*nats.Conn, uuid.UUID, *[]uuid.UUID) Executor,
	compiler Compiler) (*Server, error) {

	return &Server{
		env:         env,
		mdsClient:   mdsClient,
		natsConn:    natsConn,
		newExecutor: newExecutor,
		compiler:    compiler,
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

func failedStatusQueryResponse(status *statuspb.Status) *querybrokerpb.VizierQueryResponse {
	queryResponse := &querybrokerpb.VizierQueryResponse{
		Status: status,
	}
	return queryResponse
}

func formatCompilerError(status *statuspb.Status) (string, error) {
	var errorPB compilerpb.CompilerErrorGroup
	err := compiler.GetCompilerErrorContext(status, &errorPB)
	if err != nil {
		return "", err
	}
	return proto.MarshalTextString(&errorPB), nil
}

// ExecuteQuery executes a query on multiple agents and compute node.
func (s *Server) ExecuteQuery(ctx context.Context, req *querybrokerpb.QueryRequest) (*querybrokerpb.VizierQueryResponse, error) {
	// Get the table schema that is presumably shared across agents.
	mdsSchemaReq := &metadatapb.SchemaRequest{}
	mdsSchemaResp, err := s.mdsClient.GetSchemas(ctx, mdsSchemaReq)
	if err != nil {
		log.WithError(err).Fatal("Failed to get schemas.")
		return nil, err
	}
	schema := mdsSchemaResp.Schema

	// Get all available agents for now.
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

	// TODO(philkuz) (PL-851) Pass physical state to Compile()
	// TODO(philkuz) (PL-851) Make compile return a physical plan instead.
	// Compile Logical Plan
	compilerResultPB, err := s.compiler.Compile(schema, req.QueryStr)
	if err != nil {
		return nil, err
	}

	// TODO(philkuz/michelle) do we want to return the problem through the error
	// or through the message? I've done both for now because the error route is
	// the only one that appears in the front end.
	if compilerResultPB.Status.ErrCode != statuspb.OK {
		errorStr, err := formatCompilerError(compilerResultPB.Status)
		if err != nil {
			log.WithError(err).Errorf("Couldn't get the compiler status for message: %s", proto.MarshalTextString(compilerResultPB.Status))
			return nil, fmt.Errorf("Error occurred without line and column: '%s'", compilerResultPB.Status.Msg)
		}
		return failedStatusQueryResponse(compilerResultPB.Status), fmt.Errorf(errorStr)
	}

	// Temporary "query fragment resolver"
	planMap := make(map[uuid.UUID]*planpb.Plan)

	// TODO(philkuz) (PL-851) rewrite this loop when we have a physical plan returned instead.
	for _, agentID := range agentList {
		planMap[agentID] = compilerResultPB.LogicalPlan
	}

	queryID := uuid.NewV4()
	queryExecutor := s.newExecutor(s.natsConn, queryID, &agentList)

	s.trackExecutorForQuery(queryExecutor)

	if err := queryExecutor.ExecuteQuery(planMap); err != nil {
		return nil, err
	}

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
	mdsReq := &metadatapb.AgentInfoRequest{}
	mdsResp, err := s.mdsClient.GetAgentInfo(ctx, mdsReq)
	if err != nil {
		return nil, err
	}

	var agentStatuses []*querybrokerpb.AgentStatus

	for _, agentInfo := range mdsResp.Info {
		agentStatuses = append(agentStatuses, &querybrokerpb.AgentStatus{
			Info: &querybrokerpb.AgentInfo{
				AgentID: agentInfo.Info.AgentID,
				HostInfo: &querybrokerpb.HostInfo{
					Hostname: agentInfo.Info.HostInfo.Hostname,
				},
			},
			LastHeartbeatNs: agentInfo.LastHeartbeatNs,
			State:           querybrokerpb.AGENT_STATE_HEALTHY,
		})
	}

	// Map the responses.
	resp := &querybrokerpb.AgentInfoResponse{
		Info: agentStatuses,
	}

	return resp, nil
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
