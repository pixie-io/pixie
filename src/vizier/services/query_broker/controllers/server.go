package controllers

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/carnot/planner/compilerpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"

	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
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
func (s *Server) ExecuteQueryWithPlanner(ctx context.Context, req *plannerpb.QueryRequest, queryID uuid.UUID, planner Planner, planOpts *planpb.PlanOptions) (*queryresultspb.QueryResult, *statuspb.Status, error) {
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", aCtx.AuthToken))

	// Get the table schema that is presumably shared across agents.
	mdsSchemaReq := &metadatapb.SchemaRequest{}
	mdsSchemaResp, err := s.mdsClient.GetSchemas(ctx, mdsSchemaReq)
	if err != nil {
		log.WithError(err).Error("Failed to get schemas.")
		return nil, nil, err
	}
	schema := mdsSchemaResp.Schema
	// Get all available agents for now.
	mdsReq := &metadatapb.AgentInfoRequest{}
	mdsResp, err := s.mdsClient.GetAgentInfo(ctx, mdsReq)
	if err != nil {
		return nil, nil, err
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
		return nil, nil, err
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
		return nil, nil, err
	}

	// When the status is not OK, this means it's a compilation error on the query passed in.
	if plannerResultPB.Status.ErrCode != statuspb.OK {
		return nil, plannerResultPB.Status, nil
	}

	// Plan describes the mapping of agents to the plan that should execute on them.
	plan := plannerResultPB.Plan
	planMap := make(map[uuid.UUID]*planpb.Plan)

	for carnotID, agentPlan := range plan.QbAddressToPlan {
		u, err := uuid.FromString(carnotID)
		if err != nil {
			log.WithError(err).Fatalf("Couldn't parse uuid from agent id \"%s\"", carnotID)
			return nil, nil, err
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
		return nil, nil, err
	}

	queryResult, err := queryExecutor.WaitForCompletion()
	if err != nil {
		return nil, nil, err
	}

	s.deleteExecutorForQuery(queryID)
	return queryResult, nil, nil
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
	qr, status, err := s.ExecuteQueryWithPlanner(ctx, req, queryID, planner, planOpts)
	if err != nil {
		return nil, err
	}
	if status != nil {
		return failedStatusQueryResponse(queryID, status), nil
	}

	return &querybrokerpb.VizierQueryResponse{
		QueryID:     utils.ProtoFromUUID(&queryID),
		QueryResult: qr,
	}, nil
}

// GetSchemas returns the schemas in the system.
func (s *Server) GetSchemas(ctx context.Context, req *querybrokerpb.SchemaRequest) (*querybrokerpb.SchemaResponse, error) {
	return nil, status.Error(codes.Unimplemented, "Not implemented yet")
}

// GetAgentInfo returns information about registered agents.
func (s *Server) GetAgentInfo(ctx context.Context, req *querybrokerpb.AgentInfoRequest) (*querybrokerpb.AgentInfoResponse, error) {
	mdsReq := &metadatapb.AgentInfoRequest{}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", aCtx.AuthToken))

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

// HealthCheck continually responds with the current health of Vizier.
func (s *Server) HealthCheck(req *vizierpb.HealthCheckRequest, srv vizierpb.VizierService_HealthCheckServer) error {
	// For now, just report itself as healthy.
	for c := time.Tick(5 * time.Second); ; {
		err := srv.Send(&vizierpb.HealthCheckResponse{
			Status: &vizierpb.Status{
				Code: int32(codes.OK),
			},
		})
		if err != nil {
			log.WithError(err).Error("Error sending on stream, ending health check")
			return err
		}
		select {
		case <-srv.Context().Done():
			return nil
		case <-c:
			continue
		}
	}
	return nil
}

// ExecuteScript executes the script and sends results through the gRPC stream.
func (s *Server) ExecuteScript(req *vizierpb.ExecuteScriptRequest, srv vizierpb.VizierService_ExecuteScriptServer) error {
	// TODO(philkuz) we should move the query id into the api so we can track how queries propagate through the system.
	queryID := uuid.NewV4()

	// Convert request to a format expected by the planner.
	convertedReq, err := VizierQueryRequestToPlannerQueryRequest(req)
	if err != nil {
		return err
	}

	flags, err := ParseQueryFlags(convertedReq.QueryStr)
	if err != nil {
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Status: &vizierpb.Status{
				Message: err.Error(),
			},
		}

		return srv.Send(resp)
	}
	planOpts := flags.GetPlanOptions()

	var udfInfo udfspb.UDFInfo
	if err := loadUDFInfo(&udfInfo); err != nil {
		return err
	}
	planner := logicalplanner.New(&udfInfo)
	defer planner.Free()

	ctx := srv.Context()
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return err
	}

	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", aCtx.AuthToken))

	qr, status, err := s.ExecuteQueryWithPlanner(ctx, convertedReq, queryID, planner, planOpts)
	if err != nil {
		return err
	}
	if status != nil {
		convStatus, err := StatusToVizierStatus(status)
		if err != nil {
			return err
		}
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Status:  convStatus,
		}
		return srv.Send(resp)
	}
	if qr == nil {
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Status: &vizierpb.Status{
				Message: "Query timed out with no results",
			},
		}

		return srv.Send(resp)
	}

	// Convert query result into the externally-facing format.
	for _, table := range qr.Tables {
		tableID := uuid.NewV4().String()

		// Send schema first.
		md, err := RelationFromTable(table)
		if err != nil {
			return err
		}
		md.ID = tableID
		resp := &vizierpb.ExecuteScriptResponse{
			QueryID: queryID.String(),
			Result: &vizierpb.ExecuteScriptResponse_MetaData{
				MetaData: md,
			},
		}
		err = srv.Send(resp)
		if err != nil {
			return err
		}

		// Send row batches.
		for i, rb := range table.RowBatches {
			newRb, err := RowBatchToVizierRowBatch(rb)
			if err != nil {
				return err
			}
			newRb.TableID = tableID
			if i == len(table.RowBatches)-1 {
				newRb.Eos = true
			}

			resp := &vizierpb.ExecuteScriptResponse{
				QueryID: queryID.String(),
				Result: &vizierpb.ExecuteScriptResponse_Data{
					Data: &vizierpb.QueryData{
						Batch: newRb,
					},
				},
			}
			err = srv.Send(resp)
			if err != nil {
				return err
			}
		}
	}

	// Send execution stats.
	stats, err := QueryResultStatsToVizierStats(qr)
	if err != nil {
		return err
	}
	resp := &vizierpb.ExecuteScriptResponse{
		QueryID: queryID.String(),
		Result: &vizierpb.ExecuteScriptResponse_Data{
			Data: stats,
		},
	}
	return srv.Send(resp)
}
