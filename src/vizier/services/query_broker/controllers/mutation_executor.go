package controllers

import (
	"context"
	"fmt"

	uuid "github.com/satori/go.uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	logicalplanner "pixielabs.ai/pixielabs/src/carnot/planner"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	vizierpb "pixielabs.ai/pixielabs/src/vizier/vizierpb"
)

// TracepointMap stores a map from the name to tracepoint info.
type TracepointMap map[string]*TracepointInfo

// MutationExecutor is responsible for running script mutations.
type MutationExecutor struct {
	planner           logicalplanner.GoPlanner
	mdtp              metadatapb.MetadataTracepointServiceClient
	tracker           AgentsTracker
	activeTracepoints TracepointMap
	outputTables      []string
}

// TracepointInfo stores information of a particular tracepoint.
type TracepointInfo struct {
	Name   string
	ID     uuid.UUID
	Status *statuspb.Status
}

// NewMutationExecutor creates a new mutation executor.
func NewMutationExecutor(
	planner logicalplanner.GoPlanner,
	mdtp metadatapb.MetadataTracepointServiceClient,
	tracker AgentsTracker) *MutationExecutor {
	return &MutationExecutor{
		planner:           planner,
		mdtp:              mdtp,
		tracker:           tracker,
		activeTracepoints: make(TracepointMap, 0),
	}
}

// Execute runs the mutation. On unknown errors it will return an error, otherwise we return a status message
// that has more context about the error message.
func (m *MutationExecutor) Execute(ctx context.Context, req *vizierpb.ExecuteScriptRequest, planOpts *planpb.PlanOptions) (*statuspb.Status, error) {
	convertedReq, err := VizierQueryRequestToPlannerMutationRequest(req)
	if err != nil {
		return nil, err
	}
	info := m.tracker.GetAgentInfo()
	plannerState := &distributedpb.LogicalPlannerState{
		DistributedState: info.DistributedState(),
		PlanOptions:      planOpts,
	}

	mutations, err := m.planner.CompileMutations(plannerState, convertedReq)
	if err != nil {
		log.WithError(err).Error("Got an error while compiling mutations")
		return nil, err
	}
	if mutations.Status != nil && mutations.Status.ErrCode != statuspb.OK {
		return mutations.Status, nil
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", aCtx.AuthToken))

	if len(mutations.Mutations) == 0 {
		// No mutations to apply.
		return nil, nil
	}

	registerTracepointsReq := &metadatapb.RegisterTracepointRequest{
		Requests: make([]*metadatapb.RegisterTracepointRequest_TracepointRequest, 0),
	}
	deleteTracepointsReq := &metadatapb.RemoveTracepointRequest{
		TracepointNames: make([]string, 0),
	}

	outputTablesMap := make(map[string]bool, 0)
	// TODO(zasgar): We should make sure that we don't simultaneosly add nd delete the tracepoint.
	// While this will probably work, we should restrict this because it's likely not the intended behavior.
	for _, mut := range mutations.Mutations {
		switch mut := mut.Mutation.(type) {
		case *plannerpb.CompileMutation_Trace:
			{
				name := mut.Trace.Name
				registerTracepointsReq.Requests = append(registerTracepointsReq.Requests,
					&metadatapb.RegisterTracepointRequest_TracepointRequest{
						Program:        mut.Trace,
						TracepointName: mut.Trace.Name,
						TTL:            mut.Trace.TTL,
					})

				if _, ok := m.activeTracepoints[name]; ok {
					return nil, fmt.Errorf("tracepoint with name '%s', already used", name)
				}
				for _, out := range mut.Trace.Outputs {
					outputTablesMap[out.Name] = true
				}

				m.activeTracepoints[name] = &TracepointInfo{
					Name:   name,
					ID:     uuid.Nil,
					Status: nil,
				}
			}
		case *plannerpb.CompileMutation_DeleteTracepoint:
			{
				deleteTracepointsReq.TracepointNames = append(deleteTracepointsReq.TracepointNames, mut.DeleteTracepoint.Name)
			}
		}
	}

	if len(registerTracepointsReq.Requests) > 0 {
		resp, err := m.mdtp.RegisterTracepoint(ctx, registerTracepointsReq)
		log.WithField("resp", resp.GoString()).
			WithField("err", err).
			Info("Register Tracepoint")
		if err != nil {
			return nil, ErrTracepointRegistrationFailed
		}
		if resp.Status != nil && resp.Status.ErrCode != statuspb.OK {
			return resp.Status, ErrTracepointRegistrationFailed
		}

		// Update the internal stat of the tracepoints.
		for _, tp := range resp.Tracepoints {
			log.
				WithField("status", tp.Status.GoString()).
				WithField("name", tp.TracepointName).
				Info("Trace point Install status")
			id := utils.UUIDFromProtoOrNil(tp.TracepointID)
			m.activeTracepoints[tp.TracepointName].ID = id
			m.activeTracepoints[tp.TracepointName].Status = tp.Status
		}
	}
	if len(deleteTracepointsReq.TracepointNames) > 0 {
		delResp, err := m.mdtp.RemoveTracepoint(ctx, deleteTracepointsReq)
		if err != nil {
			return nil, ErrTracepointDeletionFailed
		}
		if delResp.Status != nil && delResp.Status.ErrCode != statuspb.OK {
			return delResp.Status, ErrTracepointDeletionFailed
		}
		// Remove the tracepoints we considered deleted.
		for _, tpName := range deleteTracepointsReq.TracepointNames {
			if _, ok := m.activeTracepoints[tpName]; ok {
				delete(m.activeTracepoints, tpName)
			}
		}
	}
	m.outputTables = make([]string, 0)
	for k := range outputTablesMap {
		m.outputTables = append(m.outputTables, k)
	}

	return nil, nil
}

// MutationInfo returns the summarized mutation information.
func (m *MutationExecutor) MutationInfo(ctx context.Context) (*vizierpb.MutationInfo, error) {
	req := &metadatapb.GetTracepointInfoRequest{
		TracepointIDs: make([]*uuidpb.UUID, 0),
	}
	for _, tp := range m.activeTracepoints {
		req.TracepointIDs = append(req.TracepointIDs, utils.ProtoFromUUID(&tp.ID))
	}
	aCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", fmt.Sprintf("bearer %s", aCtx.AuthToken))

	resp, err := m.mdtp.GetTracepointInfo(ctx, req)
	if err != nil {
		return nil, err
	}
	mutationInfo := &vizierpb.MutationInfo{
		Status: &vizierpb.Status{Code: 0},
		States: make([]*vizierpb.MutationInfo_MutationState, len(resp.Tracepoints)),
	}

	ready := true
	for idx, tp := range resp.Tracepoints {
		mutationInfo.States[idx] = &vizierpb.MutationInfo_MutationState{
			ID:    utils.UUIDFromProtoOrNil(tp.TracepointID).String(),
			State: convertLifeCycleStateToVizierLifeCycleState(tp.State),
		}
		if tp.State != statuspb.RUNNING_STATE {
			ready = false
		}
	}

	if !ready {
		mutationInfo.Status = &vizierpb.Status{
			Code:    int32(codes.Unavailable),
			Message: "probe installation in progress",
		}
		return mutationInfo, nil
	}

	if !m.isSchemaReady() {
		mutationInfo.Status = &vizierpb.Status{
			Code:    int32(codes.Unavailable),
			Message: "Schema is not ready yet",
		}
	}
	return mutationInfo, nil
}

func (m *MutationExecutor) isSchemaReady() bool {
	info := m.tracker.GetAgentInfo()
	schemaNames := make(map[string]bool, 0)
	for _, s := range info.DistributedState().SchemaInfo {
		schemaNames[s.Name] = true
	}
	for _, s := range m.outputTables {
		if _, ok := schemaNames[s]; !ok {
			return false
		}
	}
	return true
}
