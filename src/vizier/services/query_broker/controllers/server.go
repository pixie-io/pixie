package controllers

import (
	"context"
	"io"
	"sync"
	"time"

	"github.com/gogo/protobuf/proto"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/tracker"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/queryresultspb"
	"pixielabs.ai/pixielabs/src/carnotpb"
	typespb "pixielabs.ai/pixielabs/src/shared/types/proto"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"

	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	"pixielabs.ai/pixielabs/src/carnot/udfspb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/utils"
	funcs "pixielabs.ai/pixielabs/src/vizier/funcs/export"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerenv"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/querybrokerpb"
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
	AddStreamedResult(res *carnotpb.TransferResultChunkRequest) error
}

// AgentsTracker is the interface for the background agent information tracker.
type AgentsTracker interface {
	GetAgentInfo() tracker.AgentsInfo
}

// Server defines an gRPC server type.
type Server struct {
	env           querybrokerenv.QueryBrokerEnv
	agentsTracker AgentsTracker
	natsConn      *nats.Conn
	newExecutor   func(*nats.Conn, uuid.UUID) Executor
	executors     map[uuid.UUID]Executor
	// Mutex is used for managing query executor instances.
	mux sync.Mutex

	hcMux    sync.Mutex
	hcStatus error
	hcTime   time.Time

	mdtp            metadatapb.MetadataTracepointServiceClient
	udfInfo         udfspb.UDFInfo
	resultForwarder QueryResultForwarder
}

// NewServer creates GRPC handlers.
func NewServer(env querybrokerenv.QueryBrokerEnv, agentsTracker AgentsTracker, mds metadatapb.MetadataTracepointServiceClient, natsConn *nats.Conn) (*Server, error) {
	return NewServerWithExecutor(env, agentsTracker /* queryResultForwarder*/, nil, mds, natsConn, NewQueryExecutor)
}

// NewServerWithExecutor is NewServer with an functor for executor.
func NewServerWithExecutor(env querybrokerenv.QueryBrokerEnv,
	agentsTracker AgentsTracker,
	resultForwarder QueryResultForwarder,
	mds metadatapb.MetadataTracepointServiceClient,
	natsConn *nats.Conn,
	newExecutor func(*nats.Conn, uuid.UUID) Executor) (*Server, error) {

	var udfInfo udfspb.UDFInfo
	if err := loadUDFInfo(&udfInfo); err != nil {
		return nil, err
	}

	s := &Server{
		env:             env,
		agentsTracker:   agentsTracker,
		resultForwarder: resultForwarder,

		natsConn:    natsConn,
		newExecutor: newExecutor,
		executors:   make(map[uuid.UUID]Executor),

		mdtp:    mds,
		udfInfo: udfInfo,
	}
	// TODO(nserrino): update this logic when batch Kelvin API is deprecated and s.executors
	// gets subsumed by s.resultForwarder.
	if s.resultForwarder == nil {
		s.resultForwarder = NewQueryResultForwarder(s.executors)
	}
	return s, nil
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

// ExecuteQueryWithPlanner executes a query with the provided planner.
func (s *Server) ExecuteQueryWithPlanner(ctx context.Context, req *plannerpb.QueryRequest, queryID uuid.UUID, planner Planner, planOpts *planpb.PlanOptions) (*queryresultspb.QueryResult, *statuspb.Status, error) {
	ctx = context.WithValue(ctx, execStartKey, time.Now())

	info := s.agentsTracker.GetAgentInfo()
	if info == nil {
		return nil, nil, status.Error(codes.Unavailable, "not ready yet")
	}
	plannerState := &distributedpb.LogicalPlannerState{
		DistributedState:    info.DistributedState(),
		PlanOptions:         planOpts,
		ResultAddress:       s.env.Address(),
		ResultSSLTargetName: s.env.SSLTargetName(),
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

	queryExecutor := s.newExecutor(s.natsConn, queryID)

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
	resultGRPCTables := make(map[string]*planpb.GRPCSinkOperator_ResultTable)

	for _, plan := range planMap {
		for _, fragment := range plan.Nodes {
			for _, node := range fragment.Nodes {
				if node.Op.OpType == planpb.GRPC_SINK_OPERATOR {
					if output := node.Op.GetGRPCSinkOp().GetOutputTable(); output != nil {
						resultGRPCTables[output.TableName] = output
					}
				}
			}
		}
	}

	for _, table := range result.Tables {
		outputTable, ok := resultGRPCTables[table.Name]
		if !ok {
			for _, col := range table.Relation.Columns {
				col.ColumnSemanticType = typespb.ST_NONE
			}
			log.Infof("Table '%s' has no corresponding GRPCSinkOperator", table.Name)
			return nil
		}
		for i, col := range table.Relation.Columns {
			col.ColumnSemanticType = outputTable.ColumnSemanticTypes[i]
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

// executeQuery executes query. This is an internal function to be deprecated soon.
func (s *Server) executeScript(ctx context.Context, req *plannerpb.QueryRequest) (*querybrokerpb.VizierQueryResponse, error) {
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
	planner := logicalplanner.New(&s.udfInfo)
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

// ReceiveAgentQueryResult gets the query result from an agent and stores the results until all
// relevant agents have responded.
func (s *Server) ReceiveAgentQueryResult(ctx context.Context, req *querybrokerpb.AgentQueryResultRequest) (*querybrokerpb.AgentQueryResultResponse, error) {
	queryIDPB := req.Result.QueryID
	queryID, err := utils.UUIDFromProto(queryIDPB)
	if err != nil {
		return nil, status.Error(codes.InvalidArgument, err.Error())
	}

	log.WithField("queryID", queryID.String()).Trace("Got Kelvin Results")
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
	resp, err := s.executeScript(ctx, &req)
	if err != nil {
		return false, err
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

	flags, err := ParseQueryFlags(req.QueryStr)
	if err != nil {
		return srv.Send(ErrToVizierResponse(queryID, err))
	}

	planOpts := flags.GetPlanOptions()
	planner := logicalplanner.New(&s.udfInfo)
	defer planner.Free()

	if req.Mutation {
		mutationExec := NewMutationExecutor(planner, s.mdtp, s.agentsTracker)

		status, err := mutationExec.Execute(ctx, req, planOpts)
		if err != nil {
			return srv.Send(ErrToVizierResponse(queryID, err))
		}
		if status != nil {
			return srv.Send(StatusToVizierResponse(queryID, status))
		}
		mutationInfo, err := mutationExec.MutationInfo(ctx)
		if err != nil {
			return srv.Send(ErrToVizierResponse(queryID, err))
		}
		err = srv.Send(&vizierpb.ExecuteScriptResponse{
			QueryID:      queryID.String(),
			MutationInfo: mutationInfo,
		})

		if mutationInfo.Status.Code != int32(codes.OK) || err != nil {
			return srv.Send(ErrToVizierResponse(queryID, err))
		}
	}

	// Convert request to a format expected by the planner.
	convertedReq, err := VizierQueryRequestToPlannerQueryRequest(req)
	if err != nil {
		return err
	}

	qr, status, err := s.ExecuteQueryWithPlanner(ctx, convertedReq, queryID, planner, planOpts)
	if err != nil {
		return srv.Send(ErrToVizierResponse(queryID, err))
	}
	if status != nil {
		return srv.Send(StatusToVizierResponse(queryID, status))
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

// TransferResultChunk implements the API that allows the query broker receive streamed results
// from Carnot instances.
func (s *Server) TransferResultChunk(srv carnotpb.ResultSinkService_TransferResultChunkServer) error {
	var queryID uuid.UUID

	defer func() {
		if queryID != uuid.Nil {
			// Stop the client stream, if it still exists in the result forwarder.
			// It may have already been cancelled before this point.
			s.resultForwarder.OptionallyCancelClientStream(queryID)
		}
	}()

	sendAndClose := func(success bool, message string) error {
		err := srv.SendAndClose(&carnotpb.TransferResultChunkResponse{
			Success: success,
			Message: message,
		})
		if err != nil {
			log.WithError(err).Errorf("TransferResultChunk SendAndClose received error", err)
		}
		return err
	}

	for {
		select {
		case <-srv.Context().Done():
			return sendAndClose(true, "")
		default:
			msg, err := srv.Recv()
			// Stream closed from client side.
			if err != nil && err == io.EOF {
				return sendAndClose(true, "")
			}
			if err != nil {
				return status.Errorf(codes.Internal, "Error reading TransferResultChunk stream: %+v", err)
			}

			qid, err := utils.UUIDFromProto(msg.QueryID)
			if err != nil {
				return status.Error(codes.InvalidArgument, err.Error())
			}

			if queryID == uuid.Nil {
				queryID = qid
			}
			if queryID != qid {
				return status.Errorf(codes.Internal,
					"Received results from multiple queries in the same TransferResultChunk stream: %s and %s",
					queryID, qid)
			}

			err = s.resultForwarder.ForwardQueryResult(msg)
			// If the result wasn't forwarded, the client stream may have be cancelled.
			// This should not cause TransferResultChunk to return an error, we just include in the response
			// that the latest result chunk was not forwarded.
			if err != nil {
				log.WithError(err).Infof("Could not forward result message for query %s", queryID)
				return sendAndClose(false, err.Error())
			}
		}
	}
}

// Done is a legacy API on ResultSinkService that will be deprecated.
func (s *Server) Done(ctx context.Context, req *carnotpb.DoneRequest) (*carnotpb.DoneResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "method Done not implemented")
}
