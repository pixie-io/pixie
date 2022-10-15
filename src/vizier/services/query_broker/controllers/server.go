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
	"fmt"
	"io"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/lestrrat-go/jwx/jwa"
	"github.com/lestrrat-go/jwx/jwe"
	"github.com/lestrrat-go/jwx/jwk"
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/carnot/carnotpb"
	"px.dev/pixie/src/carnot/goplanner"
	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/plannerpb"
	"px.dev/pixie/src/carnot/udfspb"
	serviceUtils "px.dev/pixie/src/shared/services/utils"
	"px.dev/pixie/src/utils"
	funcs "px.dev/pixie/src/vizier/funcs/go"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/services/query_broker/querybrokerenv"
	"px.dev/pixie/src/vizier/services/query_broker/tracker"
)

const healthCheckInterval = 5 * time.Second

type contextKey string

const (
	execStartKey = contextKey("execStart")
)

// Planner describes the interface for any planner.
type Planner interface {
	Plan(req *plannerpb.QueryRequest) (*distributedpb.LogicalPlannerResult, error)
	CompileMutations(request *plannerpb.CompileMutationsRequest) (*plannerpb.CompileMutationsResponse, error)
	GenerateOTelScript(request *plannerpb.GenerateOTelScriptRequest) (*plannerpb.GenerateOTelScriptResponse, error)
	Free()
}

// AgentsTracker is the interface for the background agent information tracker.
type AgentsTracker interface {
	GetAgentInfo() tracker.AgentsInfo
}

// Server defines an gRPC server type.
type Server struct {
	env           querybrokerenv.QueryBrokerEnv
	agentsTracker AgentsTracker
	dataPrivacy   DataPrivacy
	natsConn      *nats.Conn

	hcStatus            serviceUtils.AtomicError
	healthcheckQuitCh   chan struct{}
	healthcheckQuitOnce sync.Once

	mdtp            metadatapb.MetadataTracepointServiceClient
	mdconf          metadatapb.MetadataConfigServiceClient
	resultForwarder QueryResultForwarder

	planner Planner

	queryExecFactory QueryExecutorFactory
}

// QueryExecutorFactory creates a new QueryExecutor.
type QueryExecutorFactory func(*Server, MutationExecFactory) QueryExecutor

// NewServer creates GRPC handlers.
func NewServer(env querybrokerenv.QueryBrokerEnv, agentsTracker AgentsTracker, dataPrivacy DataPrivacy,
	mds metadatapb.MetadataTracepointServiceClient, mdconf metadatapb.MetadataConfigServiceClient,
	natsConn *nats.Conn, queryExecFactory QueryExecutorFactory) (*Server, error) {
	var udfInfo udfspb.UDFInfo
	if err := loadUDFInfo(&udfInfo); err != nil {
		return nil, err
	}
	c, err := goplanner.New(&udfInfo)
	if err != nil {
		return nil, err
	}

	return NewServerWithForwarderAndPlanner(env, agentsTracker, dataPrivacy, NewQueryResultForwarder(), mds, mdconf,
		natsConn, c, queryExecFactory)
}

// NewServerWithForwarderAndPlanner is NewServer with a QueryResultForwarder and a planner generating func.
func NewServerWithForwarderAndPlanner(env querybrokerenv.QueryBrokerEnv,
	agentsTracker AgentsTracker,
	dataPrivacy DataPrivacy,
	resultForwarder QueryResultForwarder,
	mds metadatapb.MetadataTracepointServiceClient,
	mdconf metadatapb.MetadataConfigServiceClient,
	natsConn *nats.Conn,
	planner Planner,
	queryExecFactory QueryExecutorFactory) (*Server, error) {
	s := &Server{
		env:               env,
		agentsTracker:     agentsTracker,
		dataPrivacy:       dataPrivacy,
		resultForwarder:   resultForwarder,
		natsConn:          natsConn,
		mdtp:              mds,
		mdconf:            mdconf,
		planner:           planner,
		queryExecFactory:  queryExecFactory,
		healthcheckQuitCh: make(chan struct{}),
	}
	s.hcStatus.Store(fmt.Errorf("no healthcheck has run yet"))
	go s.runHealthcheck()
	return s, nil
}

// Close frees the planner memory in the server.
func (s *Server) Close() {
	s.healthcheckQuitOnce.Do(func() { close(s.healthcheckQuitCh) })
	if s.planner != nil {
		s.planner.Free()
	}
}

func loadUDFInfo(udfInfoPb *udfspb.UDFInfo) error {
	b, err := funcs.Asset("src/vizier/funcs/data/udf.pb")
	if err != nil {
		return err
	}
	return proto.Unmarshal(b, udfInfoPb)
}

type healthcheckConsumer struct {
	receivedRowBatches int32
	receivedRows       int64
}

func (h *healthcheckConsumer) Consume(result *vizierpb.ExecuteScriptResponse) error {
	if data := result.GetData(); data != nil {
		if rb := data.GetBatch(); rb != nil {
			h.receivedRowBatches++
			h.receivedRows += rb.NumRows
		}
	}
	return nil
}

// CheckHealth runs the health check and returns an error if it didn't pass.
func (s *Server) CheckHealth(ctx context.Context) error {
	checkVersionScript := `import px; px.display(px.Version())`
	req := &vizierpb.ExecuteScriptRequest{
		QueryStr:  checkVersionScript,
		QueryName: "healthcheck",
	}
	consumer := &healthcheckConsumer{
		receivedRowBatches: 0,
		receivedRows:       0,
	}

	queryExec := s.queryExecFactory(s, NewMutationExecutor)
	if err := queryExec.Run(ctx, req, consumer); err != nil {
		return err
	}
	if err := queryExec.Wait(); err != nil {
		return err
	}

	if consumer.receivedRowBatches == 0 || consumer.receivedRows == int64(0) {
		return fmt.Errorf("results not returned on health check for query ID %s", queryExec.QueryID().String())
	}

	if consumer.receivedRows > int64(1) {
		// We expect only one row to be received from this query.
		return fmt.Errorf("bad results on healthcheck for query ID %s", queryExec.QueryID().String())
	}

	return nil
}

func (s *Server) runHealthcheck() {
	t := time.NewTicker(healthCheckInterval)
	defer t.Stop()
	for {
		select {
		case <-s.healthcheckQuitCh:
			return
		case <-t.C:
			ctx, cancel := context.WithTimeout(context.Background(), healthCheckInterval)
			defer cancel()
			status := s.CheckHealth(ctx)
			s.hcStatus.Store(status)
		}
	}
}

// HealthCheck continually responds with the current health of Vizier.
func (s *Server) HealthCheck(req *vizierpb.HealthCheckRequest, srv vizierpb.VizierService_HealthCheckServer) error {
	t := time.NewTicker(healthCheckInterval)
	defer t.Stop()
	for {
		hcResult := s.hcStatus.Load()
		// Pass.
		code := int32(codes.OK)
		if hcResult != nil {
			log.Infof("Received unhealthy heath check result: %s", hcResult.Error())
			code = int32(codes.Unavailable)
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
		case <-t.C:
			continue
		}
	}
}

type executeServerConsumer struct {
	srv vizierpb.VizierService_ExecuteScriptServer
}

func (e *executeServerConsumer) Consume(result *vizierpb.ExecuteScriptResponse) error {
	return e.srv.Send(result)
}

type encryptConsumer struct {
	c       QueryResultConsumer
	jwkKey  jwk.Key
	keyAlg  jwa.KeyEncryptionAlgorithm
	encAlg  jwa.ContentEncryptionAlgorithm
	compAlg jwa.CompressionAlgorithm
}

func newEncryptConsumer(c QueryResultConsumer, encryptOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions) (*encryptConsumer, error) {
	e := &encryptConsumer{c: c}
	jwkKey, err := jwk.ParseKey([]byte(encryptOpts.GetJwkKey()))
	if err != nil {
		return nil, err
	}
	e.jwkKey = jwkKey
	err = e.keyAlg.Accept(encryptOpts.GetKeyAlg())
	if err != nil {
		return nil, err
	}
	err = e.encAlg.Accept(encryptOpts.GetContentAlg())
	if err != nil {
		return nil, err
	}
	err = e.compAlg.Accept(encryptOpts.GetCompressionAlg())
	if err != nil {
		return nil, err
	}
	return e, nil
}

func (e *encryptConsumer) Consume(result *vizierpb.ExecuteScriptResponse) error {
	if result.GetData() != nil {
		data := result.GetData()
		if data.Batch != nil {
			b, err := data.Batch.Marshal()
			if err != nil {
				return err
			}

			eb, err := jwe.Encrypt(b, e.keyAlg, e.jwkKey, e.encAlg, e.compAlg)
			if err != nil {
				return err
			}

			data.Batch = nil
			data.EncryptedBatch = eb
		}
	}
	return e.c.Consume(result)
}

// ExecuteScript executes the script and sends results through the gRPC stream.
func (s *Server) ExecuteScript(req *vizierpb.ExecuteScriptRequest, srv vizierpb.VizierService_ExecuteScriptServer) error {
	ctx := context.WithValue(srv.Context(), execStartKey, time.Now())

	var consumer QueryResultConsumer
	consumer = &executeServerConsumer{
		srv: srv,
	}
	if req.EncryptionOptions != nil {
		c, err := newEncryptConsumer(consumer, req.EncryptionOptions)
		if err != nil {
			return err
		}
		consumer = c
	}
	queryExec := s.queryExecFactory(s, NewMutationExecutor)
	if err := queryExec.Run(ctx, req, consumer); err != nil {
		return err
	}
	log.Infof("Launched query: %s", queryExec.QueryID())

	return queryExec.Wait()
}

// GenerateOTelScript generates an OTel script for the given DataFrame script.
func (s *Server) GenerateOTelScript(ctx context.Context, req *vizierpb.GenerateOTelScriptRequest) (*vizierpb.GenerateOTelScriptResponse, error) {
	info := s.agentsTracker.GetAgentInfo()
	if info == nil {
		return nil, status.Error(codes.Unavailable, "not ready yet")
	}

	distributedState := s.agentsTracker.GetAgentInfo().DistributedState()
	plannerState := &distributedpb.LogicalPlannerState{
		DistributedState: &distributedState,
	}
	resp, err := s.planner.GenerateOTelScript(&plannerpb.GenerateOTelScriptRequest{
		LogicalPlannerState: plannerState,
		PxlScript:           req.GetPxlScript(),
	})
	if err != nil {
		return nil, err
	}
	return &vizierpb.GenerateOTelScriptResponse{
		Status:     StatusToVizierStatus(resp.Status),
		OTelScript: resp.GetOTelScript(),
	}, nil
}

// TransferResultChunk implements the API that allows the query broker receive streamed results
// from Carnot instances.
func (s *Server) TransferResultChunk(srv carnotpb.ResultSinkService_TransferResultChunkServer) error {
	var queryID uuid.UUID
	// will be set if this particular stream is sending the results for a table.
	// not set if it is sending the results for exec stats.
	var tableName string
	// if tableName is set, tracks whether this particular stream has sent EOS.
	// used to cancel the query if the connection is closed by the agent before EOS is sent.
	sentEos := false

	sendAndClose := func(success bool, cancelForwarderClient bool, message string) error {
		err := srv.SendAndClose(&carnotpb.TransferResultChunkResponse{
			Success: success,
			Message: message,
		})
		if !success {
			if queryID == uuid.Nil {
				log.Errorf("TransferResultChunk encountered an error for unknown query ID: %s", message)
			} else if cancelForwarderClient {
				// If the stream died because of an HTTP/TCP timeout we don't want to cancel the result forwarder immediately
				// since we might get a retry connection from TransferResultChunk. The result forwarder has an internal timeout,
				// so if no data is transferred after that timeout it will cancel the query.

				// Stop the client stream, if it still exists in the result forwarder.
				// It may have already been cancelled before this point.
				log.Errorf("TransferResultChunk cancelling client stream for query %s: %s", queryID.String(), message)
				clientStreamErr := fmt.Errorf(message)
				s.resultForwarder.ProducerCancelStream(queryID, clientStreamErr)
			}
		}
		return err
	}

	handleAgentStreamClosed := func() error {
		if tableName != "" && !sentEos {
			// Send an error and cancel the query if the stream is closed unexpectedly.
			return sendAndClose( /*success*/ false, false, fmt.Sprintf(
				"Agent stream was unexpectedly closed for table %s of query %s before the results completed",
				tableName, queryID.String(),
			))
		}
		return sendAndClose( /*success*/ true, false, "")
	}

	// producerCtx gets set once we know what the queryID is.
	producerCtx := context.Background()

	for {
		select {
		case <-producerCtx.Done():
			return handleAgentStreamClosed()
		case <-srv.Context().Done():
			return handleAgentStreamClosed()
		default:
			msg, err := srv.Recv()
			// Stream closed from client side.
			if err != nil && err == io.EOF {
				return handleAgentStreamClosed()
			}
			if err != nil {
				if s, ok := status.FromError(err); ok {
					if s.Code() == codes.Unavailable {
						return sendAndClose( /*success*/ false, false,
							fmt.Sprintf("Agent stream disconnected for query %s", queryID.String()))
					}
				}

				return sendAndClose( /*success*/ false, true, err.Error())
			}

			qid, err := utils.UUIDFromProto(msg.QueryID)
			if err != nil {
				return sendAndClose( /*success*/ false, true, err.Error())
			}

			if queryID == uuid.Nil {
				queryID = qid
				producerCtx, err = s.resultForwarder.GetProducerCtx(queryID)
				if err != nil {
					return sendAndClose( /*success*/ false, true, err.Error())
				}
			}
			if queryID != qid {
				return sendAndClose( /*success*/ false, true, fmt.Sprintf(
					"Received results from multiple queries in the same TransferResultChunk stream: %s and %s",
					queryID, qid,
				))
			}

			err = s.resultForwarder.ForwardQueryResult(srv.Context(), msg)
			// If the result wasn't forwarded, the client stream may have be cancelled.
			// This should not cause TransferResultChunk to return an error, we just include in the response
			// that the latest result chunk was not forwarded.
			if err != nil {
				log.WithError(err).Infof("Could not forward result message for query %s", queryID)
				return sendAndClose( /*success*/ false, true, err.Error())
			}

			// Keep track of which table this stream is sending results for, and if it has sent EOS yet.
			if queryResult := msg.GetQueryResult(); queryResult != nil {
				if tableName == "" {
					tableName = queryResult.GetTableName()
				}
				if tableName != queryResult.GetTableName() {
					return sendAndClose( /*success*/ false, true, fmt.Sprintf(
						"Received results from multiple tables for query %s in the same TransferResultChunk stream"+
							": %s and %s", queryID.String(), tableName, queryResult.GetTableName(),
					))
				}
				if batch := queryResult.GetRowBatch(); batch != nil {
					if batch.GetEos() {
						sentEos = true
					}
				}
			}
		}
	}
}
