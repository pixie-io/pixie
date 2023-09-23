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
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"strings"
	"sync"
	"time"

	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/carnot/planner/distributedpb"
	"px.dev/pixie/src/carnot/planner/dynamic_tracing/ir/logicalpb"
	"px.dev/pixie/src/common/base/statuspb"
	"px.dev/pixie/src/table_store/schemapb"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/vizier/services/metadata/controllers/agent"
	"px.dev/pixie/src/vizier/services/metadata/controllers/k8smeta"
	"px.dev/pixie/src/vizier/services/metadata/controllers/tracepoint"
	"px.dev/pixie/src/vizier/services/metadata/metadataenv"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/services/shared/agentpb"
	"px.dev/pixie/src/vizier/utils/datastore"
)

// UnhealthyAgentThreshold is the amount of time where an agent is considered unhealthy if
// its last heartbeat is greater than this value.
const UnhealthyAgentThreshold = 30 * time.Second

// Server defines an gRPC server type.
type Server struct {
	env    metadataenv.MetadataEnv
	ds     datastore.MultiGetterSetterDeleterCloser
	pls    k8smeta.PodLabelStore
	agtMgr agent.Manager
	tpMgr  *tracepoint.Manager
	// The current cursor that is actively running the GetAgentsUpdate stream. Only one GetAgentsUpdate
	// stream should be running at a time.
	getAgentsCursor uuid.UUID
	mu              sync.Mutex
}

// NewServer creates GRPC handlers.
func NewServer(env metadataenv.MetadataEnv, ds datastore.MultiGetterSetterDeleterCloser, pls k8smeta.PodLabelStore, agtMgr agent.Manager, tpMgr *tracepoint.Manager) *Server {
	return &Server{
		env:    env,
		ds:     ds,
		pls:    pls,
		agtMgr: agtMgr,
		tpMgr:  tpMgr,
	}
}

func convertToRelationMap(computedSchema *storepb.ComputedSchema) (*schemapb.Schema, error) {
	schemas := computedSchema.Tables
	respSchemaPb := &schemapb.Schema{}
	respSchemaPb.RelationMap = make(map[string]*schemapb.Relation)

	for _, schema := range schemas {
		columnPbs := make([]*schemapb.Relation_ColumnInfo, len(schema.Columns))
		for j, column := range schema.Columns {
			columnPbs[j] = &schemapb.Relation_ColumnInfo{
				ColumnName:         column.Name,
				ColumnType:         column.DataType,
				ColumnDesc:         column.Desc,
				PatternType:        column.PatternType,
				ColumnSemanticType: column.SemanticType,
			}
		}
		schemaPb := &schemapb.Relation{
			Columns: columnPbs,
			Desc:    schema.Desc,
		}
		respSchemaPb.RelationMap[schema.Name] = schemaPb
	}

	return respSchemaPb, nil
}

func convertToSchemaInfo(computedSchema *storepb.ComputedSchema) ([]*distributedpb.SchemaInfo, error) {
	schemaInfo := make([]*distributedpb.SchemaInfo, len(computedSchema.Tables))

	for idx, schema := range computedSchema.Tables {
		columnPbs := make([]*schemapb.Relation_ColumnInfo, len(schema.Columns))
		for j, column := range schema.Columns {
			columnPbs[j] = &schemapb.Relation_ColumnInfo{
				ColumnName:         column.Name,
				ColumnType:         column.DataType,
				ColumnDesc:         column.Desc,
				PatternType:        column.PatternType,
				ColumnSemanticType: column.SemanticType,
			}
		}
		schemaPb := &schemapb.Relation{
			Columns: columnPbs,
		}

		agentIDs, ok := computedSchema.TableNameToAgentIDs[schema.Name]
		if !ok {
			return nil, fmt.Errorf("Can't find agentIDs for schema %s", schema.Name)
		}

		schemaInfo[idx] = &distributedpb.SchemaInfo{
			Name:      schema.Name,
			Relation:  schemaPb,
			AgentList: agentIDs.AgentID,
		}
	}

	return schemaInfo, nil
}

// GetSchemas returns the schemas in the system.
func (s *Server) GetSchemas(ctx context.Context, req *metadatapb.SchemaRequest) (*metadatapb.SchemaResponse, error) {
	computedSchema, err := s.agtMgr.GetComputedSchema()
	if err != nil {
		return nil, err
	}
	respSchemaPb, err := convertToRelationMap(computedSchema)
	if err != nil {
		return nil, err
	}
	resp := &metadatapb.SchemaResponse{
		Schema: respSchemaPb,
	}
	return resp, nil
}

// GetAgentInfo returns information about registered agents.
func (s *Server) GetAgentInfo(ctx context.Context, req *metadatapb.AgentInfoRequest) (*metadatapb.AgentInfoResponse, error) {
	agents, err := s.agtMgr.GetActiveAgents()
	if err != nil {
		return nil, err
	}

	currentTime := time.Now()

	// Populate AgentInfoResponse.
	agentResponses := make([]*metadatapb.AgentMetadata, 0)
	for _, agt := range agents {
		state := agentpb.AGENT_STATE_HEALTHY
		timeSinceLastHb := currentTime.Sub(time.Unix(0, agt.LastHeartbeatNS))
		if timeSinceLastHb > UnhealthyAgentThreshold {
			state = agentpb.AGENT_STATE_UNRESPONSIVE
		}

		resp := metadatapb.AgentMetadata{
			Agent: agt,
			Status: &agentpb.AgentStatus{
				NSSinceLastHeartbeat: timeSinceLastHb.Nanoseconds(),
				State:                state,
			},
		}
		agentResponses = append(agentResponses, &resp)
	}

	resp := metadatapb.AgentInfoResponse{
		Info: agentResponses,
	}

	return &resp, nil
}

// GetAgentUpdates streams agent updates to the requestor periodically as they come in.
// It first sends the complete initial agent state in the beginning of the request, and then deltas after that.
// Note that as it is currently designed, it can only handle one stream at a time (to a single metadata server).
// That is because the agent manager tracks the deltas in the state in a single object, rather than per request.
func (s *Server) GetAgentUpdates(req *metadatapb.AgentUpdatesRequest, srv metadatapb.MetadataService_GetAgentUpdatesServer) error {
	if req.MaxUpdatesPerResponse == 0 {
		return status.Error(codes.InvalidArgument, "Max updates per agent should be specified in AgentUpdatesRequest")
	}
	agentChunkSize := int(req.MaxUpdatesPerResponse)
	if req.MaxUpdateInterval == nil {
		return status.Error(codes.InvalidArgument, "Max update interval should be specified in AgentUpdatesRequest")
	}
	agentUpdatePeriod, err := types.DurationFromProto(req.MaxUpdateInterval)
	if err != nil {
		return status.Error(codes.Internal, fmt.Sprintf("Failed to parse duration: %+v", err))
	}

	cursor := s.agtMgr.NewAgentUpdateCursor()
	defer s.agtMgr.DeleteAgentUpdateCursor(cursor)

	// This is a temporary hack. We're seeing a bug where the grpc streamServer is unable to
	// detect that a stream has hit an HTTP2 timeout. This enforces that old, inactive
	// GetAgentUpdate streams are terminated.
	s.mu.Lock()
	s.getAgentsCursor = cursor
	s.mu.Unlock()

	for {
		s.mu.Lock()
		currCursor := s.getAgentsCursor
		s.mu.Unlock()

		if cursor != currCursor {
			log.Trace("Only one GetAgentUpdates stream can be active at once... Terminating")
			return nil
		}

		updates, newComputedSchema, err := s.agtMgr.GetAgentUpdates(cursor)
		if err == agent.ErrNoComputedSchemas {
			// We need to wait until we have computed schemas
			time.Sleep(agentUpdatePeriod)
			continue
		}

		if err != nil {
			return err
		}

		currentIdx := 0
		finishedUpdates := len(updates) == 0
		finishedSchema := newComputedSchema == nil

		if finishedSchema && finishedUpdates {
			select {
			case <-srv.Context().Done():
				log.Infof("Client closed context for GetAgentUpdates")
				return nil
			default:
				// Send an empty update after the duration has passed if we have no new information.
				err := srv.Send(&metadatapb.AgentUpdatesResponse{
					AgentSchemasUpdated: false,
				})
				if err != nil {
					log.WithError(err).Errorf("Error sending noop agent updates")
					return err
				}
			}
		} else {
			// Chunk up the data so we don't overwhelm the query broker with a lot of data at once.
			for !(finishedUpdates && finishedSchema) {
				select {
				case <-srv.Context().Done():
					log.Infof("Client closed context for GetAgentUpdates")
					return nil
				default:
					response := &metadatapb.AgentUpdatesResponse{
						AgentSchemasUpdated: false,
					}
					for idx := currentIdx; idx < currentIdx+agentChunkSize; idx++ {
						if idx < len(updates) {
							response.AgentUpdates = append(response.AgentUpdates, updates[idx])
						} else {
							finishedUpdates = true
						}
					}

					// Send the schema once all of the other updates have been sent for this batch.
					// That way, the schema will never prematurely refer to an agent that the client
					// is currently unaware of.
					if finishedUpdates && !finishedSchema {
						schemas, err := convertToSchemaInfo(newComputedSchema)
						if err != nil {
							log.WithError(err).Errorf("Received error converting schemas in GetAgentUpdates")
							return err
						}
						response.AgentSchemas = schemas
						finishedSchema = true
						response.AgentSchemasUpdated = true
					}

					currentIdx += agentChunkSize

					if finishedSchema && finishedUpdates {
						response.EndOfVersion = true
					}

					err := srv.Send(response)
					if err != nil {
						log.WithError(err).Errorf("Error sending agent updates with contents")
						return err
					}
				}
			}
		}

		log.Tracef("Sent %d agent updates", len(updates))
		time.Sleep(agentUpdatePeriod)
	}
}

// GetWithPrefixKey fetches all the metadata KVs with the given prefix. This is used for debug purposes.
func (s *Server) GetWithPrefixKey(ctx context.Context, req *metadatapb.WithPrefixKeyRequest) (*metadatapb.WithPrefixKeyResponse, error) {
	prefix := req.Prefix
	if prefix == "" {
		return nil, errors.New("Empty prefixes are not supported")
	}
	mName := req.Proto
	var msg proto.Message
	if mName != "" {
		t := proto.MessageType(mName)
		if t != nil {
			msg = reflect.New(t.Elem()).Interface().(proto.Message)
		}
	}
	ks, vs, err := s.pls.GetWithPrefix(prefix)
	if err != nil {
		return nil, err
	}
	resp := &metadatapb.WithPrefixKeyResponse{}
	for i := range ks {
		kv := &metadatapb.WithPrefixKeyResponse_KV{
			Key:   ks[i],
			Value: vs[i],
		}
		resp.Kvs = append(resp.Kvs, kv)
		if msg == nil {
			continue
		}
		msg.Reset()
		err := proto.Unmarshal(vs[i], msg)
		if err != nil {
			continue
		}
		js, err := json.Marshal(msg)
		if err != nil {
			continue
		}
		kv.Value = js
	}
	return resp, nil
}

// ConvertLabelsToPods fetches all the pods in the PodLabelStore that match the labels described in the input tp,
// and then convert the LabelSelector to a PodProcess.
func (s *Server) ConvertLabelsToPods(tp *logicalpb.TracepointDeployment) error {
	ls := tp.GetDeploymentSpec().GetLabelSelector()
	// If no label selector is provided, no change is needed.
	if ls == nil {
		return nil
	}

	namespace := ls.GetNamespace()

	ml := ls.GetLabels()
	if ml == nil {
		return nil
	}

	// Fetch pods based on match_labels.
	pods, err := s.pls.FetchPodsWithLabels(namespace, ml)
	if err != nil {
		log.WithError(err).Error("Failed to fetch pods using match labels.")
		return err
	}

	if len(pods) == 0 {
		return nil
	}

	// Add namespace to pod name, as needed in Stirling.
	for i, pod := range pods {
		pods[i] = namespace + "/" + pod
	}

	tp.DeploymentSpec = &logicalpb.DeploymentSpec{
		TargetOneof: &logicalpb.DeploymentSpec_PodProcess_{
			PodProcess: &logicalpb.DeploymentSpec_PodProcess{
				Pods:      pods,
				Container: ls.GetContainer(),
				Process:   ls.GetProcess(),
			},
		},
	}
	return nil
}

// RegisterTracepoint is a request to register the tracepoints specified in the TracepointDeployment on all agents.
func (s *Server) RegisterTracepoint(ctx context.Context, req *metadatapb.RegisterTracepointRequest) (*metadatapb.RegisterTracepointResponse, error) {
	responses := make([]*metadatapb.RegisterTracepointResponse_TracepointStatus, len(req.Requests))

	// Create tracepoint.
	for i, tp := range req.Requests {
		ttl, err := types.DurationFromProto(tp.TTL)
		if err != nil {
			return nil, err
		}

		err = s.ConvertLabelsToPods(tp.TracepointDeployment)
		if err != nil {
			return nil, err
		}

		tracepointID, err := s.tpMgr.CreateTracepoint(tp.Name, tp.TracepointDeployment, ttl)
		if err != nil && err != tracepoint.ErrTracepointAlreadyExists {
			return nil, err
		}
		if err == tracepoint.ErrTracepointAlreadyExists {
			responses[i] = &metadatapb.RegisterTracepointResponse_TracepointStatus{
				ID: utils.ProtoFromUUID(*tracepointID),
				Status: &statuspb.Status{
					ErrCode: statuspb.ALREADY_EXISTS,
				},
				Name: tp.Name,
			}
			continue
		}

		responses[i] = &metadatapb.RegisterTracepointResponse_TracepointStatus{
			ID: utils.ProtoFromUUID(*tracepointID),
			Status: &statuspb.Status{
				ErrCode: statuspb.OK,
			},
			Name: tp.Name,
		}

		// Get all agents currently running.
		agents, err := s.agtMgr.GetActiveAgents()
		if err != nil {
			return nil, err
		}

		err = s.tpMgr.RegisterTracepoint(agents, *tracepointID, tp.TracepointDeployment)
		if err != nil {
			return nil, err
		}
	}

	resp := &metadatapb.RegisterTracepointResponse{
		Tracepoints: responses,
		Status: &statuspb.Status{
			ErrCode: statuspb.OK,
		},
	}

	return resp, nil
}

// GetTracepointInfo is a request to check the status for the given tracepoint.
func (s *Server) GetTracepointInfo(ctx context.Context, req *metadatapb.GetTracepointInfoRequest) (*metadatapb.GetTracepointInfoResponse, error) {
	var tracepointInfos []*storepb.TracepointInfo
	var err error
	if len(req.IDs) > 0 {
		ids := make([]uuid.UUID, len(req.IDs))
		for i, id := range req.IDs {
			ids[i] = utils.UUIDFromProtoOrNil(id)
		}

		tracepointInfos, err = s.tpMgr.GetTracepointsForIDs(ids)
	} else {
		tracepointInfos, err = s.tpMgr.GetAllTracepoints()
	}

	if err != nil {
		return nil, err
	}

	tracepointState := make([]*metadatapb.GetTracepointInfoResponse_TracepointState, len(tracepointInfos))

	for i, tp := range tracepointInfos {
		if tp == nil { // TracepointDeployment does not exist.
			tracepointState[i] = &metadatapb.GetTracepointInfoResponse_TracepointState{
				ID:    req.IDs[i],
				State: statuspb.UNKNOWN_STATE,
				Statuses: []*statuspb.Status{{
					ErrCode: statuspb.NOT_FOUND,
				}},
			}
			continue
		}
		tUUID := utils.UUIDFromProtoOrNil(tp.ID)

		tracepointStates, err := s.tpMgr.GetTracepointStates(tUUID)
		if err != nil {
			return nil, err
		}

		state, statuses := getTracepointStateFromAgentTracepointStates(tracepointStates)

		schemas := make([]string, len(tp.Tracepoint.Programs))
		for i, t := range tp.Tracepoint.Programs {
			schemas[i] = t.TableName
		}

		tracepointState[i] = &metadatapb.GetTracepointInfoResponse_TracepointState{
			ID:            tp.ID,
			State:         state,
			Statuses:      statuses,
			Name:          tp.Name,
			ExpectedState: tp.ExpectedState,
			SchemaNames:   schemas,
		}
	}

	return &metadatapb.GetTracepointInfoResponse{
		Tracepoints: tracepointState,
	}, nil
}

func getTracepointStateFromAgentTracepointStates(agentStates []*storepb.AgentTracepointStatus) (statuspb.LifeCycleState, []*statuspb.Status) {
	if len(agentStates) == 0 {
		return statuspb.PENDING_STATE, nil
	}

	numFailed := 0
	numTerminated := 0
	numPending := 0
	numRunning := 0
	statuses := make([]*statuspb.Status, 0)

	for _, s := range agentStates {
		switch s.State {
		case statuspb.TERMINATED_STATE:
			numTerminated++
		case statuspb.FAILED_STATE:
			numFailed++
			if s.Status.ErrCode != statuspb.FAILED_PRECONDITION && s.Status.ErrCode != statuspb.OK {
				statuses = append(statuses, s.Status)
			}
		case statuspb.PENDING_STATE:
			numPending++
		case statuspb.RUNNING_STATE:
			numRunning++
		}
	}

	if numTerminated > 0 { // If any agentTracepoints are terminated, then we consider the tracepoint in an terminated state.
		return statuspb.TERMINATED_STATE, []*statuspb.Status{}
	}

	if numRunning > 0 { // If a single agentTracepoint is running, then we consider the overall tracepoint as healthy.
		return statuspb.RUNNING_STATE, []*statuspb.Status{}
	}

	if numPending > 0 { // If no agentTracepoints are running, but some are in a pending state, the tracepoint is pending.
		return statuspb.PENDING_STATE, []*statuspb.Status{}
	}

	if numFailed > 0 { // If there are no terminated/running/pending tracepoints, then the tracepoint is failed.
		if len(statuses) == 0 {
			return statuspb.FAILED_STATE, []*statuspb.Status{agentStates[0].Status} // If there are no non FAILED_PRECONDITION statuses, just use the error from the first agent.
		}
		return statuspb.FAILED_STATE, statuses
	}

	return statuspb.UNKNOWN_STATE, []*statuspb.Status{}
}

// RemoveTracepoint is a request to evict the given tracepoint on all agents.
func (s *Server) RemoveTracepoint(ctx context.Context, req *metadatapb.RemoveTracepointRequest) (*metadatapb.RemoveTracepointResponse, error) {
	err := s.tpMgr.RemoveTracepoints(req.Names)
	if err != nil {
		return nil, err
	}

	return &metadatapb.RemoveTracepointResponse{
		Status: &statuspb.Status{
			ErrCode: statuspb.OK,
		},
	}, nil
}

// UpdateConfig updates the config for the specified agent.
func (s *Server) UpdateConfig(ctx context.Context, req *metadatapb.UpdateConfigRequest) (*metadatapb.UpdateConfigResponse, error) {
	splitName := strings.Split(req.AgentPodName, "/")
	if len(splitName) != 2 {
		return nil, errors.New("Incorrectly formatted pod name. Must be of the form '<ns>/<podName>'")
	}

	err := s.agtMgr.UpdateConfig(splitName[0], splitName[1], req.Key, req.Value)
	if err != nil {
		return nil, err
	}

	return &metadatapb.UpdateConfigResponse{
		Status: &statuspb.Status{
			ErrCode: statuspb.OK,
		},
	}, nil
}
