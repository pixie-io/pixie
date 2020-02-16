package controllers

import (
	"context"
	"errors"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/go-nats"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"

	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	plannerpb "pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	planpb "pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/udfspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	funcs "pixielabs.ai/pixielabs/src/vizier/funcs/export"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// Planner describes the interface for any planner.
type Planner interface {
	Plan(planState *distributedpb.LogicalPlannerState, req *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error)
	GetAvailableFlags(req *plannerpb.QueryRequest) (*plannerpb.GetAvailableFlagsResult, error)
	Free()
}

// Executor is the interface for a query executor.
type Executor interface {
	ExecuteQuery(planMap map[uuid.UUID]*planpb.Plan) error
	AddQueryPlanToResult(*distributedpb.DistributedPlan, map[uuid.UUID]*planpb.Plan) error
	WaitForCompletion() (*queryresultspb.QueryResult, error)
	AddResult(res *querybrokerpb.AgentQueryResultRequest)
	GetQueryID() uuid.UUID
}

// Server defines an gRPC server type.
type Server struct {
	env         querybrokerenv.QueryBrokerEnv
	mdsClient   metadatapb.MetadataServiceClient
	natsConn    *nats.Conn
	newExecutor func(*nats.Conn, uuid.UUID, *[]uuid.UUID) Executor
	executors   map[uuid.UUID]Executor
	// Mutex is used for managing query executor instances.
	mux sync.Mutex
}

// NewServer creates GRPC handlers.
func NewServer(env querybrokerenv.QueryBrokerEnv, mdsClient metadatapb.MetadataServiceClient, natsConn *nats.Conn) (*Server, error) {
	return newServer(env, mdsClient, natsConn, NewQueryExecutor)
}

func newServer(env querybrokerenv.QueryBrokerEnv,
	mdsClient metadatapb.MetadataServiceClient,
	natsConn *nats.Conn,
	newExecutor func(*nats.Conn, uuid.UUID, *[]uuid.UUID) Executor) (*Server, error) {

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

func failedStatusQueryResponse(queryID uuid.UUID, status *statuspb.Status) *querybrokerpb.VizierQueryResponse {
	queryIDPB := utils.ProtoFromUUID(&queryID)
	queryResponse := &querybrokerpb.VizierQueryResponse{
		QueryID: queryIDPB,
		Status:  status,
	}
	return queryResponse
}

func formatCompilerError(status *statuspb.Status) (string, error) {
	var errorPB compilerpb.CompilerErrorGroup
	err := logicalplanner.GetCompilerErrorContext(status, &errorPB)
	if err != nil {
		return "", err
	}
	return proto.MarshalTextString(&errorPB), nil
}

func makeAgentCarnotInfo(agentID uuid.UUID, asid uint32) *distributedpb.CarnotInfo {
	// TODO(philkuz) (PL-910) need to update this to also contain table info.
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		ASID:                 asid,
		HasGRPCServer:        false,
		HasDataStore:         true,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
	}
}

func makeKelvinCarnotInfo(agentID uuid.UUID, grpcAddress string, asid uint32) *distributedpb.CarnotInfo {
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		ASID:                 asid,
		HasGRPCServer:        true,
		GRPCAddress:          grpcAddress,
		HasDataStore:         false,
		ProcessesData:        true,
		AcceptsRemoteSources: true,
	}
}

func makePlannerState(pemInfo []*agentpb.Agent, kelvinList []*agentpb.Agent, schema *schemapb.Schema, planOpts *planpb.PlanOptions) (*distributedpb.LogicalPlannerState, error) {
	// TODO(philkuz) (PL-910) need to update this to pass table info.
	carnotInfoList := make([]*distributedpb.CarnotInfo, 0)
	for _, pem := range pemInfo {
		pemID := utils.UUIDFromProtoOrNil(pem.Info.AgentID)
		carnotInfoList = append(carnotInfoList, makeAgentCarnotInfo(pemID, pem.ASID))
	}

	for _, kelvin := range kelvinList {
		kelvinID := utils.UUIDFromProtoOrNil(kelvin.Info.AgentID)
		kelvinGRPCAddress := kelvin.Info.IPAddress
		carnotInfoList = append(carnotInfoList, makeKelvinCarnotInfo(kelvinID, kelvinGRPCAddress, kelvin.ASID))
	}

	plannerState := distributedpb.LogicalPlannerState{
		Schema: schema,
		DistributedState: &distributedpb.DistributedState{
			CarnotInfo: carnotInfoList,
		},
		PlanOptions: planOpts,
	}
	return &plannerState, nil
}

// ExecuteQueryWithPlanner executes a query with the provided planner.
func (s *Server) ExecuteQueryWithPlanner(ctx context.Context, req *plannerpb.QueryRequest, queryID uuid.UUID, planner Planner, planOpts *planpb.PlanOptions) (*querybrokerpb.VizierQueryResponse, error) {
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

	var kelvinList []*agentpb.Agent
	var pemList []*agentpb.Agent

	for _, m := range mdsResp.Info {
		if m.Agent.Info.Capabilities == nil || m.Agent.Info.Capabilities.CollectsData {
			pemList = append(pemList, m.Agent)
		} else {
			kelvinList = append(kelvinList, m.Agent)
		}
	}

	plannerState, err := makePlannerState(pemList, kelvinList, schema, planOpts)
	if err != nil {
		return nil, err
	}

	log.WithField("query_id", queryID).Infof("Running query: %s", req.QueryStr)
	start := time.Now()
	defer func(t time.Time) {
		duration := time.Now().Sub(t)
		log.WithField("query_id", queryID).WithField("duration", duration).Info("Executed query")
	}(start)

	// Compile the query plan.
	plannerResultPB, err := planner.Plan(plannerState, req)
	if err != nil {
		return nil, err
	}

	// When the status is not OK, this means it's a compilation error on the query passed in.
	if plannerResultPB.Status.ErrCode != statuspb.OK {
		return failedStatusQueryResponse(queryID, plannerResultPB.Status), nil
	}

	// Plan describes the mapping of agents to the plan that should execute on them.
	plan := plannerResultPB.Plan
	planMap := make(map[uuid.UUID]*planpb.Plan)

	for carnotID, agentPlan := range plan.QbAddressToPlan {
		u, err := uuid.FromString(carnotID)
		if err != nil {
			log.WithError(err).Fatalf("Couldn't parse uuid from agent id \"%s\"", carnotID)
			return nil, err
		}
		planMap[u] = agentPlan
	}

	pemIDs := make([]uuid.UUID, len(pemList))
	for i, p := range pemList {
		pemIDs[i] = utils.UUIDFromProtoOrNil(p.Info.AgentID)
	}

	queryExecutor := s.newExecutor(s.natsConn, queryID, &pemIDs)

	s.trackExecutorForQuery(queryExecutor)

	// TODO(zasgar): Cleanup this code to push the distrbuted plan into
	// ExecuteQuery directly and do the mapping in there.
	if plannerState.PlanOptions.Explain {
		if err := queryExecutor.AddQueryPlanToResult(plan, planMap); err != nil {
			log.WithError(err).Error("Failed to add query plan to result")
		}
	}

	if err := queryExecutor.ExecuteQuery(planMap); err != nil {
		return nil, err
	}

	queryResult, err := queryExecutor.WaitForCompletion()
	if err != nil {
		return nil, err
	}

	queryResponse := &querybrokerpb.VizierQueryResponse{
		QueryID:     utils.ProtoFromUUID(&queryID),
		QueryResult: queryResult,
	}

	s.deleteExecutorForQuery(queryID)
	return queryResponse, nil
}

func loadUDFInfo(udfInfoPb *udfspb.UDFInfo) error {
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if err != nil {
		return err
	}
	proto.Unmarshal(b, udfInfoPb)
	return nil
}

// ExecuteQuery executes a query on multiple agents and compute node.
func (s *Server) ExecuteQuery(ctx context.Context, req *plannerpb.QueryRequest) (*querybrokerpb.VizierQueryResponse, error) {
	// TODO(philkuz) we should move the query id into the api so we can track how queries propagate through the system.
	queryID := uuid.NewV4()

	flags, err := ParseQueryFlags(req.QueryStr)
	if err != nil {
		queryIDPB := utils.ProtoFromUUID(&queryID)

		return &querybrokerpb.VizierQueryResponse{
			QueryID: queryIDPB,
			Status: &statuspb.Status{
				ErrCode: statuspb.INVALID_ARGUMENT,
				Msg:     err.Error(),
			},
		}, nil
	}
	planOpts := flags.GetPlanOptions()

	var udfInfo udfspb.UDFInfo
	if err := loadUDFInfo(&udfInfo); err != nil {
		return nil, err
	}
	planner := logicalplanner.New(&udfInfo)
	defer planner.Free()
	return s.ExecuteQueryWithPlanner(ctx, req, queryID, planner, planOpts)
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

	var agentStatuses []*querybrokerpb.AgentMetadata

	for _, agentInfo := range mdsResp.Info {
		agentStatuses = append(agentStatuses, &querybrokerpb.AgentMetadata{
			Agent:  agentInfo.Agent,
			Status: agentInfo.Status,
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

// GetAvailableFlagsWithPlanner gets the flag specs for a given query from the given planner
func (s *Server) GetAvailableFlagsWithPlanner(ctx context.Context, req *plannerpb.QueryRequest, planner Planner) (*plannerpb.GetAvailableFlagsResult, error) {
	resultPB, err := planner.GetAvailableFlags(req)
	if err != nil {
		return nil, err
	}

	return resultPB, nil
}

// GetAvailableFlags gets the flags specs from a given query from the logical planner.
func (s *Server) GetAvailableFlags(ctx context.Context, req *plannerpb.QueryRequest) (*plannerpb.GetAvailableFlagsResult, error) {
	var udfInfo udfspb.UDFInfo
	if err := loadUDFInfo(&udfInfo); err != nil {
		return nil, err
	}
	planner := logicalplanner.New(&udfInfo)
	defer planner.Free()
	return s.GetAvailableFlagsWithPlanner(ctx, req, planner)
}
