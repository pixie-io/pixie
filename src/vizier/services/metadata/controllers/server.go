package controllers

import (
	"context"
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	types "github.com/gogo/protobuf/types"
	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	schemapb "pixielabs.ai/pixielabs/src/table_store/proto"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadataenv"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
	agentpb "pixielabs.ai/pixielabs/src/vizier/services/shared/agentpb"
)

// UnhealthyAgentThreshold is the amount of time where an agent is considered unhealthy if
// its last heartbeat is greater than this value.
const UnhealthyAgentThreshold = 30 * time.Second

// Server defines an gRPC server type.
type Server struct {
	env               metadataenv.MetadataEnv
	agentManager      AgentManager
	tracepointManager *TracepointManager
	clock             utils.Clock
	// The current cursor that is actively running the GetAgentsUpdate stream. Only one GetAgentsUpdate
	// stream should be running at a time.
	getAgentsCursor uuid.UUID
	mu              sync.Mutex
}

// NewServerWithClock creates a new server with a clock and the ability to configure the chunk size and
// update period of the GetAgentUpdates handler.
func NewServerWithClock(env metadataenv.MetadataEnv, agtMgr AgentManager, tracepointManager *TracepointManager,
	clock utils.Clock) (*Server, error) {
	return &Server{
		env:               env,
		agentManager:      agtMgr,
		clock:             clock,
		tracepointManager: tracepointManager,
	}, nil
}

// NewServer creates GRPC handlers.
func NewServer(env metadataenv.MetadataEnv, agtMgr AgentManager, tracepointManager *TracepointManager) (*Server, error) {
	clock := utils.SystemClock{}
	return NewServerWithClock(env, agtMgr, tracepointManager, clock)
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
	computedSchema, err := s.agentManager.GetComputedSchema()
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
	agents, err := s.agentManager.GetActiveAgents()
	if err != nil {
		return nil, err
	}

	currentTime := s.clock.Now()

	// Populate AgentInfoResponse.
	agentResponses := make([]*metadatapb.AgentMetadata, 0)
	for _, agent := range agents {
		state := agentpb.AGENT_STATE_HEALTHY
		timeSinceLastHb := currentTime.Sub(time.Unix(0, agent.LastHeartbeatNS))
		if timeSinceLastHb > UnhealthyAgentThreshold {
			state = agentpb.AGENT_STATE_UNRESPONSIVE
		}

		resp := metadatapb.AgentMetadata{
			Agent: agent,
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

	cursor := s.agentManager.NewAgentUpdateCursor()
	defer s.agentManager.DeleteAgentUpdateCursor(cursor)

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

		updates, newComputedSchema, err := s.agentManager.GetAgentUpdates(cursor)
		if err == errNoComputedSchemas {
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

		log.Infof("Sent %d agent updates", len(updates))
		time.Sleep(agentUpdatePeriod)
	}
}

// RegisterTracepoint is a request to register the tracepoints specified in the TracepointDeployment on all agents.
func (s *Server) RegisterTracepoint(ctx context.Context, req *metadatapb.RegisterTracepointRequest) (*metadatapb.RegisterTracepointResponse, error) {
	// TODO(michelle): Some additional work will need to be done
	// in order to transactionalize the creation of multiple tracepoints. The current behavior is temporary just
	// so we can get the updated protos out.
	responses := make([]*metadatapb.RegisterTracepointResponse_TracepointStatus, len(req.Requests))

	// Create tracepoint.
	for i, tp := range req.Requests {
		ttl, err := types.DurationFromProto(tp.TTL)
		if err != nil {
			return nil, err
		}
		tracepointID, err := s.tracepointManager.CreateTracepoint(tp.Name, tp.TracepointDeployment, ttl)
		if err != nil && err != ErrTracepointAlreadyExists {
			return nil, err
		}
		if err == ErrTracepointAlreadyExists {
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
		agents, err := s.agentManager.GetActiveAgents()
		if err != nil {
			return nil, err
		}
		agentIDs := make([]uuid.UUID, len(agents))
		for i, agent := range agents {
			agentIDs[i] = utils.UUIDFromProtoOrNil(agent.Info.AgentID)
		}

		// Register tracepoint on all agents.
		err = s.tracepointManager.RegisterTracepoint(agentIDs, *tracepointID, tp.TracepointDeployment)
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

		tracepointInfos, err = s.tracepointManager.GetTracepointsForIDs(ids)
	} else {
		tracepointInfos, err = s.tracepointManager.GetAllTracepoints()
	}

	if err != nil {
		return nil, err
	}

	tracepointState := make([]*metadatapb.GetTracepointInfoResponse_TracepointState, len(tracepointInfos))

	for i, tracepoint := range tracepointInfos {
		if tracepoint == nil { // TracepointDeployment does not exist.
			tracepointState[i] = &metadatapb.GetTracepointInfoResponse_TracepointState{
				ID:    req.IDs[i],
				State: statuspb.UNKNOWN_STATE,
				Statuses: []*statuspb.Status{&statuspb.Status{
					ErrCode: statuspb.NOT_FOUND,
				}},
			}
			continue
		}
		tUUID := utils.UUIDFromProtoOrNil(tracepoint.ID)

		tracepointStates, err := s.tracepointManager.GetTracepointStates(tUUID)
		if err != nil {
			return nil, err
		}

		state, statuses := getTracepointStateFromAgentTracepointStates(tracepointStates)

		schemas := make([]string, len(tracepoint.Tracepoint.Tracepoints))
		for i, tracepoint := range tracepoint.Tracepoint.Tracepoints {
			schemas[i] = tracepoint.TableName
		}

		tracepointState[i] = &metadatapb.GetTracepointInfoResponse_TracepointState{
			ID:            tracepoint.ID,
			State:         state,
			Statuses:      statuses,
			Name:          tracepoint.Name,
			ExpectedState: tracepoint.ExpectedState,
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
		if s.State == statuspb.TERMINATED_STATE {
			numTerminated++
		} else if s.State == statuspb.FAILED_STATE {
			numFailed++
			if s.Status.ErrCode != statuspb.FAILED_PRECONDITION && s.Status.ErrCode != statuspb.OK {
				statuses = append(statuses, s.Status)
			}
		} else if s.State == statuspb.PENDING_STATE {
			numPending++
		} else if s.State == statuspb.RUNNING_STATE {
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
	err := s.tracepointManager.RemoveTracepoints(req.Names)
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

	err := s.agentManager.UpdateConfig(splitName[0], splitName[1], req.Key, req.Value)
	if err != nil {
		return nil, err
	}

	return &metadatapb.UpdateConfigResponse{
		Status: &statuspb.Status{
			ErrCode: statuspb.OK,
		},
	}, nil
}
