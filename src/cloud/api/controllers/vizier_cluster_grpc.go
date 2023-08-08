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

	"github.com/gofrs/uuid"
	"golang.org/x/net/idna"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/metadata"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	"px.dev/pixie/src/shared/artifacts/versionspb"
	"px.dev/pixie/src/shared/cvmsgspb"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/shared/services/authcontext"
	"px.dev/pixie/src/utils"
)

// VizierClusterInfo is the server that implements the VizierClusterInfo gRPC service.
type VizierClusterInfo struct {
	VzMgr                 vzmgrpb.VZMgrServiceClient
	ArtifactTrackerClient artifacttrackerpb.ArtifactTrackerClient
}

func contextWithAuthToken(ctx context.Context) (context.Context, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	return metadata.AppendToOutgoingContext(ctx, "authorization",
		fmt.Sprintf("bearer %s", sCtx.AuthToken)), nil
}

// CreateCluster creates a cluster for the current org.
func (v *VizierClusterInfo) CreateCluster(ctx context.Context, request *cloudpb.CreateClusterRequest) (*cloudpb.CreateClusterResponse, error) {
	return nil, status.Errorf(codes.Unimplemented, "Deprecated. Please use `px deploy`")
}

// GetClusterInfo returns information about Vizier clusters.
func (v *VizierClusterInfo) GetClusterInfo(ctx context.Context, request *cloudpb.GetClusterInfoRequest) (*cloudpb.GetClusterInfoResponse, error) {
	sCtx, err := authcontext.FromContext(ctx)
	if err != nil {
		return nil, err
	}
	orgIDstr := sCtx.Claims.GetUserClaims().OrgID
	orgID, err := uuid.FromString(orgIDstr)
	if err != nil {
		return nil, err
	}

	ctx, err = contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	vzIDs := make([]*uuidpb.UUID, 0)
	if request.ID != nil {
		vzIDs = append(vzIDs, request.ID)
	} else {
		viziers, err := v.VzMgr.GetViziersByOrg(ctx, utils.ProtoFromUUID(orgID))
		if err != nil {
			return nil, err
		}
		vzIDs = viziers.VizierIDs
	}

	return v.getClusterInfoForViziers(ctx, vzIDs)
}

func convertContainerState(cs metadatapb.ContainerState) cloudpb.ContainerState {
	switch cs {
	case metadatapb.CONTAINER_STATE_RUNNING:
		return cloudpb.CONTAINER_STATE_RUNNING
	case metadatapb.CONTAINER_STATE_TERMINATED:
		return cloudpb.CONTAINER_STATE_TERMINATED
	case metadatapb.CONTAINER_STATE_WAITING:
		return cloudpb.CONTAINER_STATE_WAITING
	case metadatapb.CONTAINER_STATE_UNKNOWN:
		return cloudpb.CONTAINER_STATE_UNKNOWN
	default:
		return cloudpb.CONTAINER_STATE_UNKNOWN
	}
}

func convertPodPhase(p metadatapb.PodPhase) cloudpb.PodPhase {
	switch p {
	case metadatapb.PENDING:
		return cloudpb.PENDING
	case metadatapb.RUNNING:
		return cloudpb.RUNNING
	case metadatapb.SUCCEEDED:
		return cloudpb.SUCCEEDED
	case metadatapb.FAILED:
		return cloudpb.FAILED
	case metadatapb.TERMINATED:
		return cloudpb.TERMINATED
	case metadatapb.PHASE_UNKNOWN:
		return cloudpb.PHASE_UNKNOWN
	default:
		return cloudpb.PHASE_UNKNOWN
	}
}

// Converts vzmgrpb proto format of PodStatus to cloudpb proto format.
func convertPodStatuses(vzMgrStatuses map[string]*cvmsgspb.PodStatus) map[string]*cloudpb.PodStatus {
	podStatuses := make(map[string]*cloudpb.PodStatus)
	for podName, status := range vzMgrStatuses {
		var containers []*cloudpb.ContainerStatus
		for _, container := range status.Containers {
			containers = append(containers, &cloudpb.ContainerStatus{
				Name:         container.Name,
				State:        convertContainerState(container.State),
				Message:      container.Message,
				Reason:       container.Reason,
				CreatedAt:    container.CreatedAt,
				RestartCount: container.RestartCount,
			})
		}
		var events []*cloudpb.K8SEvent
		for _, ev := range status.Events {
			events = append(events, &cloudpb.K8SEvent{
				Message:   ev.Message,
				LastTime:  ev.LastTime,
				FirstTime: ev.FirstTime,
			})
		}

		podStatuses[podName] = &cloudpb.PodStatus{
			Name:          status.Name,
			Status:        convertPodPhase(status.Status),
			StatusMessage: status.StatusMessage,
			Reason:        status.Reason,
			Containers:    containers,
			CreatedAt:     status.CreatedAt,
			Events:        events,
			RestartCount:  status.RestartCount,
		}
	}
	return podStatuses
}

func (v *VizierClusterInfo) getClusterInfoForViziers(ctx context.Context, ids []*uuidpb.UUID) (*cloudpb.GetClusterInfoResponse, error) {
	resp := &cloudpb.GetClusterInfoResponse{}

	cNames := make(map[string]int)
	vzInfoResp, err := v.VzMgr.GetVizierInfos(ctx, &vzmgrpb.GetVizierInfosRequest{
		VizierIDs: ids,
	})

	if err != nil {
		return nil, err
	}

	for _, vzInfo := range vzInfoResp.VizierInfos {
		if vzInfo == nil || vzInfo.VizierID == nil {
			continue
		}

		s := vzStatusToClusterStatus(vzInfo.Status)
		prevS := vzStatusToClusterStatus(vzInfo.PreviousStatus)
		prettyName := PrettifyClusterName(vzInfo.ClusterName, false)
		// Convert to punycode (if non-latin) to help detect homoglyphs.
		prettyName, err = idna.Punycode.ToASCII(prettyName)
		if err != nil {
			return nil, err
		}

		if val, ok := cNames[prettyName]; ok {
			cNames[prettyName] = val + 1
		} else {
			cNames[prettyName] = 1
		}

		resp.Clusters = append(resp.Clusters, &cloudpb.ClusterInfo{
			ID:              vzInfo.VizierID,
			Status:          s,
			StatusMessage:   vzInfo.StatusMessage,
			LastHeartbeatNs: vzInfo.LastHeartbeatNs,
			Config: &cloudpb.VizierConfig{
				PassthroughEnabled: true,
			},
			ClusterUID:                    vzInfo.ClusterUID,
			ClusterName:                   vzInfo.ClusterName,
			PrettyClusterName:             prettyName,
			ClusterVersion:                vzInfo.ClusterVersion,
			VizierVersion:                 vzInfo.VizierVersion,
			OperatorVersion:               vzInfo.OperatorVersion,
			ControlPlanePodStatuses:       convertPodStatuses(vzInfo.ControlPlanePodStatuses),
			UnhealthyDataPlanePodStatuses: convertPodStatuses(vzInfo.UnhealthyDataPlanePodStatuses),
			NumNodes:                      vzInfo.NumNodes,
			NumInstrumentedNodes:          vzInfo.NumInstrumentedNodes,
			PreviousStatus:                prevS,
			PreviousStatusTime:            vzInfo.PreviousStatusTime,
		})
	}

	// For duplicate prettyNames, update the prettyNames to have more context.
	for i, c := range resp.Clusters {
		if cNames[c.PrettyClusterName] > 1 {
			resp.Clusters[i].PrettyClusterName = PrettifyClusterName(c.ClusterName, true)
		}
	}

	return resp, nil
}

// GetClusterConnectionInfo returns information about connections to Vizier cluster.
func (v *VizierClusterInfo) GetClusterConnectionInfo(ctx context.Context, request *cloudpb.GetClusterConnectionInfoRequest) (*cloudpb.GetClusterConnectionInfoResponse, error) {
	id := request.ID
	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	ci, err := v.VzMgr.GetVizierConnectionInfo(ctx, id)
	if err != nil {
		return nil, err
	}

	return &cloudpb.GetClusterConnectionInfoResponse{
		Token: ci.Token,
	}, nil
}

// UpdateClusterVizierConfig supports updates of VizierConfig for a cluster
func (v *VizierClusterInfo) UpdateClusterVizierConfig(ctx context.Context, req *cloudpb.UpdateClusterVizierConfigRequest) (*cloudpb.UpdateClusterVizierConfigResponse, error) {
	return &cloudpb.UpdateClusterVizierConfigResponse{}, nil
}

// UpdateOrInstallCluster updates or installs the given vizier cluster to the specified version.
func (v *VizierClusterInfo) UpdateOrInstallCluster(ctx context.Context, req *cloudpb.UpdateOrInstallClusterRequest) (*cloudpb.UpdateOrInstallClusterResponse, error) {
	if req.Version == "" {
		return nil, status.Errorf(codes.InvalidArgument, "version cannot be empty")
	}

	ctx, err := contextWithAuthToken(ctx)
	if err != nil {
		return nil, err
	}

	// Validate version.
	atReq := &artifacttrackerpb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   req.Version,
		ArtifactType: versionspb.AT_CONTAINER_SET_YAMLS,
	}

	_, err = v.ArtifactTrackerClient.GetDownloadLink(ctx, atReq)
	if err != nil {
		return nil, status.Errorf(codes.InvalidArgument, "invalid version")
	}

	resp, err := v.VzMgr.UpdateOrInstallVizier(ctx, &cvmsgspb.UpdateOrInstallVizierRequest{
		VizierID:     req.ClusterID,
		Version:      req.Version,
		RedeployEtcd: req.RedeployEtcd,
	})
	if err != nil {
		return nil, err
	}

	return &cloudpb.UpdateOrInstallClusterResponse{
		UpdateStarted: resp.UpdateStarted,
	}, nil
}

func vzStatusToClusterStatus(s cvmsgspb.VizierStatus) cloudpb.ClusterStatus {
	switch s {
	case cvmsgspb.VZ_ST_HEALTHY:
		return cloudpb.CS_HEALTHY
	case cvmsgspb.VZ_ST_UNHEALTHY:
		return cloudpb.CS_UNHEALTHY
	case cvmsgspb.VZ_ST_DISCONNECTED:
		return cloudpb.CS_DISCONNECTED
	case cvmsgspb.VZ_ST_UPDATING:
		return cloudpb.CS_UPDATING
	case cvmsgspb.VZ_ST_CONNECTED:
		return cloudpb.CS_CONNECTED
	case cvmsgspb.VZ_ST_UPDATE_FAILED:
		return cloudpb.CS_UPDATE_FAILED
	case cvmsgspb.VZ_ST_DEGRADED:
		return cloudpb.CS_DEGRADED
	default:
		return cloudpb.CS_UNKNOWN
	}
}
