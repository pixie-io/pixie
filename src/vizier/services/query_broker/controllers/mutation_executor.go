package controllers

import (
	"context"
	"fmt"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	uuidpb "pixielabs.ai/pixielabs/src/api/public/uuidpb"
	public_vizierapipb "pixielabs.ai/pixielabs/src/api/public/vizierapipb"
	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/carnot/planner/plannerpb"
	"pixielabs.ai/pixielabs/src/carnot/planpb"
	statuspb "pixielabs.ai/pixielabs/src/common/base/proto"
	"pixielabs.ai/pixielabs/src/shared/services/authcontext"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
)

// TracepointMap stores a map from the name to tracepoint info.
type TracepointMap map[string]*TracepointInfo

// MutationExecutor is responsible for running script mutations.
type MutationExecutor struct {
	planner           Planner
	mdtp              metadatapb.MetadataTracepointServiceClient
	mdconf            metadatapb.MetadataConfigServiceClient
	activeTracepoints TracepointMap
	outputTables      []string
	distributedState  *distributedpb.DistributedState
}

// TracepointInfo stores information of a particular tracepoint.
type TracepointInfo struct {
	Name   string
	ID     uuid.UUID
	Status *statuspb.Status
}

// NewMutationExecutor creates a new mutation executor.
func NewMutationExecutor(
	planner Planner,
	mdtp metadatapb.MetadataTracepointServiceClient,
	mdconf metadatapb.MetadataConfigServiceClient,
	distributedState *distributedpb.DistributedState) *MutationExecutor {
	return &MutationExecutor{
		planner:           planner,
		mdtp:              mdtp,
		mdconf:            mdconf,
		distributedState:  distributedState,
		activeTracepoints: make(TracepointMap, 0),
	}
}

// Execute runs the mutation. On unknown errors it will return an error, otherwise we return a status message
// that has more context about the error message.
func (m *MutationExecutor) Execute(ctx context.Context, req *public_vizierapipb.ExecuteScriptRequest, planOpts *planpb.PlanOptions) (*statuspb.Status, error) {
	convertedReq, err := VizierQueryRequestToPlannerMutationRequest(req)
	if err != nil {
		return nil, err
	}
	plannerState := &distributedpb.LogicalPlannerState{
		DistributedState: m.distributedState,
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
		Names: make([]string, 0),
	}
	configmapReqs := make([]*metadatapb.UpdateConfigRequest, 0)

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
						TracepointDeployment: mut.Trace,
						Name:                 mut.Trace.Name,
						TTL:                  mut.Trace.TTL,
					})

				if _, ok := m.activeTracepoints[name]; ok {
					return nil, fmt.Errorf("tracepoint with name '%s', already used", name)
				}
				for _, tracepoint := range mut.Trace.Tracepoints {
					outputTablesMap[tracepoint.TableName] = true
				}

				m.activeTracepoints[name] = &TracepointInfo{
					Name:   name,
					ID:     uuid.Nil,
					Status: nil,
				}
			}
		case *plannerpb.CompileMutation_DeleteTracepoint:
			{
				deleteTracepointsReq.Names = append(deleteTracepointsReq.Names, mut.DeleteTracepoint.Name)
			}
		case *plannerpb.CompileMutation_ConfigUpdate:
			{
				configmapReqs = append(configmapReqs, &metadatapb.UpdateConfigRequest{
					Key:          mut.ConfigUpdate.Key,
					Value:        mut.ConfigUpdate.Value,
					AgentPodName: mut.ConfigUpdate.AgentPodName,
				})
			}
		}
	}

	if len(registerTracepointsReq.Requests) > 0 {
		resp, err := m.mdtp.RegisterTracepoint(ctx, registerTracepointsReq)
		if err != nil {
			log.WithError(err).
				Errorf("Failed to register tracepoints")
			return nil, ErrTracepointRegistrationFailed
		}
		if resp.Status != nil && resp.Status.ErrCode != statuspb.OK {
			log.WithField("status", resp.Status.String()).
				Errorf("Failed to register tracepoints with bad status")

			return resp.Status, ErrTracepointRegistrationFailed
		}

		// Update the internal stat of the tracepoints.
		for _, tp := range resp.Tracepoints {
			id := utils.UUIDFromProtoOrNil(tp.ID)
			m.activeTracepoints[tp.Name].ID = id
			m.activeTracepoints[tp.Name].Status = tp.Status
		}
	}
	if len(deleteTracepointsReq.Names) > 0 {
		delResp, err := m.mdtp.RemoveTracepoint(ctx, deleteTracepointsReq)
		if err != nil {
			log.WithError(err).
				Errorf("Failed to delete tracepoints")
			return nil, ErrTracepointDeletionFailed
		}
		if delResp.Status != nil && delResp.Status.ErrCode != statuspb.OK {
			log.WithField("status", delResp.Status.String()).
				Errorf("Failed to delete tracepoints with bad status")
			return delResp.Status, ErrTracepointDeletionFailed
		}
		// Remove the tracepoints we considered deleted.
		for _, tpName := range deleteTracepointsReq.Names {
			if _, ok := m.activeTracepoints[tpName]; ok {
				delete(m.activeTracepoints, tpName)
			}
		}
	}

	if len(configmapReqs) > 0 {
		for _, configmapReq := range configmapReqs {
			resp, err := m.mdconf.UpdateConfig(ctx, configmapReq)
			if err != nil || (resp.Status != nil && resp.Status.ErrCode != statuspb.OK) {
				return nil, ErrConfigUpdateFailed
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
func (m *MutationExecutor) MutationInfo(ctx context.Context) (*public_vizierapipb.MutationInfo, error) {
	req := &metadatapb.GetTracepointInfoRequest{
		IDs: make([]*uuidpb.UUID, 0),
	}
	for _, tp := range m.activeTracepoints {
		req.IDs = append(req.IDs, utils.ProtoFromUUID(tp.ID))
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
	mutationInfo := &public_vizierapipb.MutationInfo{
		Status: &public_vizierapipb.Status{Code: 0},
		States: make([]*public_vizierapipb.MutationInfo_MutationState, len(resp.Tracepoints)),
	}

	ready := true
	for idx, tp := range resp.Tracepoints {
		mutationInfo.States[idx] = &public_vizierapipb.MutationInfo_MutationState{
			ID:    utils.UUIDFromProtoOrNil(tp.ID).String(),
			State: convertLifeCycleStateToVizierLifeCycleState(tp.State),
			Name:  tp.Name,
		}
		if tp.State != statuspb.RUNNING_STATE {
			ready = false
		}
	}

	if !ready {
		mutationInfo.Status = &public_vizierapipb.Status{
			Code:    int32(codes.Unavailable),
			Message: "probe installation in progress",
		}
		return mutationInfo, nil
	}

	if !m.isSchemaReady() {
		mutationInfo.Status = &public_vizierapipb.Status{
			Code:    int32(codes.Unavailable),
			Message: "Schema is not ready yet",
		}
	}
	return mutationInfo, nil
}

func (m *MutationExecutor) isSchemaReady() bool {
	schemaNames := make(map[string]bool, 0)
	for _, s := range m.distributedState.SchemaInfo {
		schemaNames[s.Name] = true
	}
	for _, s := range m.outputTables {
		if _, ok := schemaNames[s]; !ok {
			return false
		}
	}
	return true
}
