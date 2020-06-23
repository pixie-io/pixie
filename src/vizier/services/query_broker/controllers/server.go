package controllers

import (
	"context"
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
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
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

const healthCheckInterval = 5 * time.Second

type contextKey string

const (
	execStartKey       = contextKey("execStart")
	compileCompleteKey = contextKey("compileDone")
)

// Planner describes the interface for any planner.
type Planner interface {
	Plan(planState *distributedpb.LogicalPlannerState, req *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error)
	Free()
}

// Executor is the interface for a query executor.
type Executor interface {
	ExecuteQuery(planMap map[uuid.UUID]*planpb.Plan, analyze bool) error
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

	hcMux    sync.Mutex
	hcStatus error
	hcTime   time.Time
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

func makeAgentCarnotInfo(agentID uuid.UUID, asid uint32, agentMetadata *distributedpb.MetadataInfo) *distributedpb.CarnotInfo {
	// TODO(philkuz) (PL-910) need to update this to also contain table info.
	return &distributedpb.CarnotInfo{
		QueryBrokerAddress:   agentID.String(),
		ASID:                 asid,
		HasGRPCServer:        false,
		HasDataStore:         true,
		ProcessesData:        true,
		AcceptsRemoteSources: false,
		MetadataInfo:         agentMetadata,
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
		// When we support persistent backup, Kelvins will also have MetadataInfo.
		MetadataInfo: nil,
	}
}

func makePlannerState(pemInfo []*agentpb.Agent, kelvinList []*agentpb.Agent, agentMetadataMap map[uuid.UUID]*distributedpb.MetadataInfo,
	schema *schemapb.Schema, planOpts *planpb.PlanOptions) (*distributedpb.LogicalPlannerState, error) {
	// TODO(philkuz) (PL-910) need to update this to pass table info.
	carnotInfoList := make([]*distributedpb.CarnotInfo, 0)
	for _, pem := range pemInfo {
		pemID := utils.UUIDFromProtoOrNil(pem.Info.AgentID)
		var agentMetadata *distributedpb.MetadataInfo
		if md, found := agentMetadataMap[pemID]; found {
			agentMetadata = md
		}
		carnotInfoList = append(carnotInfoList, makeAgentCarnotInfo(pemID, pem.ASID, agentMetadata))
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
	ctx = context.WithValue(ctx, execStartKey, time.Now())
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", aCtx.AuthToken))

	// Get all available agents for now.
	agentInfosReq := &metadatapb.AgentInfoRequest{}
	agentInfosResp, err := s.mdsClient.GetAgentInfo(ctx, agentInfosReq)
	if err != nil {
		return nil, nil, err
	}

	var kelvinList []*agentpb.Agent
	var pemList []*agentpb.Agent

	for _, m := range agentInfosResp.Info {
		if m.Agent.Info.Capabilities == nil || m.Agent.Info.Capabilities.CollectsData {
			pemList = append(pemList, m.Agent)
		} else {
			kelvinList = append(kelvinList, m.Agent)
		}
	}

	mdsAgentMetadataResp, err := s.mdsClient.GetAgentTableMetadata(ctx, &metadatapb.AgentTableMetadataRequest{})
	if err != nil {
		log.WithError(err).Error("Failed to get agent metadata.")
		return nil, nil, err
	}

	var schema *schemapb.Schema
	// We currently assume the schema is shared across all agents.
	// In the unusual case where there are no agents, we can still run Kelvin-only
	// queries, so in that case we call out for the schemas directly.
	if len(mdsAgentMetadataResp.MetadataByAgent) > 0 {
		schema = mdsAgentMetadataResp.MetadataByAgent[0].Schema
	} else {
		// Get the table schema that is presumably shared across agents.
		mdsSchemaReq := &metadatapb.SchemaRequest{}
		mdsSchemaResp, err := s.mdsClient.GetSchemas(ctx, mdsSchemaReq)
		if err != nil {
			log.WithError(err).Error("Failed to get schemas.")
		}
		schema = mdsSchemaResp.Schema
	}

	var agentTableMetadata = make(map[uuid.UUID]*distributedpb.MetadataInfo)
	for _, md := range mdsAgentMetadataResp.MetadataByAgent {
		if md.DataInfo != nil && md.DataInfo.MetadataInfo != nil {
			agentUUID, err := utils.UUIDFromProto(md.AgentID)
			if err != nil {
				log.WithError(err).Error("Failed to parse agent UUID: %+v", md.AgentID)
				return nil, nil, err
			}
			agentTableMetadata[agentUUID] = md.DataInfo.MetadataInfo
		}
	}

	plannerState, err := makePlannerState(pemList, kelvinList, agentTableMetadata, schema, planOpts)
	if err != nil {
		return nil, nil, err
	}

	log.WithField("query_id", queryID).
		Infof("Running script")
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

	ctx = context.WithValue(ctx, compileCompleteKey, time.Now())
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
	defer s.deleteExecutorForQuery(queryID)

	if err := queryExecutor.ExecuteQuery(planMap, planOpts.Analyze); err != nil {
		return nil, nil, err
	}

	queryResult, err := queryExecutor.WaitForCompletion()
	if err != nil {
		return nil, nil, err
	}

	// TODO(zasgar): Cleanup this code to push the distrbuted plan into
	// ExecuteQuery directly and do the mapping in there.
	if plannerState.PlanOptions.Explain {
		if err := AddQueryPlanToResult(queryResult, plan, planMap, &queryResult.AgentExecutionStats); err != nil {
			log.WithError(err).Error("Failed to add query plan to result")
		}
	}

	execStartTime := ctx.Value(execStartKey).(time.Time)
	compilationCompleteTime := ctx.Value(compileCompleteKey).(time.Time)
	if queryResult != nil {
		queryResult.TimingInfo.CompilationTimeNs = compilationCompleteTime.Sub(execStartTime).Nanoseconds()
	}

	if queryResult != nil {
		if err := annotateResultWithSemanticTypes(queryResult, planMap); err != nil {
			return nil, nil, err
		}
	}

	return queryResult, nil, nil
}

func annotateResultWithSemanticTypes(result *queryresultspb.QueryResult, planMap map[uuid.UUID]*planpb.Plan) error {
	memSinks := make(map[string]*planpb.MemorySinkOperator)

	for _, plan := range planMap {
		for _, fragment := range plan.Nodes {
			for _, node := range fragment.Nodes {
				if node.Op.OpType == planpb.MEMORY_SINK_OPERATOR {
					memSinks[node.Op.GetMemSinkOp().Name] = node.Op.GetMemSinkOp()
				}
			}
		}
	}

	for _, table := range result.Tables {
		memSinkOp, ok := memSinks[table.Name]
		if !ok {
			for _, col := range table.Relation.Columns {
				col.ColumnSemanticType = typespb.ST_NONE
			}
			log.Infof("Table '%s' has no corresponding MemSinkOp", table.Name)
			return nil
		}
		for i, col := range table.Relation.Columns {
			col.ColumnSemanticType = memSinkOp.ColumnSemanticTypes[i]
		}
	}
	return nil
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
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	log.WithField("queryID", queryID.String()).Info("Got Kelvin Results")
	setExecutor := func(queryID uuid.UUID) (Executor, bool) {
		s.mux.Lock()
		exec, ok := s.executors[queryID]
		defer s.mux.Unlock()
		return exec, ok
	}

	exec, ok := setExecutor(queryID)
	if !ok {
		return nil, status.Error(codes.InvalidArgument, "Unexpected/Incorrect queryID")
	}

	exec.AddResult(req)
	queryResponse := &querybrokerpb.AgentQueryResultResponse{}
	return queryResponse, nil
}

// checkHealth runs the health check and returns (request passed, health check error/nil).
func (s *Server) checkHealth(ctx context.Context) (bool, error) {
	checkVersionScript := `import px; px.display(px.Version())`
	req := plannerpb.QueryRequest{
		QueryStr: checkVersionScript,
	}
	resp, err := s.ExecuteQuery(ctx, &req)
	if err != nil {
		return true, err
	}
	if resp.Status != nil && resp.Status.ErrCode != statuspb.OK {
		return false, status.Error(codes.Code(resp.Status.ErrCode), resp.Status.Msg)
	}
	if resp.QueryResult == nil || len(resp.QueryResult.Tables) != 1 || len(resp.QueryResult.Tables[0].RowBatches) != 1 {
		return false, status.Error(codes.Unavailable, "results not returned on health check")
	}
	if resp.QueryResult.Tables[0].RowBatches[0].NumRows != 1 {
		return false, status.Error(codes.Unavailable, "bad results on healthcheck")
	}
	// HealthCheck OK.
	return false, nil
}

func (s *Server) checkHealthCached(ctx context.Context) error {
	currentTime := time.Now()
	s.hcMux.Lock()
	defer s.hcMux.Unlock()
	if currentTime.Sub(s.hcTime) < healthCheckInterval {
		return s.hcStatus
	}
	reqOk, status := s.checkHealth(ctx)
	if !reqOk || status != nil {
		// If the request failed don't cache the results.
		return status
	}
	s.hcTime = currentTime
	s.hcStatus = status
	return s.hcStatus
}

// HealthCheck continually responds with the current health of Vizier.
func (s *Server) HealthCheck(req *vizierpb.HealthCheckRequest, srv vizierpb.VizierService_HealthCheckServer) error {
	// For now, just report itself as healthy.
	for c := time.Tick(healthCheckInterval); ; {
		hcStatus := s.checkHealthCached(srv.Context())
		// Pass.
		code := int32(codes.OK)
		if hcStatus != nil {
			s, ok := status.FromError(hcStatus)
			if !ok {
				code = int32(codes.Unavailable)
			} else {
				code = int32(s.Code())
			}

			if code == int32(codes.Unauthenticated) {
				// Request failed probably because of token expiration. Abort the request since this is not recoverable.
				return status.Errorf(codes.Unauthenticated, "authentication error")
			}
		}
		err := srv.Send(&vizierpb.HealthCheckResponse{
			Status: &vizierpb.Status{
				Code: code,
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
}

// ExecuteScript executes the script and sends results through the gRPC stream.
func (s *Server) ExecuteScript(req *vizierpb.ExecuteScriptRequest, srv vizierpb.VizierService_ExecuteScriptServer) error {
	ctx := context.WithValue(srv.Context(), execStartKey, time.Now())
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
			Status:  ErrToVizierStatus(err),
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
				Code:    int32(codes.DeadlineExceeded),
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
