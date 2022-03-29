/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

import (
	"context"
	"errors"
	"strings"
	"time"

	"golang.org/x/sync/errgroup"

	"github.com/gofrs/uuid"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/planpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

// QueryResultConsumer defines an interface to allow consumption of Query results from a QueryResultExecutor.
type QueryResultConsumer interface {
	Consume(*vizierpb.ExecuteScriptResponse) error
}

// QueryExecutor executes a query and allows the caller to consume results via a QueryResultConsumer.
type QueryExecutor interface {
	Run(context.Context, *vizierpb.ExecuteScriptRequest, QueryResultConsumer) error
	Wait() error
	QueryID() uuid.UUID
}

// DataPrivacy is an interface that manages data privacy in the query executor.
type DataPrivacy interface {
	// RedactionOptions returns the proto message containing options for redaction based on the cached data privacy level.
	RedactionOptions(ctx context.Context) (*distributedpb.RedactionOptions, error)
}

// MutationExecFactory is a function that creates a new MutationExecutorImpl.
type MutationExecFactory func(Planner,
	metadatapb.MetadataTracepointServiceClient,
	metadatapb.MetadataConfigServiceClient,
	*distributedpb.DistributedState) MutationExecutor

// QueryExecutorImpl implements the QueryExecutor interface.
type QueryExecutorImpl struct {
	resultAddress       string
	resultSSLTargetName string
	agentsTracker       AgentsTracker
	dataPrivacy         DataPrivacy
	natsConn            *nats.Conn
	mdtp                metadatapb.MetadataTracepointServiceClient
	mdconf              metadatapb.MetadataConfigServiceClient
	resultForwarder     QueryResultForwarder
	planner             Planner

	eg *errgroup.Group

	queryID           uuid.UUID
	startTime         time.Time
	compilationTimeNs int64

	mutationExecFactory MutationExecFactory
}

// NewQueryExecutorFromServer creates a new QueryExecutor using the properties of a query broker server.
func NewQueryExecutorFromServer(s *Server, mutExecFactory MutationExecFactory) QueryExecutor {
	return NewQueryExecutor(
		s.env.Address(),
		s.env.SSLTargetName(),
		s.agentsTracker,
		s.dataPrivacy,
		s.natsConn,
		s.mdtp,
		s.mdconf,
		s.resultForwarder,
		s.planner,
		mutExecFactory,
	)
}

// NewQueryExecutor creates a new QueryExecutorImpl.
func NewQueryExecutor(
	resultAddress string,
	resultSSLTargetName string,
	agentsTracker AgentsTracker,
	dataPrivacy DataPrivacy,
	natsConn *nats.Conn,
	mdtp metadatapb.MetadataTracepointServiceClient,
	mdconf metadatapb.MetadataConfigServiceClient,
	resultForwarder QueryResultForwarder,
	planner Planner,
	mutExecFactory MutationExecFactory,
) QueryExecutor {
	return &QueryExecutorImpl{
		resultAddress:       resultAddress,
		resultSSLTargetName: resultSSLTargetName,
		agentsTracker:       agentsTracker,
		dataPrivacy:         dataPrivacy,
		natsConn:            natsConn,
		mdtp:                mdtp,
		mdconf:              mdconf,
		resultForwarder:     resultForwarder,
		planner:             planner,
		mutationExecFactory: mutExecFactory,
	}
}

// Run launches a query with the given QueryResultConsumer consuming results, and does not wait for the query to error or finish.
func (q *QueryExecutorImpl) Run(ctx context.Context, req *vizierpb.ExecuteScriptRequest, consumer QueryResultConsumer) error {
	q.eg, ctx = errgroup.WithContext(ctx)

	if req.QueryID != "" {
		queryID, err := uuid.FromString(req.QueryID)
		if err != nil {
			return err
		}
		q.queryID = queryID
	} else {
		queryID, err := uuid.NewV4()
		if err != nil {
			return err
		}
		q.queryID = queryID
	}

	resultCh := make(chan *vizierpb.ExecuteScriptResponse)

	q.eg.Go(func() error { return q.runConsumer(ctx, resultCh, consumer) })
	q.eg.Go(func() error {
		return q.runScript(ctx, resultCh, req)
	})

	return nil
}

// Wait waits for the query to finish or error.
func (q *QueryExecutorImpl) Wait() error {
	err := q.eg.Wait()
	if err == nil {
		return nil
	}
	// There are a few common failure cases that may occur naturally during query execution. For example, ctxDeadlineExceeded,
	// and invalid arguments. In this case, we do not want to unnecessarily log our error state.
	if strings.Contains(err.Error(), "Distributed state does not have a Carnot instance") {
		return err
	}
	if strings.Contains(err.Error(), "InvalidArgument") {
		return err
	}
	if strings.Contains(err.Error(), "failed to initialize all result tables") {
		return err
	}
	if errors.Is(err, nats.ErrConnectionClosed) {
		return err
	}
	if errors.Is(err, context.DeadlineExceeded) {
		return err
	}
	if errors.Is(err, context.Canceled) {
		log.WithField("query_id", q.queryID).
			Info("Query cancelled")
		return err
	}
	log.WithField("query_id", q.queryID).
		WithField("duration", time.Since(q.startTime)).
		WithError(err).
		Error("failed to execute query")
	return err
}

// QueryID returns the uuid of the executing query.
func (q *QueryExecutorImpl) QueryID() uuid.UUID {
	return q.queryID
}

func (q *QueryExecutorImpl) runConsumer(ctx context.Context, resultCh <-chan *vizierpb.ExecuteScriptResponse, consumer QueryResultConsumer) error {
	for {
		select {
		case <-ctx.Done():
			return ctx.Err()
		case result, ok := <-resultCh:
			if !ok {
				return nil
			}
			if err := consumer.Consume(result); err != nil {
				return err
			}
		}
	}
}

func (q *QueryExecutorImpl) sendResponse(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, resp *vizierpb.ExecuteScriptResponse) error {
	select {
	case <-ctx.Done():
		return ctx.Err()
	case resultCh <- resp:
		return nil
	}
}

func (q *QueryExecutorImpl) getPlanOpts(queryStr string) (*planpb.PlanOptions, error) {
	flags, err := ParseQueryFlags(queryStr)
	if err != nil {
		return nil, err
	}

	planOpts := flags.GetPlanOptions()
	return planOpts, nil
}

func (q *QueryExecutorImpl) runMutation(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, req *vizierpb.ExecuteScriptRequest, planOpts *planpb.PlanOptions, distributedState *distributedpb.DistributedState) error {
	mutationExec := q.mutationExecFactory(q.planner, q.mdtp, q.mdconf, distributedState)

	s, err := mutationExec.Execute(ctx, req, planOpts)
	if err != nil {
		return err
	}
	if s != nil {
		if err := q.sendResponse(ctx, resultCh, StatusToVizierResponse(q.queryID, s)); err != nil {
			return err
		}
		return StatusToError(s)
	}
	mutationInfo, err := mutationExec.MutationInfo(ctx)
	if err != nil {
		return err
	}

	resp := &vizierpb.ExecuteScriptResponse{
		QueryID:      q.queryID.String(),
		MutationInfo: mutationInfo,
	}
	if err := q.sendResponse(ctx, resultCh, resp); err != nil {
		return err
	}

	if mutationInfo.Status.Code != int32(codes.OK) {
		return VizierStatusToError(mutationInfo.Status)
	}
	return nil
}

func (q *QueryExecutorImpl) compilePlan(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, req *plannerpb.QueryRequest, planOpts *planpb.PlanOptions, distributedState *distributedpb.DistributedState) (*distributedpb.DistributedPlan, error) {
	info := q.agentsTracker.GetAgentInfo()
	if info == nil {
		return nil, status.Error(codes.Unavailable, "not ready yet")
	}

	redactOptions, err := q.dataPrivacy.RedactionOptions(ctx)
	if err != nil {
		log.WithError(err).Errorf("Failed to get the redaction options")
		return nil, status.Errorf(codes.Internal, "error setting up the compiler")
	}

	var otelConfig *distributedpb.OTelEndpointConfig
	if req.Configs != nil && req.Configs.OTelEndpointConfig != nil {
		otelConfig = &distributedpb.OTelEndpointConfig{
			URL:     req.Configs.OTelEndpointConfig.URL,
			Headers: req.Configs.OTelEndpointConfig.Headers,
		}
	}

	plannerState := &distributedpb.LogicalPlannerState{
		DistributedState:    distributedState,
		PlanOptions:         planOpts,
		ResultAddress:       q.resultAddress,
		ResultSSLTargetName: q.resultSSLTargetName,
		RedactionOptions:    redactOptions,
		OTelEndpointConfig:  otelConfig,
	}

	// Compile the query plan.
	start := time.Now()
	plannerResultPB, err := q.planner.Plan(plannerState, req)
	if err != nil {
		// send the compilation error and return nil.
		return nil, err
	}
	q.compilationTimeNs = time.Since(start).Nanoseconds()

	// When the status is not OK, this means it's a compilation error on the query passed in.
	if plannerResultPB.Status.ErrCode != statuspb.OK {
		if err := q.sendResponse(ctx, resultCh, StatusToVizierResponse(q.queryID, plannerResultPB.Status)); err != nil {
			return nil, err
		}
		return nil, StatusToError(plannerResultPB.Status)
	}
	return plannerResultPB.Plan, nil
}

func (q *QueryExecutorImpl) buildAgentPlanMap(plan *distributedpb.DistributedPlan) (map[uuid.UUID]*planpb.Plan, error) {
	planMap := make(map[uuid.UUID]*planpb.Plan)

	for carnotID, agentPlan := range plan.QbAddressToPlan {
		u, err := uuid.FromString(carnotID)
		if err != nil {
			log.WithError(err).Errorf("Couldn't parse uuid from agent id \"%s\"", carnotID)
			return planMap, err
		}
		planMap[u] = agentPlan
	}
	return planMap, nil
}

func (q *QueryExecutorImpl) buildTableMap(planMap map[uuid.UUID]*planpb.Plan) (map[string]string, error) {
	tableNameToIDMap := make(map[string]string)

	for _, plan := range planMap {
		for _, fragment := range plan.Nodes {
			for _, node := range fragment.Nodes {
				if node.Op.OpType == planpb.GRPC_SINK_OPERATOR {
					if output := node.Op.GetGRPCSinkOp().GetOutputTable(); output != nil {
						id, err := uuid.NewV4()
						if err != nil {
							return tableNameToIDMap, err
						}
						tableNameToIDMap[output.TableName] = id.String()
					}
				}
			}
		}
	}

	return tableNameToIDMap, nil
}

func (q *QueryExecutorImpl) sendTableRelationResponses(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, tableNameToIDMap map[string]string, planMap map[uuid.UUID]*planpb.Plan) error {
	tableRelationResponses, err := TableRelationResponses(q.queryID, tableNameToIDMap, planMap)
	if err != nil {
		return err
	}
	for _, resp := range tableRelationResponses {
		if err := q.sendResponse(ctx, resultCh, resp); err != nil {
			return err
		}
	}
	return nil
}

func (q *QueryExecutorImpl) buildQueryPlanOpts(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, plan *distributedpb.DistributedPlan, planMap map[uuid.UUID]*planpb.Plan, planOpts *planpb.PlanOptions) (*QueryPlanOpts, error) {
	if !planOpts.Explain {
		return nil, nil
	}
	queryPlanTableID, err := uuid.NewV4()
	if err != nil {
		return nil, err
	}

	if err := q.sendResponse(ctx, resultCh, QueryPlanRelationResponse(q.queryID, queryPlanTableID.String())); err != nil {
		return nil, err
	}
	queryPlanOpts := &QueryPlanOpts{
		TableID: queryPlanTableID.String(),
		Plan:    plan,
		PlanMap: planMap,
	}
	return queryPlanOpts, nil
}

func (q *QueryExecutorImpl) prepareScript(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, req *vizierpb.ExecuteScriptRequest) error {
	planOpts, err := q.getPlanOpts(req.QueryStr)
	if err != nil {
		return err
	}

	distributedState := q.agentsTracker.GetAgentInfo().DistributedState()

	if req.Mutation {
		if err := q.runMutation(ctx, resultCh, req, planOpts, &distributedState); err != nil {
			return err
		}
	}

	// Convert request to a format expected by the planner.
	convertedReq, err := VizierQueryRequestToPlannerQueryRequest(req)
	if err != nil {
		return err
	}

	plan, err := q.compilePlan(ctx, resultCh, convertedReq, planOpts, &distributedState)
	if err != nil {
		return err
	}

	planMap, err := q.buildAgentPlanMap(plan)
	if err != nil {
		return err
	}
	tableNameToIDMap, err := q.buildTableMap(planMap)
	if err != nil {
		return err
	}

	if err := q.sendTableRelationResponses(ctx, resultCh, tableNameToIDMap, planMap); err != nil {
		return err
	}

	queryPlanOpts, err := q.buildQueryPlanOpts(ctx, resultCh, plan, planMap, planOpts)
	if err != nil {
		return err
	}

	err = q.resultForwarder.RegisterQuery(q.queryID, tableNameToIDMap, q.compilationTimeNs, queryPlanOpts)
	if err != nil {
		return err
	}
	err = LaunchQuery(q.queryID, q.natsConn, planMap, planOpts.Analyze)
	if err != nil {
		return err
	}
	return nil
}

func (q *QueryExecutorImpl) runScript(ctx context.Context, resultCh chan<- *vizierpb.ExecuteScriptResponse, req *vizierpb.ExecuteScriptRequest) error {
	defer close(resultCh)
	q.startTime = time.Now()
	log.WithField("query_id", q.queryID).Infof("Running script")

	if req.QueryID == "" {
		if err := q.prepareScript(ctx, resultCh, req); err != nil {
			return err
		}
	}

	return q.resultForwarder.StreamResults(ctx, q.queryID, resultCh)
}
